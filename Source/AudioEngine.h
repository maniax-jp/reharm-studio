#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <memory>

/**
 * ChordData holds the processed MIDI notes for a chord progression.
 * Using a struct allows us to swap the entire progression atomically.
 */
struct ChordData
{
    std::vector<std::vector<int>> notes;
    std::vector<double> beats;
    int totalChords = 0;
    double totalBeats = 0.0;

    ChordData() = default;

    // Existing constructor: assigns 4.0 beats per chord.
    ChordData (const juce::Array<juce::Array<int>>& juceNotes)
    {
        totalChords = juceNotes.size();
        for (const auto& chord : juceNotes)
        {
            notes.emplace_back(chord.begin(), chord.end());
            beats.push_back(4.0);
        }
        totalBeats = (double) totalChords * 4.0;
    }

    // New constructor with explicit per-slot durations.
    ChordData (std::vector<std::vector<int>> newNotes, std::vector<double> newBeats)
    {
        jassert(newNotes.size() == newBeats.size());
        const auto sz = std::min(newNotes.size(), newBeats.size());
        notes.assign(newNotes.begin(), newNotes.begin() + sz);
        beats.assign(newBeats.begin(), newBeats.begin() + sz);
        totalChords = static_cast<int>(sz);
        for (double b : beats)
            totalBeats += b;
    }
};

/**
 * AudioEngine handles the real-time audio processing, MIDI scheduling,
 * and VST3 plugin hosting.
 */
class AudioEngine
{
public:
    /** Host playhead so plugins always see valid tempo/position (never zeroed ProcessContext). */
    class HostPlayHead : public juce::AudioPlayHead
    {
    public:
        juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
        {
            juce::AudioPlayHead::PositionInfo info;
            const double sr  = sampleRate.load();
            const auto  smp  = samples.load();
            const double sec = sr > 0 ? (double) smp / sr : 0.0;
            const double curBpm = bpm.load();
            const double ppqPos = ppq.load();
            info.setBpm(curBpm);
            info.setTimeSignature(juce::AudioPlayHead::TimeSignature{4, 4});
            info.setTimeInSamples((juce::int64) smp);
            info.setTimeInSeconds(sec);
            info.setPpqPosition(ppqPos);
            info.setPpqPositionOfLastBarStart(std::floor(ppqPos / 4.0) * 4.0);
            info.setIsPlaying(playing.load());
            info.setIsRecording(false);
            info.setIsLooping(false);
            return info;
        }

        std::atomic<double>  bpm { 120.0 };
        std::atomic<bool>    playing { false };
        std::atomic<int64_t> samples { 0 };
        std::atomic<double>  sampleRate { 44100.0 };
        std::atomic<double>  ppq { 0.0 };
    };

    AudioEngine();
    ~AudioEngine();

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate);
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    void releaseResources();

    // Control methods
    juce::AudioPluginInstance* getPluginInstance() const { return mPluginInstance.get(); }
    void setPluginInstance(std::unique_ptr<juce::AudioPluginInstance> plugin);
    void loadPlugin(std::unique_ptr<juce::AudioPluginInstance> plugin, double sampleRate, int blockSize);
    void setPluginReady(bool ready) { std::atomic_store(&mPluginReady, ready); }
    void setBpm(int newBpm);
    void setVolume(float newVolume);
    void setChordData(std::shared_ptr<ChordData> newData);
    void startPlayback();
    void stopPlayback();
    void beginPluginLoad();
    void endPluginLoad();
    bool isPluginLoading() const { return mLoadingPlugin.load(); }
    void loadPluginInstance(std::unique_ptr<juce::AudioPluginInstance> plugin, double sampleRate, int blockSize);

    /** Index of the slot currently sounding, or -1 when stopped. Safe from any thread. */
    int getCurrentPlayingSlot() const
    { return mPlayingSlotShared.load(std::memory_order_relaxed); }

private:
    // Playhead must outlive the plugin (plugin holds a raw pointer via setPlayHead).
    HostPlayHead mPlayHead;

    // Plugin and Audio state
    std::unique_ptr<juce::AudioPluginInstance> mPluginInstance;
    juce::CriticalSection pluginLock;
    std::atomic<bool> mPluginReady { false };
    double mSampleRate = 44100.0;

    // Preallocated scratch buffer for plugin processing (no heap alloc on audio thread).
    juce::AudioBuffer<float> mTempBuffer;

    // Thread-safe parameters
    std::atomic<int> mBpm { 120 };
    std::atomic<float> mVolume { 0.8f };
    std::atomic<bool> mIsPlaying { false };
    std::atomic<bool> mStopRequested { false };
    std::atomic<bool> mLoadingPlugin { false };

    // Chord progression data (Using C++17 atomic shared_ptr functions)
    std::shared_ptr<ChordData> mCurrentChordData;

    // Playback state (Used only in audio thread)
    int mCurrentChordIndex = 0;
    double mCurrentBeatPosition = 0.0;
    std::atomic<int> mPlayingSlotShared { -1 };
    bool mFirstBlock = false;

    // Audio-thread-only tracking of currently sounding notes.
    static constexpr int kMaxSoundingNotes = 16;
    int mSoundingNotes[kMaxSoundingNotes] = {};
    int mNumSoundingNotes = 0;
    const void* mActiveChordDataPtr = nullptr; // last seen ChordData address

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
