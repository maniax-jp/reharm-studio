#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <memory>

/**
 * ChordData holds the processed MIDI notes for a chord progression.
 * Using a struct allows us to swap the entire progression atomically.
 */
struct ChordData
{
    std::vector<std::vector<int>> notes;
    int totalChords = 0;

    ChordData() = default;
    ChordData(const juce::Array<juce::Array<int>>& juceNotes)
    {
        totalChords = juceNotes.size();
        for (const auto& chord : juceNotes)
        {
            notes.emplace_back(chord.begin(), chord.end());
        }
    }
};

/**
 * AudioEngine handles the real-time audio processing, MIDI scheduling,
 * and VST3 plugin hosting.
 */
class AudioEngine
{
public:
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

    // State getters
    bool isPlaying() const { return mIsPlaying.load(); }
    int getBpm() const { return mBpm.load(); }

private:
    // Plugin and Audio state
    std::unique_ptr<juce::AudioPluginInstance> mPluginInstance;
    juce::CriticalSection pluginLock;
    std::atomic<bool> mPluginReady { false };
    double mSampleRate = 44100.0;

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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
