#include "AudioEngine.h"

AudioEngine::AudioEngine()
{
    std::atomic_store(&mCurrentChordData, std::make_shared<ChordData>());
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
    }
}

void AudioEngine::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // Capture a local reference to the current chord data to ensure it stays valid during the block
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
        mStopRequested = false;
        mCurrentChordIndex = 0;
        mCurrentBeatPosition = 0.0;
    }

    {
        juce::ScopedLock sl (pluginLock);
        if (mPluginInstance != nullptr)
            mPluginInstance->processBlock(buffer, midiMessages);
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
    if (mPluginInstance != nullptr)
        mPluginInstance->releaseResources();
    mPluginInstance = std::move(plugin);
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
    mIsPlaying = true;
}

void AudioEngine::stopPlayback()
{
    mStopRequested = true;
    mIsPlaying = false;
}
