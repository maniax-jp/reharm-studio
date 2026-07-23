#include <JuceHeader.h>
#include "../Source/PlaybackBuilder.h"
#include "../Source/ProgressionModel.h"

using namespace reharm;

class PlaybackBuilderTest : public juce::UnitTest
{
public:
    PlaybackBuilderTest() : juce::UnitTest ("PlaybackBuilder") {}

    void runTest() override
    {
        beginTest ("Block mode compatibility: mixed subdivisions, interior rest, trailing rest trimmed");
        {
            ProgressionModel model;
            model.setNumBars (4);

            {
                ProgressionModel::ScopedBulkEdit bulk (model);

                model.setSubdivision (0, BarSubdivision::One);
                model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C

                model.setSubdivision (1, BarSubdivision::Two);
                model.setChord (1, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7
                model.setChord (1, 1, std::nullopt);                          // interior rest

                model.setSubdivision (2, BarSubdivision::Four);
                model.setChord (2, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7
                model.setChord (2, 1, std::nullopt);
                model.setChord (2, 2, Chord { 9, ChordQuality::Minor, -1 });     // Am
                model.setChord (2, 3, std::nullopt);

                model.setSubdivision (3, BarSubdivision::One);
                model.setChord (3, 0, std::nullopt); // trailing rest: must be trimmed
            }

            PlaybackSettings settings; // Off / Close
            auto data = PlaybackBuilder::build (model, settings);

            auto flat = model.flatten();
            expectEquals ((int) flat.size(), 8); // 1 + 2 + 4 + 1 (bar 3's single rest slot)

            // Trailing rest (bar 3, index 6) trimmed -> 6 engine slots remain.
            expectEquals (data->totalChords, 6);
            expectEquals ((int) data->displayIndices.size(), 6);

            for (int i = 0; i < 6; ++i)
                expectEquals (data->displayIndices[(size_t) i], i); // identity map

            // notes/beats match flatten() (minus the trimmed trailing rest).
            for (int i = 0; i < 6; ++i)
            {
                const auto& fc = flat[(size_t) i];
                expectEquals (data->beats[(size_t) i], fc.beats);

                if (fc.chord.has_value())
                {
                    const auto expectedNotes = Voicing::midiNotes (*fc.chord, settings.voicing);
                    expect (data->notes[(size_t) i] == expectedNotes);
                }
                else
                {
                    expect (data->notes[(size_t) i].empty());
                }
            }

            // Interior rest (index 2, bar1 slot1) is preserved as an empty-note engine slot.
            expect (data->notes[2].empty());
        }

        beginTest ("Arp Up: 1 bar div=1, C Major, rate=Eighth -> 8 micro-slots");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C major

            PlaybackSettings settings;
            settings.voicing = Voicing::Style::Close;
            settings.arpPattern = ArpPattern::Up;
            settings.arpRate = ArpRate::Eighth;

            auto data = PlaybackBuilder::build (model, settings);

            const auto chordNotes = Voicing::midiNotes (Chord { 0, ChordQuality::Major, -1 }, Voicing::Style::Close);
            expect (chordNotes == std::vector<int> ({ 60, 64, 67 }));

            expectEquals (data->totalChords, 8);
            const std::vector<int> expectedSeq = { 60, 64, 67, 60, 64, 67, 60, 64 };
            for (int i = 0; i < 8; ++i)
            {
                expectEquals ((int) data->notes[(size_t) i].size(), 1);
                expectEquals (data->notes[(size_t) i][0], expectedSeq[(size_t) i]);
                expectWithinAbsoluteError (data->beats[(size_t) i], 0.5, 1.0e-9);
                expectEquals (data->displayIndices[(size_t) i], 0);
            }
            expectWithinAbsoluteError (data->totalBeats, 4.0, 1.0e-9);
        }

        beginTest ("Arp Down and UpDown: G7 (4 notes) and Power5 (2 notes, k=2)");
        {
            // Down: G7 -> {55,59,62,65} (root=55 for pitch class 7 at octaveBase 60 -> 55+... );
            // derive expected via Voicing::midiNotes directly to avoid hardcoding octave math.
            const Chord g7 { 7, ChordQuality::Dominant7, -1 };
            const auto g7Notes = Voicing::midiNotes (g7, Voicing::Style::Close); // ascending, 4 notes
            expectEquals ((int) g7Notes.size(), 4);

            ProgressionModel modelDown;
            modelDown.setNumBars (1);
            modelDown.setSubdivision (0, BarSubdivision::One);
            modelDown.setChord (0, 0, g7);

            PlaybackSettings downSettings;
            downSettings.arpPattern = ArpPattern::Down;
            downSettings.arpRate = ArpRate::Quarter; // 4 beats / 1.0 per step = 4 micro-slots

            auto downData = PlaybackBuilder::build (modelDown, downSettings);
            expectEquals (downData->totalChords, 4);
            for (int i = 0; i < 4; ++i)
                expectEquals (downData->notes[(size_t) i][0], g7Notes[(size_t) (3 - i)]);

            // UpDown with a 2-note dyad (Power5): period should be 2 (n0, n1 repeating).
            const Chord power5 { 0, ChordQuality::Power5, -1 };
            const auto power5Notes = Voicing::midiNotes (power5, Voicing::Style::Close);
            expectEquals ((int) power5Notes.size(), 2);

            ProgressionModel modelUpDown;
            modelUpDown.setNumBars (1);
            modelUpDown.setSubdivision (0, BarSubdivision::One);
            modelUpDown.setChord (0, 0, power5);

            PlaybackSettings upDownSettings;
            upDownSettings.arpPattern = ArpPattern::UpDown;
            upDownSettings.arpRate = ArpRate::Eighth; // 4 beats / 0.5 = 8 micro-slots

            auto upDownData = PlaybackBuilder::build (modelUpDown, upDownSettings);
            expectEquals (upDownData->totalChords, 8);
            const std::vector<int> expectedPeriod2 = { power5Notes[0], power5Notes[1] };
            for (int i = 0; i < 8; ++i)
                expectEquals (upDownData->notes[(size_t) i][0], expectedPeriod2[(size_t) (i % 2)]);

            // UpDown period-6 check with a 4-note chord (G7): n0 n1 n2 n3 n2 n1 repeating.
            ProgressionModel modelUpDown4;
            modelUpDown4.setNumBars (1);
            modelUpDown4.setSubdivision (0, BarSubdivision::One);
            modelUpDown4.setChord (0, 0, g7);

            PlaybackSettings upDown4Settings;
            upDown4Settings.arpPattern = ArpPattern::UpDown;
            upDown4Settings.arpRate = ArpRate::Sixteenth; // 4 beats / 0.25 = 16 micro-slots

            auto upDown4Data = PlaybackBuilder::build (modelUpDown4, upDown4Settings);
            expectEquals (upDown4Data->totalChords, 16);
            const std::vector<int> expectedPeriod6 = { g7Notes[0], g7Notes[1], g7Notes[2], g7Notes[3], g7Notes[2], g7Notes[1] };
            for (int i = 0; i < 16; ++i)
                expectEquals (upDown4Data->notes[(size_t) i][0], expectedPeriod6[(size_t) (i % 6)]);
        }

        beginTest ("Rate differences: Quarter->4 micro-slots per 4-beat slot; Sixteenth->4 micro-slots per 1-beat slot");
        {
            ProgressionModel modelQuarter;
            modelQuarter.setNumBars (1);
            modelQuarter.setSubdivision (0, BarSubdivision::One); // 4 beats
            modelQuarter.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });

            PlaybackSettings quarterSettings;
            quarterSettings.arpPattern = ArpPattern::Up;
            quarterSettings.arpRate = ArpRate::Quarter;

            auto quarterData = PlaybackBuilder::build (modelQuarter, quarterSettings);
            expectEquals (quarterData->totalChords, 4);
            for (int i = 0; i < 4; ++i)
                expectWithinAbsoluteError (quarterData->beats[(size_t) i], 1.0, 1.0e-9);

            ProgressionModel modelSixteenth;
            modelSixteenth.setNumBars (1);
            modelSixteenth.setSubdivision (0, BarSubdivision::Four); // 1 beat per slot
            modelSixteenth.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });
            modelSixteenth.setChord (0, 1, std::nullopt);
            modelSixteenth.setChord (0, 2, std::nullopt);
            modelSixteenth.setChord (0, 3, std::nullopt);

            PlaybackSettings sixteenthSettings;
            sixteenthSettings.arpPattern = ArpPattern::Up;
            sixteenthSettings.arpRate = ArpRate::Sixteenth;

            auto sixteenthData = PlaybackBuilder::build (modelSixteenth, sixteenthSettings);
            // Trailing rests (slots 1..3) are trimmed; slot 0 (1 beat) expands to 4 micro-slots.
            expectEquals (sixteenthData->totalChords, 4);
            for (int i = 0; i < 4; ++i)
            {
                expectWithinAbsoluteError (sixteenthData->beats[(size_t) i], 0.25, 1.0e-9);
                expectEquals (sixteenthData->displayIndices[(size_t) i], 0);
            }
        }

        beginTest ("Rest slots are never subdivided even with arpeggio on");
        {
            ProgressionModel model;
            model.setNumBars (2);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });
            model.setSubdivision (1, BarSubdivision::One);
            model.setChord (1, 0, std::nullopt); // interior... but it's the last bar; add a bar after to keep it interior
            model.setNumBars (3);
            model.setSubdivision (2, BarSubdivision::One);
            model.setChord (2, 0, Chord { 5, ChordQuality::Minor, -1 });

            PlaybackSettings settings;
            settings.arpPattern = ArpPattern::Up;
            settings.arpRate = ArpRate::Eighth;

            auto data = PlaybackBuilder::build (model, settings);
            expectEquals (data->totalChords, 8 + 1 + 8); // chord->8 microslots, rest->1, chord->8 microslots
            expectEquals ((int) data->notes[8].size(), 0);            // the rest slot
            expectWithinAbsoluteError (data->beats[8], 4.0, 1.0e-9);   // rest keeps its original beats
            expectEquals (data->displayIndices[8], 1);                 // UI flat index of bar 1 slot 0
        }

        beginTest ("Empty progression: totalChords==0, displayIndices empty");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, std::nullopt);

            PlaybackSettings settings;
            settings.arpPattern = ArpPattern::Up;
            auto data = PlaybackBuilder::build (model, settings);

            expectEquals (data->totalChords, 0);
            expect (data->displayIndices.empty());
            expect (data->notes.empty());
            expect (data->beats.empty());
        }

        beginTest ("cumBeats invariants hold for generated ChordData (regression)");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });

            PlaybackSettings settings;
            settings.arpPattern = ArpPattern::Up;
            settings.arpRate = ArpRate::Sixteenth;

            auto data = PlaybackBuilder::build (model, settings);
            expectEquals ((int) data->cumBeats.size(), data->totalChords + 1);
            expectWithinAbsoluteError (data->cumBeats.back(), data->totalBeats, 1.0e-9);
        }
    }
};

static PlaybackBuilderTest playbackBuilderTest;
