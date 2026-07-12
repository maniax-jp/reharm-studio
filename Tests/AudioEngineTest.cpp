#include <JuceHeader.h>
#include "../Source/AudioEngine.h"
#include <vector>

class MockPluginInstance : public juce::AudioPluginInstance
{
public:
    MockPluginInstance() : juce::AudioPluginInstance (juce::AudioProcessor::BusesProperties()) {}

    // AudioProcessor overrides
    const juce::String getName() const override { return "MockPlugin"; }
    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return "Default"; }
    void changeProgramName (int index, const juce::String& newName) override {}
    void getStateInformation (juce::MemoryBlock& destData) override {}
    void setStateInformation (const void* data, int sizeInBytes) override {}

    // AudioPluginInstance overrides
    void fillInPluginDescription (juce::PluginDescription& description) const override
    {
        description.name = "MockPlugin";
        description.manufacturerName = "MockVendor";
    }

    // Other overrides
    void prepareToPlay (double sampleRate, int samplesPerBlock) override {}
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override {}
    void releaseResources() override {}
};

class AudioEngineTest : public juce::UnitTest
{
public:
    AudioEngineTest() : juce::UnitTest ("AudioEngineTest") {}

    void runTest() override
    {
        beginTest ("InitialNoteOnAtStart");
        {
            AudioEngine engine;
            double sampleRate = 44100.0;
            int bpm = 120;
            engine.prepareToPlay (512, sampleRate);
            engine.setBpm (bpm);

            // Create a simple progression: C Major (60, 64, 67)
            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 1;
            chordData->notes.push_back ({60, 64, 67});
            chordData->beats.push_back (4.0);
            chordData->totalBeats = 4.0;
            engine.setChordData (chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            engine.processBlock (buffer, midi);

            expect (midi.getNumEvents() == 3, "Should have 3 NoteOn events at the start");
        }

        beginTest ("ChordTransitionTiming");
        {
            AudioEngine engine;
            double sampleRate = 44100.0;
            int bpm = 60; // 1 beat = 1 second = 44100 samples
            engine.prepareToPlay (512, sampleRate);
            engine.setBpm (bpm);

            // Progression: C Major (60, 64, 67) -> G Major (67, 71, 74)
            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 2;
            chordData->notes.push_back ({60, 64, 67});
            chordData->notes.push_back ({67, 71, 74});
            chordData->beats.push_back (4.0);
            chordData->beats.push_back (4.0);
            chordData->totalBeats = 8.0;
            engine.setChordData (chordData);
            engine.startPlayback();

            // First block: NoteOn for chord 0
            juce::AudioBuffer<float> buffer (2, 44100); // 1 second block
            juce::MidiBuffer midi;
            engine.processBlock (buffer, midi);

            // Transition happens at 4 beats = 4 seconds.
            // Since our block is only 1 second, no transition yet.
            expect (midi.getNumEvents() == 3, "Should only have first chord notes in first second");

            // Process 3 more seconds
            midi.clear();
            engine.processBlock (buffer, midi); // 2s
            engine.processBlock (buffer, midi); // 3s
            engine.processBlock (buffer, midi); // 4s

            // In the 4th second, we expect NoteOff for chord 0 and NoteOn for chord 1
            // Total events: 3 NoteOff + 3 NoteOn = 6
            expect (midi.getNumEvents() == 6, "Should have transition events at 4 seconds");
        }

        beginTest ("StopPlaybackImmediateNoteOff");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);

            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 1;
            chordData->notes.push_back ({60, 64, 67});
            chordData->beats.push_back (4.0);
            chordData->totalBeats = 4.0;
            engine.setChordData (chordData);
            engine.startPlayback();

            // Start processing
            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            engine.processBlock (buffer, midi);

            // Stop playback
            engine.stopPlayback();
            midi.clear();
            engine.processBlock (buffer, midi);

            expect (midi.getNumEvents() == 3, "Should send NoteOff for all notes when stopping");
        }

        beginTest ("PluginInstanceGetter");
        {
            AudioEngine engine;
            auto plugin = std::make_unique<MockPluginInstance>();
            auto* pluginPtr = plugin.get();

            engine.setPluginInstance (std::move (plugin));
            expect (engine.getPluginInstance() == pluginPtr, "getPluginInstance should return the loaded plugin");
        }

        // ---- New tests for variable-length chord slots ----

        beginTest ("NewConstructorVariableBeats");
        {
            auto chordData = std::make_shared<ChordData>(
                std::vector<std::vector<int>> {{60, 64, 67}, {67, 71, 74}, {65, 69, 72}},
                std::vector<double> {4.0, 2.0, 2.0}
            );
            expectEquals (chordData->totalBeats, 8.0, "Total beats should be 8.0");
            expectEquals (chordData->totalChords, 3, "Should have 3 chords");
            expectEquals ((int) chordData->beats.size(), 3, "Should have 3 beat entries");
            expectEquals (chordData->beats[0], 4.0, "First slot should be 4 beats");
            expectEquals (chordData->beats[1], 2.0, "Second slot should be 2 beats");
            expectEquals (chordData->beats[2], 2.0, "Third slot should be 2 beats");
        }

        beginTest ("OldConstructorSetsDefaultBeats");
        {
            juce::Array<juce::Array<int>> juceNotes;
            juceNotes.add ({60, 64, 67});
            juceNotes.add ({67, 71, 74});

            auto chordData = std::make_shared<ChordData>(juceNotes);
            expectEquals (chordData->totalChords, 2, "Should have 2 chords");
            expectEquals ((int) chordData->beats.size(), 2, "Should have 2 beat entries");
            expectEquals (chordData->beats[0], 4.0, "First slot default should be 4.0");
            expectEquals (chordData->beats[1], 4.0, "Second slot default should be 4.0");
            expectEquals (chordData->totalBeats, 8.0, "Total beats should be 8.0");
        }

        beginTest ("EmptySlotDoesNotCrash");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);
            engine.setBpm (120);

            // Progression with an empty (rest) slot in the middle.
            auto chordData = std::make_shared<ChordData>(
                std::vector<std::vector<int>> {{60, 64, 67}, {}, {67, 71, 74}},
                std::vector<double> {4.0, 2.0, 2.0}
            );
            engine.setChordData (chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            // Process several blocks; should not crash even with empty slot.
            for (int i = 0; i < 10; ++i)
            {
                midi.clear();
                engine.processBlock (buffer, midi);
            }

            expect (true, "Processing with empty slot should not crash");
        }

        beginTest ("GetCurrentPlayingSlot");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);
            engine.setBpm (120);

            // Stopped state should return -1.
            expectEquals (engine.getCurrentPlayingSlot(), -1, "Stopped should return -1");

            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 1;
            chordData->notes.push_back ({60, 64, 67});
            chordData->beats.push_back (4.0);
            chordData->totalBeats = 4.0;
            engine.setChordData (chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            engine.processBlock (buffer, midi);

            expect (engine.getCurrentPlayingSlot() >= 0, "Playing slot should be >= 0 after playback starts");
        }
    }
};

static AudioEngineTest audioEngineTest;
