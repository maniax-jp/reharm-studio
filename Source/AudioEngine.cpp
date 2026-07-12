#include "AudioEngine.h"

AudioEngine::AudioEngine()
{
    std::atomic_store(&mCurrentChordData, std::make_shared<ChordData>());
}

void AudioEngine::beginPluginLoad()
{
    std::atomic_store(&mLoadingPlugin, true);
}

void AudioEngine::endPluginLoad()
{
    std::atomic_store(&mLoadingPlugin, false);
}

AudioEngine::~AudioEngine()
{
    stopPlayback();
}

void AudioEngine::prepareToPlay(int samplesPerBlockExpected, double sampleRate)
{
    juce::ScopedLock sl (pluginLock);
    mSampleRate = sampleRate;
    mPlayHead.sampleRate = sampleRate;

    if (mPluginInstance != nullptr)
    {
        mPluginInstance->prepareToPlay(sampleRate, samplesPerBlockExpected);
    }

    const int maxCh = juce::jmax (2, mPluginInstance != nullptr
                                         ? juce::jmax (mPluginInstance->getTotalNumInputChannels(),
                                                       mPluginInstance->getTotalNumOutputChannels())
                                         : 2);
    mTempBuffer.setSize (maxCh, samplesPerBlockExpected);

    if (mPluginInstance != nullptr)
        std::atomic_store(&mPluginReady, true);
}

void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // If loading a plugin, only clear the buffer and return immediately.
    if (std::atomic_load(&mLoadingPlugin))
    {
        buffer.clear();
        buffer.applyGain(mVolume.load());
        return;
    }

    buffer.clear();

    // Capture a local reference to the current chord data
    auto chordData = std::atomic_load(&mCurrentChordData);

    // Guard: chord data may have been swapped for a shorter progression while index was large.
    if (mCurrentChordIndex >= chordData->totalChords)
        mCurrentChordIndex = 0;

    // Block-start beat position for the playhead (capture before advancing).
    const double blockStartPpq = mCurrentBeatPosition;

    if (mIsPlaying.load())
    {
        int numSamples = buffer.getNumSamples();
        int currentBpm = mBpm.load();
        double beatsPerSample = currentBpm / (60.0 * mSampleRate);
        double blockBeats = numSamples * beatsPerSample;

        double startBeat = mCurrentBeatPosition;
        mCurrentBeatPosition += blockBeats;
        double endBeat = mCurrentBeatPosition;

        double totalBeats = chordData->totalBeats;
        if (totalBeats > 0)
        {
            // Build cumulative beat positions for variable-length slots.
            // cumBeats[i] = start beat of slot i, cumBeats[n] = totalBeats.
            std::vector<double> cumBeats;
            cumBeats.reserve(static_cast<size_t>(chordData->totalChords) + 1);
            cumBeats.push_back(0.0);
            for (int i = 0; i < chordData->totalChords; ++i)
                cumBeats.push_back(cumBeats.back() + chordData->beats[i]);

            // 1. Handle the very first Note On (only on the initial block after startPlayback).
            if (mFirstBlock && startBeat == 0.0 && mCurrentChordIndex < chordData->totalChords)
            {
                if (!chordData->notes[mCurrentChordIndex].empty())
                {
                    for (int note : chordData->notes[mCurrentChordIndex])
                        midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<uint8>(64)), 0);
                }
                mPlayingSlotShared.store(mCurrentChordIndex, std::memory_order_relaxed);
                mFirstBlock = false;
            }

            // 2. Handle transitions (including loop).
            while (true)
            {
                // Next transition is at the end of the current slot.
                double nextTransitionBeat = cumBeats[mCurrentChordIndex + 1];

                if (nextTransitionBeat > endBeat)
                    break;

                if (nextTransitionBeat >= startBeat)
                {
                    int relativeOffset = static_cast<int>((nextTransitionBeat - startBeat) / beatsPerSample);

                    // Send NoteOff for current slot.
                    for (int note : chordData->notes[mCurrentChordIndex])
                        midiMessages.addEvent(juce::MidiMessage::noteOff(1, note), relativeOffset);

                    // Advance to next slot.
                    mCurrentChordIndex++;
                    if (mCurrentChordIndex >= chordData->totalChords)
                    {
                        mCurrentChordIndex = 0;
                        // Normalize start/end for the next loop iteration.
                        startBeat -= totalBeats;
                        endBeat -= totalBeats;
                    }

                    // Send NoteOn for the new slot if not empty.
                    if (!chordData->notes[mCurrentChordIndex].empty())
                    {
                        for (int note : chordData->notes[mCurrentChordIndex])
                            midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<uint8>(64)), relativeOffset);
                    }
                    mPlayingSlotShared.store(mCurrentChordIndex, std::memory_order_relaxed);
                }
                else
                {
                    // Transition occurred before this block; just advance index.
                    mCurrentChordIndex++;
                    if (mCurrentChordIndex >= chordData->totalChords)
                    {
                        mCurrentChordIndex = 0;
                        startBeat -= totalBeats;
                        endBeat -= totalBeats;
                    }
                    mPlayingSlotShared.store(mCurrentChordIndex, std::memory_order_relaxed);
                }
            }

            // Wrap currentBeatPosition for the next block.
            while (mCurrentBeatPosition >= totalBeats)
                mCurrentBeatPosition -= totalBeats;
        }
    }
    else if (mStopRequested.load())
    {
        if (mCurrentChordIndex < chordData->totalChords)
        {
            for (int note : chordData->notes[mCurrentChordIndex])
                midiMessages.addEvent(juce::MidiMessage::noteOff(1, note), 0);
        }
        std::atomic_store(&mStopRequested, false);
        mCurrentChordIndex = 0;
        mCurrentBeatPosition = 0.0;
        mPlayingSlotShared.store(-1, std::memory_order_relaxed);
    }

    // Keep host playhead in sync before plugin processBlock (plugins read tempo/position here).
    mPlayHead.sampleRate = mSampleRate;
    mPlayHead.bpm = (double) mBpm.load();
    mPlayHead.playing = mIsPlaying.load();
    mPlayHead.ppq = blockStartPpq;

    {
        juce::ScopedLock sl (pluginLock);
        if (mPluginInstance != nullptr && std::atomic_load(&mPluginReady))
        {
            int numDeviceCh = buffer.getNumChannels();
            int numPluginOut = mPluginInstance->getTotalNumOutputChannels();
            int numCh = juce::jmax (numDeviceCh, numPluginOut);
            mTempBuffer.setSize (numCh, buffer.getNumSamples(), false, false, true);
            mTempBuffer.clear();

            try
            {
                mPluginInstance->processBlock (mTempBuffer, midiMessages);
                for (int ch = 0; ch < numDeviceCh; ++ch)
                    buffer.copyFrom (ch, 0, mTempBuffer, ch, 0, buffer.getNumSamples());
            }
            catch (...)
            {
                juce::Logger::writeToLog ("Exception caught during plugin processBlock");
            }
        }
    }

    mPlayHead.samples += buffer.getNumSamples();
    buffer.applyGain(mVolume.load());
}

void AudioEngine::releaseResources()
{
    juce::ScopedLock sl (pluginLock);
    if (mPluginInstance != nullptr)
        mPluginInstance->releaseResources();
}

void AudioEngine::setPluginInstance(std::unique_ptr<juce::AudioPluginInstance> plugin)
{
    juce::ScopedLock sl (pluginLock);
    std::atomic_store(&mPluginReady, false);
    if (mPluginInstance != nullptr)
        mPluginInstance->releaseResources();
    mPluginInstance = std::move(plugin);
    if (mPluginInstance != nullptr)
        mPluginInstance->setPlayHead (&mPlayHead);
}

void AudioEngine::loadPluginInstance(std::unique_ptr<juce::AudioPluginInstance> plugin, double sampleRate, int blockSize)
{
    {
        juce::ScopedLock sl (pluginLock);
        std::atomic_store(&mPluginReady, false);
        if (mPluginInstance != nullptr)
            mPluginInstance->releaseResources();
        mPluginInstance = std::move(plugin);
        if (mPluginInstance != nullptr)
            mPluginInstance->setPlayHead (&mPlayHead);
    }

    if (mPluginInstance != nullptr)
    {
        mPluginInstance->prepareToPlay (sampleRate, blockSize);

        // Size scratch buffer while mPluginReady is still false.
        const int maxCh = juce::jmax (2, juce::jmax (mPluginInstance->getTotalNumInputChannels(),
                                                      mPluginInstance->getTotalNumOutputChannels()));
        mTempBuffer.setSize (maxCh, blockSize);
    }

    juce::ScopedLock sl (pluginLock);
    std::atomic_store(&mPluginReady, true);
}

void AudioEngine::loadPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin, double sampleRate, int blockSize)
{
    {
        juce::ScopedLock sl (pluginLock);
        std::atomic_store(&mPluginReady, false);
        if (mPluginInstance != nullptr)
            mPluginInstance->releaseResources();
        mPluginInstance = std::move(plugin);
        if (mPluginInstance != nullptr)
            mPluginInstance->setPlayHead (&mPlayHead);
    }

    if (mPluginInstance != nullptr)
    {
        mPluginInstance->prepareToPlay (sampleRate, blockSize);

        const int maxCh = juce::jmax (2, juce::jmax (mPluginInstance->getTotalNumInputChannels(),
                                                      mPluginInstance->getTotalNumOutputChannels()));
        mTempBuffer.setSize (maxCh, blockSize);
    }

    juce::ScopedLock sl (pluginLock);
    std::atomic_store(&mPluginReady, true);
}

void AudioEngine::setBpm(int newBpm)
{
    mBpm.store(newBpm);
}

void AudioEngine::setVolume(float newVolume)
{
    mVolume.store(newVolume);
}

void AudioEngine::setChordData(std::shared_ptr<ChordData> newData)
{
    std::atomic_store(&mCurrentChordData, newData);
}

void AudioEngine::startPlayback()
{
    mCurrentChordIndex = 0;
    mCurrentBeatPosition = 0.0;
    mPlayHead.samples = 0;
    mPlayHead.ppq = 0.0;
    mFirstBlock = true;
    mPlayingSlotShared.store(0, std::memory_order_relaxed);
    std::atomic_store(&mIsPlaying, true);
}

void AudioEngine::stopPlayback()
{
    std::atomic_store(&mStopRequested, true);
    std::atomic_store(&mIsPlaying, false);
    mFirstBlock = false;
}
