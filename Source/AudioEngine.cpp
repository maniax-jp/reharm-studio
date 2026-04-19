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

    if (mPluginInstance != nullptr)
    {
        mPluginInstance->prepareToPlay(sampleRate, samplesPerBlockExpected);
        std::atomic_store(&mPluginReady, true);
    }
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

    if (mIsPlaying.load())
    {
        int numSamples = buffer.getNumSamples();
        int currentBpm = mBpm.load();
        double beatsPerSample = currentBpm / (60.0 * mSampleRate);
        double blockBeats = numSamples * beatsPerSample;

        double startBeat = mCurrentBeatPosition;
        mCurrentBeatPosition += blockBeats;
        double endBeat = mCurrentBeatPosition;

        double totalBeats = (double)chordData->totalChords * 4.0;
        if (totalBeats <= 0) return;

        // 1. Handle the very first Note On
        if (startBeat == 0.0 && mCurrentChordIndex < chordData->totalChords)
        {
            for (int note : chordData->notes[0])
                midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<uint8>(64)), 0);
        }

        // 2. Handle transitions (including loop)
        while (true)
        {
            double nextTransitionBeat = (mCurrentChordIndex + 1) * 4.0;
            if (mCurrentChordIndex == chordData->totalChords - 1)
                nextTransitionBeat = totalBeats;

            if (nextTransitionBeat > endBeat)
                break;

            if (nextTransitionBeat >= startBeat)
            {
                int relativeOffset = static_cast<int>((nextTransitionBeat - startBeat) / beatsPerSample);

                for (int note : chordData->notes[mCurrentChordIndex])
                    midiMessages.addEvent(juce::MidiMessage::noteOff(1, note), relativeOffset);

                mCurrentChordIndex++;
                if (mCurrentChordIndex >= chordData->totalChords)
                    mCurrentChordIndex = 0;

                for (int note : chordData->notes[mCurrentChordIndex])
                    midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<uint8>(64)), relativeOffset);

                if (nextTransitionBeat == totalBeats)
                {
                    startBeat -= totalBeats;
                    endBeat -= totalBeats;
                }
            }
            else
            {
                mCurrentChordIndex++;
                if (mCurrentChordIndex >= chordData->totalChords)
                    mCurrentChordIndex = 0;
            }
        }

        // Wrap currentBeatPosition for the next block
        while (mCurrentBeatPosition >= totalBeats)
            mCurrentBeatPosition -= totalBeats;
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
    }

    {
        juce::ScopedLock sl (pluginLock);
        if (mPluginInstance != nullptr && std::atomic_load(&mPluginReady))
        {
            int numDeviceCh = buffer.getNumChannels();
            int numPluginOut = mPluginInstance->getTotalNumOutputChannels();
            int numCh = jmax (numDeviceCh, numPluginOut);
            juce::AudioBuffer<float> tempBuffer (numCh, buffer.getNumSamples());
            tempBuffer.clear();

            try
            {
                mPluginInstance->processBlock (tempBuffer, midiMessages);
                for (int ch = 0; ch < numDeviceCh; ++ch)
                    buffer.copyFrom (ch, 0, tempBuffer, ch, 0, buffer.getNumSamples());
            }
            catch (...)
            {
                juce::Logger::writeToLog ("Exception caught during plugin processBlock");
            }
        }
    }
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
}

void AudioEngine::loadPluginInstance(std::unique_ptr<juce::AudioPluginInstance> plugin, double sampleRate, int blockSize)
{
    {
        juce::ScopedLock sl (pluginLock);
        std::atomic_store(&mPluginReady, false);
        if (mPluginInstance != nullptr)
            mPluginInstance->releaseResources();
        mPluginInstance = std::move(plugin);
    }

    mPluginInstance->prepareToPlay (sampleRate, blockSize);

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
    }

    mPluginInstance->prepareToPlay (sampleRate, blockSize);

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
    std::atomic_store(&mIsPlaying, true);
}

void AudioEngine::stopPlayback()
{
    std::atomic_store(&mStopRequested, true);
    std::atomic_store(&mIsPlaying, false);
}
