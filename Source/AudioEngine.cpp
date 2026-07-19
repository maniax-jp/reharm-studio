#include "AudioEngine.h"
#include <array>

AudioEngine::AudioEngine()
{
    std::atomic_store(&mCurrentChordData, std::make_shared<ChordData>());

    // Register 32 voices and one sound for the fallback triangle synthesiser.
    for (int i = 0; i < 32; ++i)
        mFallbackSynth.addVoice (new TriangleVoice());
    mFallbackSynth.addSound (new TriangleSound());
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
    mFallbackSynth.setCurrentPlaybackSampleRate (sampleRate);

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
    // A4: サンプルレート0以下の場合は早期リターン
    if (mSampleRate <= 0.0)
    {
        buffer.clear();
        return;
    }

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

    // Handle a (re)start request on the audio thread so playback-state fields
    // are only ever written here. Sends note-offs for anything still sounding
    // from a previous run before resetting.
    if (mIsPlaying.load() && mStartRequested.exchange(false))
    {
        for (int i = 0; i < mNumSoundingNotes; ++i)
            midiMessages.addEvent(juce::MidiMessage::noteOff(1, mSoundingNotes[i]), 0);
        mNumSoundingNotes = 0;
        std::atomic_store(&mStopRequested, false);
        mCurrentChordIndex = 0;
        mCurrentBeatPosition = 0.0;
        mFirstBlock = true;
        mActiveChordDataPtr = chordData.get();
        mPlayHead.samples = 0;
        mPlayHead.ppq = 0.0;
    }

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
            // A1: 累積ビート配列はChordData内で事前計算済み。参照のみ。
            // F2: cumBeatsの整合性を確認し、スタックフォールバックの境界をチェック
            std::array<double, 65> stackCumBeats;
            const bool hasValidCumBeats = (static_cast<int>(chordData->cumBeats.size()) == chordData->totalChords + 1);
            bool useStackFallback = false;

            if (!hasValidCumBeats)
            {
                // スタックフォールバック: totalChords<=64 かつ beatsが十分にある場合のみ
                useStackFallback = (chordData->totalChords <= 64
                                    && static_cast<int>(chordData->beats.size()) >= chordData->totalChords);
                if (useStackFallback)
                {
                    stackCumBeats[0] = 0.0;
                    for (int i = 0; i < chordData->totalChords; ++i)
                        stackCumBeats[i + 1] = stackCumBeats[i] + chordData->beats[i];
                }
            }

            const double* cumBeats = nullptr;
            bool hasCumBeatsForTransitions = false;

            if (hasValidCumBeats)
            {
                cumBeats = chordData->cumBeats.data();
                hasCumBeatsForTransitions = true;
            }
            else if (useStackFallback)
            {
                cumBeats = stackCumBeats.data();
                hasCumBeatsForTransitions = true;
            }
            // else: cumBeatsが利用できない場合は遷移処理をスキップ

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

                // Find current slot from cumBeats (F2: cumBeats==nullptr の場合を回避).
                if (hasCumBeatsForTransitions)
                {
                    for (int i = 0; i < chordData->totalChords; ++i)
                    {
                        if (mCurrentBeatPosition < cumBeats[i + 1])
                        {
                            mCurrentChordIndex = i;
                            break;
                        }
                        mCurrentChordIndex = i;
                    }
                }
                // else: cumBeatsが利用できない場合はmCurrentChordIndex=0のままとする

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
            // F4: numSamples<=0のときは遷移処理をスキップ（jlimitの上限が-1になるのを回避）
            if (numSamples > 0 && hasCumBeatsForTransitions)
            {
                // A3: 最大イテレーション上限（totalChords*2 + ブロック内遷移数の見積もり）
                const int maxIterations = chordData->totalChords * 2 + 16;
                int iterations = 0;
                while (iterations < maxIterations)
                {
                    ++iterations;

                    double nextTransitionBeat = cumBeats[mCurrentChordIndex + 1];

                    if (nextTransitionBeat > endBeat)
                        break;

                    if (nextTransitionBeat >= startBeat)
                    {
                        // A6: relativeOffsetをjlimitでクランプ
                        int relativeOffset = static_cast<int>((nextTransitionBeat - startBeat) / beatsPerSample);
                        relativeOffset = juce::jlimit(0, numSamples - 1, relativeOffset);

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

                // F3: maxIterations到達で打ち切られた場合、chordIndexを再同期
                if (iterations >= maxIterations)
                {
                    double wrappedPos = fmod(mCurrentBeatPosition, totalBeats);
                    if (wrappedPos < 0) wrappedPos += totalBeats;
                    for (int i = 0; i < chordData->totalChords; ++i)
                    {
                        if (wrappedPos < cumBeats[i + 1])
                        {
                            mCurrentChordIndex = i;
                            break;
                        }
                    }
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

    bool pluginWasReady = false;
    {
        // A2: ScopedLock → tryEnter() でオーディオスレッドをブロックしない
        if (pluginLock.tryEnter())
        {
            if (mPluginInstance != nullptr && std::atomic_load(&mPluginReady))
            {
                int numDeviceCh = buffer.getNumChannels();
                int numPluginOut = mPluginInstance->getTotalNumOutputChannels();
                int numCh = juce::jmax (numDeviceCh, numPluginOut);

                // A1: processBlock内でのsetSizeによる再確保はスキップ
                // F1: 確保済み容量を超える場合はフォールバックへ(pluginWasReady=false)
                if (mTempBuffer.getNumChannels() >= numCh && mTempBuffer.getNumSamples() >= buffer.getNumSamples())
                {
                    pluginWasReady = true;
                    mTempBuffer.clear();

                    try
                    {
                        mPluginInstance->processBlock (mTempBuffer, midiMessages);
                        for (int ch = 0; ch < numDeviceCh; ++ch)
                            buffer.copyFrom (ch, 0, mTempBuffer, ch, 0, buffer.getNumSamples());
                    }
                    catch (...)
                    {
                        // A8: 例外時にpluginReady=falseにして以後フォールバックへ
                        juce::Logger::writeToLog ("Exception caught during plugin processBlock");
                        std::atomic_store(&mPluginReady, false);
                        pluginWasReady = false;
                    }
                }
            }
            pluginLock.exit();
        }
        // tryEnter失敗時はpluginWasReady=falseのまま（フォールバックシンセ）
    }

    if (!pluginWasReady)
    {
        mFallbackSynth.renderNextBlock (buffer, midiMessages, 0, buffer.getNumSamples());
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
        // A5: prepareToPlay前にバスを有効化
        mPluginInstance->enableAllBuses();
        mPluginInstance->prepareToPlay (sampleRate, blockSize);

        // Size scratch buffer while mPluginReady is still false.
        const int maxCh = juce::jmax (2, juce::jmax (mPluginInstance->getTotalNumInputChannels(),
                                                      mPluginInstance->getTotalNumOutputChannels()));
        mTempBuffer.setSize (maxCh, blockSize);
    }

    juce::ScopedLock sl (pluginLock);
    std::atomic_store(&mPluginReady, true);
}

void AudioEngine::setBpm(int newBpm)
{
    // A4: BPMを1〜999にクランプ
    mBpm.store(juce::jlimit(1, 999, newBpm));
}

void AudioEngine::setVolume(float newVolume)
{
    // A4: Volumeを0.0〜1.0にクランプ
    mVolume.store(juce::jlimit(0.0f, 1.0f, newVolume));
}

void AudioEngine::setChordData(std::shared_ptr<ChordData> newData)
{
    std::atomic_store(&mCurrentChordData, newData);
}

void AudioEngine::startPlayback()
{
    // Actual state reset happens on the audio thread (see processBlock) so
    // message-thread writes never race with audio-thread-only fields.
    mPlayingSlotShared.store(0, std::memory_order_relaxed);
    std::atomic_store(&mStartRequested, true);
    std::atomic_store(&mIsPlaying, true);
}

void AudioEngine::stopPlayback()
{
    std::atomic_store(&mStartRequested, false);
    std::atomic_store(&mStopRequested, true);
    std::atomic_store(&mIsPlaying, false);
}
