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

            // 3 note-offs for the sounding chord plus one allNotesOff safety message.
            expect (midi.getNumEvents() == 4, "Should send NoteOff for all notes plus allNotesOff when stopping");
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

        beginTest ("ChordDataSwapDuringPlayback sends NoteOff for old notes");
        {
            AudioEngine engine;
            double sampleRate = 44100.0;
            int bpm = 60; // 1 beat = 1 second
            engine.prepareToPlay (512, sampleRate);
            engine.setBpm (bpm);

            // Original: C Major (notes 60, 64, 67), 4 beats
            auto chordData1 = std::make_shared<ChordData>();
            chordData1->totalChords = 1;
            chordData1->notes.push_back ({60, 64, 67});
            chordData1->beats.push_back (4.0);
            chordData1->totalBeats = 4.0;
            engine.setChordData (chordData1);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            engine.processBlock (buffer, midi);

            // First block: 3 NoteOn events for C Major
            expect (midi.getNumEvents() == 3, "Should have 3 NoteOn at start");
            midi.clear();

            // Process another block without transition (short block, no chord change)
            engine.processBlock (buffer, midi);
            midi.clear();

            // Swap to new chord data: G Major (notes 67, 71, 74)
            auto chordData2 = std::make_shared<ChordData>();
            chordData2->totalChords = 1;
            chordData2->notes.push_back ({67, 71, 74});
            chordData2->beats.push_back (4.0);
            chordData2->totalBeats = 4.0;
            engine.setChordData (chordData2);

            // Next processBlock should send NoteOff for old notes (60, 64, 67)
            // and NoteOn for new notes (67, 71, 74)
            engine.processBlock (buffer, midi);

            // Check that note-off events for old notes exist
            bool foundOldNoteOff = false;
            bool foundNewNoteOn = false;
            for (const auto meta : midi)
            {
                const auto msg = meta.getMessage();
                if (msg.isNoteOff())
                {
                    if (msg.getNoteNumber() == 60 || msg.getNoteNumber() == 64)
                        foundOldNoteOff = true;
                }
                if (msg.isNoteOn())
                {
                    if (msg.getNoteNumber() == 71 || msg.getNoteNumber() == 74)
                        foundNewNoteOn = true;
                }
            }
            expect (foundOldNoteOff, "Should send NoteOff for old chord notes after swap");
            expect (foundNewNoteOn, "Should send NoteOn for new chord notes after swap");
        }

        beginTest ("StopAfterSwap sends NoteOff and allNotesOff");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);
            engine.setBpm (60);

            // Original chord data
            auto chordData1 = std::make_shared<ChordData>();
            chordData1->totalChords = 1;
            chordData1->notes.push_back ({60, 64, 67});
            chordData1->beats.push_back (4.0);
            chordData1->totalBeats = 4.0;
            engine.setChordData (chordData1);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;
            engine.processBlock (buffer, midi);
            midi.clear();

            // Swap chord data
            auto chordData2 = std::make_shared<ChordData>();
            chordData2->totalChords = 1;
            chordData2->notes.push_back ({72, 76, 79});
            chordData2->beats.push_back (4.0);
            chordData2->totalBeats = 4.0;
            engine.setChordData (chordData2);

            // Process to trigger swap
            engine.processBlock (buffer, midi);
            midi.clear();

            // Stop playback
            engine.stopPlayback();
            engine.processBlock (buffer, midi);

            // Should have note-off events (from mSoundingNotes) and allNotesOff
            bool foundNoteOff = false;
            bool foundAllNotesOff = false;
            for (const auto meta : midi)
            {
                const auto msg = meta.getMessage();
                if (msg.isNoteOff())
                    foundNoteOff = true;
                if (msg.isAllNotesOff())
                    foundAllNotesOff = true;
            }
            expect (foundNoteOff, "Should send NoteOff for sounding notes on stop");
            expect (foundAllNotesOff, "Should send allNotesOff on stop");

            // After stop, further processBlock should not produce noteOn
            midi.clear();
            engine.processBlock (buffer, midi);
            bool foundNoteOn = false;
            for (const auto meta : midi)
            {
                if (meta.getMessage().isNoteOn())
                    foundNoteOn = true;
            }
            expect (!foundNoteOn, "Should not send NoteOn after stop");
        }

        beginTest ("FallbackSynthProducesAudioWithoutPlugin");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);
            engine.setBpm (120);

            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 1;
            chordData->notes.push_back ({60, 64, 67});
            chordData->beats.push_back (4.0);
            chordData->totalBeats = 4.0;
            engine.setChordData (chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            for (int i = 0; i < 10; ++i)
            {
                buffer.clear();
                midi.clear();
                engine.processBlock (buffer, midi);
            }

            expect (buffer.getMagnitude(0, 0, 512) > 0.0f,
                    "Fallback synth should produce audio without plugin");
        }

        beginTest ("FallbackSynthSilentWhenStopped");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            for (int i = 0; i < 10; ++i)
            {
                buffer.clear();
                midi.clear();
                engine.processBlock (buffer, midi);
            }

            expect (buffer.getMagnitude(0, 0, 512) < 1.0e-6f,
                    "Fallback synth should be silent when not playing");
        }

        beginTest ("FallbackSynthStopsAfterStopPlayback");
        {
            AudioEngine engine;
            double sampleRate = 44100.0;
            int blockSize = 512;
            engine.prepareToPlay (blockSize, sampleRate);
            engine.setBpm (120);

            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 1;
            chordData->notes.push_back ({60, 64, 67});
            chordData->beats.push_back (4.0);
            chordData->totalBeats = 4.0;
            engine.setChordData (chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, blockSize);
            juce::MidiBuffer midi;

            // Start playback (triggers note-on)
            buffer.clear();
            midi.clear();
            engine.processBlock (buffer, midi);

            // Stop playback (triggers note-off, starts ADSR release)
            engine.stopPlayback();
            buffer.clear();
            midi.clear();
            engine.processBlock (buffer, midi);

            // Process enough blocks for the ADSR release tail to finish (0.08s).
            // 0.08s = 3528 samples; with 512-sample blocks, 8 blocks = 4096 samples.
            for (int i = 0; i < 8; ++i)
            {
                buffer.clear();
                midi.clear();
                engine.processBlock (buffer, midi);
            }

            // After release tail, buffer should be silent.
            expect (buffer.getMagnitude(0, 0, blockSize) < 1.0e-6f,
                    "Fallback synth should go silent after stopPlayback and release tail");
        }
    }
};

static AudioEngineTest audioEngineTest;
