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
            expect (midi.getNumEvents() == 6, "Should have transition events");
        }

        beginTest ("VariableChordDurations");
        {
            AudioEngine engine;
            double sampleRate = 44100.0;
            engine.prepareToPlay (512, sampleRate);
            engine.setBpm (120);

            // Progression with variable durations: C(2 beats) -> Am(3 beats) -> F(5 beats)
            auto chordData = std::make_shared<ChordData>(
                std::vector<std::vector<int>>{{60, 64, 67}, {69, 72, 76}, {65, 69, 72}},
                std::vector<double>{2.0, 3.0, 5.0});
            engine.setChordData (chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            // First block should produce NoteOn for chord 0
            buffer.clear();
            midi.clear();
            engine.processBlock (buffer, midi);
            expect (midi.getNumEvents() == 3, "Should have 3 NoteOn events for first chord");
        }

        beginTest ("StopPlaybackSendsNoteOffs");
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

            // Stop playback (triggers note-off)
            engine.stopPlayback();
            buffer.clear();
            midi.clear();
            engine.processBlock (buffer, midi);

            // Check that NoteOff and allNotesOff were sent
            // (NoteOff events should be from mSoundingNotes) and allNotesOff
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

        // ===== 新テスト: A3 ビート0のクランプ =====
        
        // ===== 復元テスト: PluginInstanceGetter =====
        beginTest ("PluginInstanceGetter");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);

            // loadPluginInstance でプラグインを設定
            auto mock = std::make_unique<MockPluginInstance>();
            engine.loadPluginInstance(std::move(mock), 44100.0, 512);

            // getPluginInstance が nullptr でないことを確認
            expect (engine.getPluginInstance() != nullptr,
                    "getPluginInstance should return the loaded plugin");
        }

        // ===== 復元テスト: NewConstructorVariableBeats =====
        beginTest ("NewConstructorVariableBeats");
        {
            auto chordData = std::make_shared<ChordData>(
                std::vector<std::vector<int>>{{60}, {64}, {67}},
                std::vector<double>{2.0, 3.0, 6.0});

            expect (chordData->beats[0] == 2.0, "New constructor should set beats[0]=2.0");
            expect (chordData->beats[1] == 3.0, "New constructor should set beats[1]=3.0");
            expect (chordData->beats[2] == 6.0, "New constructor should set beats[2]=6.0");
        }

        // ===== 復元テスト: OldConstructorSetsDefaultBeats =====
        beginTest ("OldConstructorSetsDefaultBeats");
        {
            juce::Array<juce::Array<int>> notes;
            notes.add(juce::Array<int>{60, 64, 67});
            notes.add(juce::Array<int>{67, 71, 74});

            auto chordData = std::make_shared<ChordData>(notes);

            for (int i = 0; i < chordData->totalChords; ++i)
                expect (chordData->beats[i] == 4.0, "Old constructor should set all beats to 4.0");
        }

        // ===== 復元テスト: EmptySlotDoesNotCrash =====
        beginTest ("EmptySlotDoesNotCrash");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);

            // スロットにノートが含まれていないChordData
            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 2;
            chordData->notes.push_back({}); // 空のノート
            chordData->notes.push_back({60, 64, 67});
            chordData->beats.push_back(4.0);
            chordData->beats.push_back(4.0);
            chordData->totalBeats = 8.0;
            engine.setChordData(chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;

            // クラッシュせずに処理できることを確認
            for (int i = 0; i < 10; ++i)
            {
                buffer.clear();
                midi.clear();
                engine.processBlock(buffer, midi);
            }

            expect(true, "Processing empty slots should not crash");
        }

        // ===== 復元テスト: GetCurrentPlayingSlot =====
        beginTest ("GetCurrentPlayingSlot");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);

            // 再生前に-1であることを確認
            expect (engine.getCurrentPlayingSlot() == -1,
                    "getCurrentPlayingSlot should be -1 before playback");

            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 1;
            chordData->notes.push_back({60, 64, 67});
            chordData->beats.push_back(4.0);
            chordData->totalBeats = 4.0;
            engine.setChordData(chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            engine.processBlock(buffer, midi);

            // 再生後、現在のスロットが0であることを確認
            expect (engine.getCurrentPlayingSlot() == 0,
                    "getCurrentPlayingSlot should be 0 after playback starts");
        }

        // ===== 復元テスト: ChordDataSwapDuringPlayback =====
        beginTest ("ChordDataSwapDuringPlayback sends NoteOff for old notes");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);
            engine.setBpm (120);

            auto chordData1 = std::make_shared<ChordData>();
            chordData1->totalChords = 2;
            chordData1->notes.push_back({60, 64, 67});
            chordData1->notes.push_back({67, 71, 74});
            chordData1->beats.push_back(4.0);
            chordData1->beats.push_back(4.0);
            chordData1->totalBeats = 8.0;
            engine.setChordData(chordData1);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            engine.processBlock(buffer, midi);
            midi.clear();

            // Swap chord data
            auto chordData2 = std::make_shared<ChordData>();
            chordData2->totalChords = 1;
            chordData2->notes.push_back({72, 76, 79});
            chordData2->beats.push_back(4.0);
            chordData2->totalBeats = 4.0;
            engine.setChordData(chordData2);

            // Process to trigger swap detection
            engine.processBlock(buffer, midi);

            bool foundNoteOff = false;
            bool foundNoteOn = false;
            for (const auto meta : midi)
            {
                const auto msg = meta.getMessage();
                if (msg.isNoteOff()) foundNoteOff = true;
                if (msg.isNoteOn()) foundNoteOn = true;
            }
            expect (foundNoteOff, "Should send NoteOff for old notes on swap");
            expect (foundNoteOn, "Should send NoteOn for new chord on swap");
        }

        // ===== 復元テスト: StopAfterSwap =====
        beginTest ("StopAfterSwap sends NoteOff and allNotesOff");
        {
            AudioEngine engine;
            engine.prepareToPlay (512, 44100.0);
            engine.setBpm (120);

            auto chordData1 = std::make_shared<ChordData>();
            chordData1->totalChords = 1;
            chordData1->notes.push_back({60, 64, 67});
            chordData1->beats.push_back(4.0);
            chordData1->totalBeats = 4.0;
            engine.setChordData(chordData1);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer(2, 512);
            juce::MidiBuffer midi;
            engine.processBlock(buffer, midi);
            midi.clear();

            // Swap chord data
            auto chordData2 = std::make_shared<ChordData>();
            chordData2->totalChords = 1;
            chordData2->notes.push_back({72, 76, 79});
            chordData2->beats.push_back(4.0);
            chordData2->totalBeats = 4.0;
            engine.setChordData(chordData2);

            engine.processBlock(buffer, midi);
            midi.clear();

            // Stop playback
            engine.stopPlayback();
            engine.processBlock(buffer, midi);

            bool foundNoteOff = false;
            bool foundAllNotesOff = false;
            for (const auto meta : midi)
            {
                const auto msg = meta.getMessage();
                if (msg.isNoteOff()) foundNoteOff = true;
                if (msg.isAllNotesOff()) foundAllNotesOff = true;
            }
            expect (foundNoteOff, "Should send NoteOff for sounding notes on stop");
            expect (foundAllNotesOff, "Should send allNotesOff on stop");

            // After stop, further processBlock should not produce noteOn
            midi.clear();
            engine.processBlock(buffer, midi);
            bool foundNoteOn = false;
            for (const auto meta : midi)
            {
                if (meta.getMessage().isNoteOn()) foundNoteOn = true;
            }
            expect (!foundNoteOn, "Should not send NoteOn after stop");
        }


        // ===== 新テスト: ChordData ビートクランプ =====
        beginTest ("ChordDataZeroBeatClamped");
        {
            // ビート0を含むChordDataを構築し、クランプされることを確認
            auto chordData = std::make_shared<ChordData>(
                std::vector<std::vector<int>>{{60, 64, 67}, {67, 71, 74}, {69, 72, 76}},
                std::vector<double>{0.0, 4.0, -1.0});

            expect (chordData->totalChords == 3, "Should have 3 chords");
            for (int i = 0; i < chordData->totalChords; ++i)
                expect (chordData->beats[i] >= 0.01, "Each beat should be clamped to at least 0.01");

            expect (chordData->beats[0] == 0.01, "Beat 0 should be clamped to 0.01");
            expect (chordData->beats[2] == 0.01, "Negative beat should be clamped to 0.01");
            expect (chordData->beats[1] == 4.0, "Normal beat should remain 4.0");
        }

        // ===== 新テスト: A4 サンプルレート0の安全性 =====
        beginTest ("ProcessBlockWithZeroSampleRate");
        {
            AudioEngine engine;
            // サンプルレート0でprepareToPlay
            engine.prepareToPlay (512, 0.0);

            auto chordData = std::make_shared<ChordData>();
            chordData->totalChords = 1;
            chordData->notes.push_back ({60, 64, 67});
            chordData->beats.push_back (4.0);
            chordData->totalBeats = 4.0;
            engine.setChordData (chordData);
            engine.startPlayback();

            juce::AudioBuffer<float> buffer (2, 512);
            juce::MidiBuffer midi;

            // クラッシュせずにbufferがclearされることを確認
            engine.processBlock (buffer, midi);
            expect (buffer.getMagnitude(0, 0, 512) < 1.0e-6f,
                    "Buffer should be silent with zero sample rate");
        }

        // ===== 新テスト: A4 BPM/Volumeクランプ =====
        beginTest ("BpmAndVolumeClamping");
        {
            AudioEngine engine;

            // BPM クランプ確認
            engine.setBpm(0);
            expect (engine.mBpm.load() == 1, "BPM 0 should be clamped to 1");

            engine.setBpm(-100);
            expect (engine.mBpm.load() == 1, "Negative BPM should be clamped to 1");

            engine.setBpm(2000);
            expect (engine.mBpm.load() == 999, "BPM 2000 should be clamped to 999");

            engine.setBpm(120);
            expect (engine.mBpm.load() == 120, "Normal BPM should pass through");

            // Volume クランプ確認
            engine.setVolume(-1.0f);
            expect (engine.mVolume.load() == 0.0f, "Volume -1.0 should be clamped to 0.0");

            engine.setVolume(2.0f);
            expect (engine.mVolume.load() == 1.0f, "Volume 2.0 should be clamped to 1.0");

            engine.setVolume(0.5f);
            expect (engine.mVolume.load() == 0.5f, "Normal volume should pass through");
        }

        // ===== 新テスト: A1 cumBeats事前計算の整合性 =====
        beginTest ("CumBeatsPrecomputedCorrectly");
        {
            // 等間隔ビートのケース
            juce::Array<juce::Array<int>> notes1;
            notes1.add(juce::Array<int>{60, 64, 67});
            notes1.add(juce::Array<int>{67, 71, 74});
            notes1.add(juce::Array<int>{69, 72, 76});
            auto cd1 = std::make_shared<ChordData>(notes1);

            expect (cd1->cumBeats.size() == 4, "cumBeats should have totalChords+1 entries");
            expect (cd1->cumBeats[0] == 0.0, "cumBeats[0] should be 0.0");
            expect (cd1->cumBeats[1] == 4.0, "cumBeats[1] should be 4.0");
            expect (cd1->cumBeats[2] == 8.0, "cumBeats[2] should be 8.0");
            expect (cd1->cumBeats[3] == 12.0, "cumBeats[3] should be 12.0 (totalBeats)");

            // 可変ビートのケース
            auto cd2 = std::make_shared<ChordData>(
                std::vector<std::vector<int>>{{60, 64, 67}, {67, 71, 74}, {69, 72, 76}},
                std::vector<double>{2.0, 3.0, 5.0});

            expect (cd2->cumBeats.size() == 4, "cumBeats should have totalChords+1 entries");
            expect (cd2->cumBeats[0] == 0.0, "cumBeats[0] should be 0.0");
            expect (cd2->cumBeats[1] == 2.0, "cumBeats[1] should be 2.0");
            expect (cd2->cumBeats[2] == 5.0, "cumBeats[2] should be 5.0");
            expect (cd2->cumBeats[3] == 10.0, "cumBeats[3] should be 10.0 (totalBeats)");

            // クランプされたビートでもcumBeatsが整合していることを確認
            auto cd3 = std::make_shared<ChordData>(
                std::vector<std::vector<int>>{{60}, {64}},
                std::vector<double>{0.0, 3.0});

            expect (cd3->cumBeats.size() == 3, "cumBeats should have totalChords+1 entries");
            expect (cd3->cumBeats[0] == 0.0, "cumBeats[0] should be 0.0");
            expect (cd3->cumBeats[1] == 0.01, "cumBeats[1] should be 0.01 (clamped from 0.0)");
            expect (cd3->cumBeats[2] == 3.01, "cumBeats[2] should be 3.01 (0.01 + 3.0)");
        }
    }
};

static AudioEngineTest audioEngineTest;
