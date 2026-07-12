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
            std::vector<double> cumBeats;
            cumBeats.reserve(static_cast<size_t>(chordData->totalChords) + 1);
            cumBeats.push_back(0.0);
            for (int i = 0; i < chordData->totalChords; ++i)
                cumBeats.push_back(cumBeats.back() + chordData->beats[i]);

            // Detect chord data swap: send note-offs for old sounding notes.
            if (chordData.get() != mActiveChordDataPtr && mNumSoundingNotes > 0)
            {
                for (int i = 0; i < mNumSoundingNotes; ++i)
                    midiMessages.addEvent(juce::MidiMessage::noteOff(1, mSoundingNotes[i]), 0);
                mNumSoundingNotes = 0;

                // Normalize beat position to new data range.
                mCurrentBeatPosition = fmod(mCurrentBeatPosition, totalBeats);
                startBeat = mCurrentBeatPosition - blockBeats;
                if (startBeat < 0) startBeat = 0;
                endBeat = mCurrentBeatPosition;

                // Find current slot from cumBeats.
                mCurrentChordIndex = 0;
                for (int i = 0; i < chordData->totalChords; ++i)
                {
                    if (mCurrentBeatPosition < cumBeats[i + 1])
                    {
                        mCurrentChordIndex = i;
                        break;
                    }
                    mCurrentChordIndex = i;
                }

                // Send note-on for the current slot of new data.
                if (!chordData->notes[mCurrentChordIndex].empty())
                {
                    for (int n = 0; n < static_cast<int>(chordData->notes[mCurrentChordIndex].size())
                         && mNumSoundingNotes < kMaxSoundingNotes; ++n)
                    {
                        const int note = chordData->notes[mCurrentChordIndex][n];
                        bool dup = false;
                        for (int d = 0; d < mNumSoundingNotes; ++d)
                            if (mSoundingNotes[d] == note) { dup = true; break; }
                        if (!dup)
                        {
                            mSoundingNotes[mNumSoundingNotes++] = note;
                            midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<uint8>(64)), 0);
                        }
                    }
                }
                mPlayingSlotShared.store(mCurrentChordIndex, std::memory_order_relaxed);
                mActiveChordDataPtr = chordData.get();
                mFirstBlock = false;
            }

            // Guard: index out of range after swap or other edge case.
            if (mCurrentChordIndex >= chordData->totalChords)
            {
                if (mNumSoundingNotes > 0)
                {
                    for (int i = 0; i < mNumSoundingNotes; ++i)
                        midiMessages.addEvent(juce::MidiMessage::noteOff(1, mSoundingNotes[i]), 0);
                    mNumSoundingNotes = 0;
                }
                mCurrentChordIndex = 0;
                mCurrentBeatPosition = 0.0;
                mFirstBlock = true;
            }

            // Helper: send note-on for a slot and track in mSoundingNotes.
            auto sendSlotNoteOn = [&]()
            {
                if (mCurrentChordIndex < chordData->totalChords
                    && !chordData->notes[mCurrentChordIndex].empty())
                {
                    for (int n = 0; n < static_cast<int>(chordData->notes[mCurrentChordIndex].size())
                         && mNumSoundingNotes < kMaxSoundingNotes; ++n)
                    {
                        const int note = chordData->notes[mCurrentChordIndex][n];
                        bool dup = false;
                        for (int d = 0; d < mNumSoundingNotes; ++d)
                            if (mSoundingNotes[d] == note) { dup = true; break; }
                        if (!dup)
                            mSoundingNotes[mNumSoundingNotes++] = note;
                    }
                }
                mPlayingSlotShared.store(mCurrentChordIndex, std::memory_order_relaxed);
            };

            // 1. Handle the very first Note On.
            if (mFirstBlock && mCurrentChordIndex < chordData->totalChords)
            {
                if (!chordData->notes[mCurrentChordIndex].empty())
                {
                    for (int n = 0; n < static_cast<int>(chordData->notes[mCurrentChordIndex].size())
                         && mNumSoundingNotes < kMaxSoundingNotes; ++n)
                    {
                        const int note = chordData->notes[mCurrentChordIndex][n];
                        bool dup = false;
                        for (int d = 0; d < mNumSoundingNotes; ++d)
                            if (mSoundingNotes[d] == note) { dup = true; break; }
                        if (!dup)
                        {
                            mSoundingNotes[mNumSoundingNotes++] = note;
                            midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<uint8>(64)), 0);
                        }
                    }
                }
                mPlayingSlotShared.store(mCurrentChordIndex, std::memory_order_relaxed);
                mFirstBlock = false;
                if (mActiveChordDataPtr == nullptr)
                    mActiveChordDataPtr = chordData.get();
            }

            // 2. Handle transitions (including loop).
            while (true)
            {
                double nextTransitionBeat = cumBeats[mCurrentChordIndex + 1];

                if (nextTransitionBeat > endBeat)
                    break;

                if (nextTransitionBeat >= startBeat)
                {
                    int relativeOffset = static_cast<int>((nextTransitionBeat - startBeat) / beatsPerSample);

                    // Send NoteOff for all currently sounding notes.
                    for (int i = 0; i < mNumSoundingNotes; ++i)
                        midiMessages.addEvent(juce::MidiMessage::noteOff(1, mSoundingNotes[i]), relativeOffset);
                    mNumSoundingNotes = 0;

                    // Advance to next slot.
                    mCurrentChordIndex++;
                    if (mCurrentChordIndex >= chordData->totalChords)
                    {
                        mCurrentChordIndex = 0;
                        startBeat -= totalBeats;
                        endBeat -= totalBeats;
                    }

                    // Send NoteOn for the new slot if not empty.
                    if (!chordData->notes[mCurrentChordIndex].empty())
                    {
                        for (int n = 0; n < static_cast<int>(chordData->notes[mCurrentChordIndex].size())
                             && mNumSoundingNotes < kMaxSoundingNotes; ++n)
                        {
                            const int note = chordData->notes[mCurrentChordIndex][n];
                            bool dup = false;
                            for (int d = 0; d < mNumSoundingNotes; ++d)
                                if (mSoundingNotes[d] == note) { dup = true; break; }
                            if (!dup)
                            {
                                mSoundingNotes[mNumSoundingNotes++] = note;
                                midiMessages.addEvent(juce::MidiMessage::noteOn(1, note, static_cast<uint8>(64)), relativeOffset);
                            }
                        }
                    }
                    mPlayingSlotShared.store(mCurrentChordIndex, std::memory_order_relaxed);
                }
                else
                {
                    // Transition before this block; advance index without MIDI events.
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
        // Send note-off for all currently sounding notes.
        for (int i = 0; i < mNumSoundingNotes; ++i)
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, mSoundingNotes[i]), 0);
        mNumSoundingNotes = 0;

        // Safety: send all-notes-off as a fallback.
        midiMessages.addEvent(juce::MidiMessage::allNotesOff(1), 0);

        std::atomic_store(&mStopRequested, false);
        mCurrentChordIndex = 0;
        mCurrentBeatPosition = 0.0;
        mPlayingSlotShared.store(-1, std::memory_order_relaxed);
        mActiveChordDataPtr = nullptr;
    }

    // Keep host playhead in sync before plugin processBlock.
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
    mNumSoundingNotes = 0;
    mActiveChordDataPtr = std::atomic_load(&mCurrentChordData).get();
    std::atomic_store(&mIsPlaying, true);
}

void AudioEngine::stopPlayback()
{
    std::atomic_store(&mStopRequested, true);
    std::atomic_store(&mIsPlaying, false);
    mFirstBlock = false;
}
