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

        beginTest ("Arp Up (Open): 1 bar div=1, C Major, rate=Eighth -> 8 micro-slots");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C major

            PlaybackSettings settings;
            settings.voicing = Voicing::Style::Open; // Open uses the role-order base
            settings.arpPattern = ArpPattern::Up;
            settings.arpRate = ArpRate::Eighth;

            auto data = PlaybackBuilder::build (model, settings);

            // arpeggioBaseNotes re-stacks in place (octave-independent): C G C' E'.
            const auto base = PlaybackBuilder::arpeggioBaseNotes ({60, 64, 67}, 0);
            expect (base == std::vector<int> ({60, 67, 72, 76}));

            // build() Open runs one octave lower (root C3=48 matches Maj7 Open root).
            expectEquals (data->totalChords, 8);
            const std::vector<int> expectedSeq = { 48, 55, 60, 64, 48, 55, 60, 64 };
            for (int i = 0; i < 8; ++i)
            {
                expectEquals ((int) data->notes[(size_t) i].size(), 1);
                expectEquals (data->notes[(size_t) i][0], expectedSeq[(size_t) i]);
                expectWithinAbsoluteError (data->beats[(size_t) i], 0.5, 1.0e-9);
                expectEquals (data->displayIndices[(size_t) i], 0);
            }
            expectWithinAbsoluteError (data->totalBeats, 4.0, 1.0e-9);
        }

        beginTest ("Arp Down and UpDown: G7 (4 notes) and Power5 (2 notes, DYAD)");
        {
            // Down: G7 -> 4 notes, reverse order
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

            // UpDown with Power5 (DYAD): new 8-note period pattern
            const Chord power5 { 0, ChordQuality::Power5, -1 };
            const auto power5Notes = Voicing::midiNotes (power5, Voicing::Style::Close);
            expectEquals ((int) power5Notes.size(), 2);
            const auto power5Base = PlaybackBuilder::arpeggioBaseNotes (power5Notes, power5.rootPitchClass);
            expectEquals ((int) power5Base.size(), 4);
            // DYAD base: {C, G, C', G'} = {60, 67, 72, 79}
            expect (power5Base == std::vector<int> ({60, 67, 72, 79}));

            ProgressionModel modelUpDown;
            modelUpDown.setNumBars (1);
            modelUpDown.setSubdivision (0, BarSubdivision::One);
            modelUpDown.setChord (0, 0, power5);

            PlaybackSettings upDownSettings;
            upDownSettings.voicing = Voicing::Style::Open; // 8-note period is the Open behaviour
            upDownSettings.arpPattern = ArpPattern::UpDown;
            upDownSettings.arpRate = ArpRate::Eighth; // 4 beats / 0.5 = 8 micro-slots

            auto upDownData = PlaybackBuilder::build (modelUpDown, upDownSettings);
            expectEquals (upDownData->totalChords, 8);
            // New 8-note period at the Open octave base (C3=48): C G C' G' C'' G' C' G
            // = {48,55,60,67,72,67,60,55}. top = C'' = root + 24.
            const std::vector<int> expectedPeriod8 = { 48, 55, 60, 67, 72, 67, 60, 55 };
            for (int i = 0; i < 8; ++i)
                expectEquals (upDownData->notes[(size_t) i][0], expectedPeriod8[(size_t) (i % 8)]);

            // UpDown period-6 check with a 4-note chord (G7): legacy pattern maintained.
            ProgressionModel modelUpDown4;
            modelUpDown4.setNumBars (1);
            modelUpDown4.setSubdivision (0, BarSubdivision::One);
            modelUpDown4.setChord (0, 0, g7);

            PlaybackSettings upDown4Settings;
            upDown4Settings.arpPattern = ArpPattern::UpDown;
            upDown4Settings.arpRate = ArpRate::Sixteenth; // 4 beats / 0.25 = 16 micro-slots

            auto upDown4Data = PlaybackBuilder::build (modelUpDown4, upDown4Settings);
            expectEquals (upDown4Data->totalChords, 16);
            const std::vector<int> expectedPeriod6_4note = { g7Notes[0], g7Notes[1], g7Notes[2], g7Notes[3], g7Notes[2], g7Notes[1] };
            for (int i = 0; i < 16; ++i)
                expectEquals (upDown4Data->notes[(size_t) i][0], expectedPeriod6_4note[(size_t) (i % 6)]);
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

            ProgressionModel modelSixteenth;
            modelSixteenth.setNumBars (1);
            modelSixteenth.setSubdivision (0, BarSubdivision::Four); // 1 beat each
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

        // --- arpeggioBaseNotes helper tests ---
        beginTest ("arpeggioBaseNotes: 2-note {60,67} root=0 -> {60,67,72,79}");
        {
            auto result = PlaybackBuilder::arpeggioBaseNotes ({60, 67}, 0);
            expect (result == std::vector<int> ({60, 67, 72, 79}));
        }

        beginTest ("arpeggioBaseNotes: 2-note {60,64} root=0 -> {60,64,72,76}");
        {
            auto result = PlaybackBuilder::arpeggioBaseNotes ({60, 64}, 0);
            expect (result == std::vector<int> ({60, 64, 72, 76}));
        }

        beginTest ("arpeggioBaseNotes: 3-note Cmaj {60,64,67} root=0 -> {60,67,72,76}");
        {
            auto result = PlaybackBuilder::arpeggioBaseNotes ({60, 64, 67}, 0);
            expect (result == std::vector<int> ({60, 67, 72, 76}));
        }

        beginTest ("arpeggioBaseNotes: 3-note Am {57,60,64} root=9 -> {57,64,69,72}");
        {
            auto result = PlaybackBuilder::arpeggioBaseNotes ({57, 60, 64}, 9);
            expect (result == std::vector<int> ({57, 64, 69, 72}));
        }

        beginTest ("arpeggioBaseNotes: 3-note Fmaj {53,57,60} root=5 -> {53,60,65,69}");
        {
            auto result = PlaybackBuilder::arpeggioBaseNotes ({53, 57, 60}, 5);
            expect (result == std::vector<int> ({53, 60, 65, 69}));
        }

        beginTest ("arpeggioBaseNotes: 4+ notes unchanged");
        {
            auto result = PlaybackBuilder::arpeggioBaseNotes ({60, 64, 67, 70}, 0);
            expect (result == std::vector<int> ({60, 64, 67, 70}));
        }

        beginTest ("arpeggioBaseNotes: 1 note unchanged");
        {
            auto result = PlaybackBuilder::arpeggioBaseNotes ({60}, 0);
            expect (result == std::vector<int> ({60}));
        }

        // --- build() integration tests ---
        beginTest ("build(): Power5 arp up eighth -> 8 slots cycling 4 expanded notes");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Power5, -1 });

            PlaybackSettings settings;
            settings.voicing = Voicing::Style::Close;
            settings.arpPattern = ArpPattern::Up;
            settings.arpRate = ArpRate::Eighth;

            auto data = PlaybackBuilder::build (model, settings);

            const auto baseNotes = Voicing::midiNotes (Chord { 0, ChordQuality::Power5, -1 }, Voicing::Style::Close);
            const std::vector<int> expanded = { baseNotes[0], baseNotes[1], baseNotes[0] + 12, baseNotes[1] + 12 };

            expectEquals (data->totalChords, 8);
            for (int i = 0; i < 8; ++i)
            {
                expectEquals ((int) data->notes[(size_t) i].size(), 1);
                expectEquals (data->notes[(size_t) i][0], expanded[(size_t) (i % 4)]);
                expectWithinAbsoluteError (data->beats[(size_t) i], 0.5, 1.0e-9);
            }
        }

        beginTest ("build(): Power5 arp off -> unchanged 2-note block");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Power5, -1 });

            PlaybackSettings settings;
            settings.voicing = Voicing::Style::Close;
            settings.arpPattern = ArpPattern::Off;

            auto data = PlaybackBuilder::build (model, settings);

            const auto baseNotes = Voicing::midiNotes (Chord { 0, ChordQuality::Power5, -1 }, Voicing::Style::Close);
            expectEquals (data->totalChords, 1);
            expect (data->notes[0] == baseNotes);
            expectWithinAbsoluteError (data->beats[0], 4.0, 1.0e-9);
        }

        beginTest ("build(): TRIAD arp updown (Open) -> new 8-note period pattern");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });

            PlaybackSettings settings;
            settings.voicing = Voicing::Style::Open; // role order + 8-note period
            settings.arpPattern = ArpPattern::UpDown;
            settings.arpRate = ArpRate::Eighth;

            auto data = PlaybackBuilder::build (model, settings);

            // Open runs at octave base C3=48: C G C' E' C'' E' C' G.
            // 8-note UpDown = {48,55,60,64,72,64,60,55}; C'' = top = root + 24 = 72.
            const std::vector<int> cycle = { 48, 55, 60, 64, 72, 64, 60, 55 };

            expectEquals (data->totalChords, 8);
            for (int i = 0; i < 8; ++i)
            {
                expectEquals ((int) data->notes[(size_t) i].size(), 1);
                expectEquals (data->notes[(size_t) i][0], cycle[(size_t) (i % 8)]);
                expectWithinAbsoluteError (data->beats[(size_t) i], 0.5, 1.0e-9);
            }
        }

        beginTest ("build(): DYAD/TRIAD arpeggio - Close ascending vs Open role order");
        {
            // Helper: flatten the first `n` single-note engine slots to a vector.
            auto firstN = [] (const std::shared_ptr<ChordData>& d, int n)
            {
                std::vector<int> v;
                for (int i = 0; i < n && i < d->totalChords; ++i)
                    v.push_back (d->notes[(size_t) i][0]);
                return v;
            };
            auto run = [&] (ChordQuality q, ArpPattern pat, Voicing::Style v)
            {
                ProgressionModel m; m.setNumBars (1); m.setSubdivision (0, BarSubdivision::One);
                m.setChord (0, 0, Chord { 0, q, -1 });
                PlaybackSettings s { v, pat, ArpRate::Eighth };
                return PlaybackBuilder::build (m, s);
            };

            // Close runs at octave base C4=60; Open runs one octave lower (C3=48).
            // --- DYAD (C5) ---
            // Up/Down re-stack to C G C' G' in both, but Open is an octave lower.
            expect (firstN (run (ChordQuality::Power5, ArpPattern::Up,   Voicing::Style::Close), 4)
                    == std::vector<int> ({ 60, 67, 72, 79 }));
            expect (firstN (run (ChordQuality::Power5, ArpPattern::Up,   Voicing::Style::Open),  4)
                    == std::vector<int> ({ 48, 55, 60, 67 }));
            // UpDown differs: Close = legacy period-6 (n0..n3,n2,n1), Open = 8-note period.
            expect (firstN (run (ChordQuality::Power5, ArpPattern::UpDown, Voicing::Style::Close), 6)
                    == std::vector<int> ({ 60, 67, 72, 79, 72, 67 }));
            expect (firstN (run (ChordQuality::Power5, ArpPattern::UpDown, Voicing::Style::Open),  8)
                    == std::vector<int> ({ 48, 55, 60, 67, 72, 67, 60, 55 }));

            // --- TRIAD (Cmaj) ---
            // Close = ascending octave-fill C E G C'. Open = role order C G C' E' (octave lower).
            expect (firstN (run (ChordQuality::Major, ArpPattern::Up, Voicing::Style::Close), 4)
                    == std::vector<int> ({ 60, 64, 67, 72 }));
            expect (firstN (run (ChordQuality::Major, ArpPattern::Up, Voicing::Style::Open),  4)
                    == std::vector<int> ({ 48, 55, 60, 64 }));
            // UpDown: Close legacy period-6 over C E G C'; Open 8-note period (octave lower).
            expect (firstN (run (ChordQuality::Major, ArpPattern::UpDown, Voicing::Style::Close), 6)
                    == std::vector<int> ({ 60, 64, 67, 72, 67, 64 }));
            expect (firstN (run (ChordQuality::Major, ArpPattern::UpDown, Voicing::Style::Open),  8)
                    == std::vector<int> ({ 48, 55, 60, 64, 72, 64, 60, 55 }));
        }

        beginTest ("build(): 4-note arpeggio honours voicing (Open != Close)");
        {
            // For 4+ note chords the arpeggio walks the voiced notes as-is, so the
            // Close/Open setting DOES apply (Open must differ from Close).
            for (auto quality : { ChordQuality::Major7, ChordQuality::Dominant7,
                                  ChordQuality::Minor7 })
            {
                ProgressionModel model;
                model.setNumBars (1);
                model.setSubdivision (0, BarSubdivision::One);
                model.setChord (0, 0, Chord { 0, quality, -1 });

                PlaybackSettings closeS { Voicing::Style::Close, ArpPattern::Up, ArpRate::Eighth };
                PlaybackSettings openS  { Voicing::Style::Open,  ArpPattern::Up, ArpRate::Eighth };

                auto closeData = PlaybackBuilder::build (model, closeS);
                auto openData  = PlaybackBuilder::build (model, openS);

                bool anyDiff = false;
                const int n = juce::jmin (openData->totalChords, closeData->totalChords);
                for (int i = 0; i < n; ++i)
                    if (openData->notes[(size_t) i] != closeData->notes[(size_t) i])
                        anyDiff = true;
                expect (anyDiff);
            }
        }

        beginTest ("build(): Major7 (4 notes) arp updown -> legacy period-6 pattern");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major7, -1 });

            PlaybackSettings settings;
            settings.voicing = Voicing::Style::Close;
            settings.arpPattern = ArpPattern::UpDown;
            settings.arpRate = ArpRate::Sixteenth;

            auto data = PlaybackBuilder::build (model, settings);

            const auto chordNotes = Voicing::midiNotes (Chord { 0, ChordQuality::Major7, -1 }, Voicing::Style::Close);
            expectEquals ((int) chordNotes.size(), 4);

            // Legacy period-6: n0 n1 n2 n3 n2 n1
            const std::vector<int> cycle = { chordNotes[0], chordNotes[1], chordNotes[2], chordNotes[3],
                                              chordNotes[2], chordNotes[1] };

            expectEquals (data->totalChords, 16);
            for (int i = 0; i < 16; ++i)
            {
                expectEquals ((int) data->notes[(size_t) i].size(), 1);
                expectEquals (data->notes[(size_t) i][0], cycle[(size_t) (i % 6)]);
                expectWithinAbsoluteError (data->beats[(size_t) i], 0.25, 1.0e-9);
            }
        }

        beginTest ("build(): TRIAD arp down (Open) -> reverse of role-order base [E', C', G, C]");
        {
            ProgressionModel model;
            model.setNumBars (1);
            model.setSubdivision (0, BarSubdivision::One);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 });

            PlaybackSettings settings;
            settings.voicing = Voicing::Style::Open; // role-order base is the Open behaviour
            settings.arpPattern = ArpPattern::Down;
            settings.arpRate = ArpRate::Quarter;

            auto data = PlaybackBuilder::build (model, settings);

            // Open base at C3=48: [C, G, C', E'] = [48, 55, 60, 64]
            // Down: [E', C', G, C] = [64, 60, 55, 48]
            const std::vector<int> expectedReverse = { 64, 60, 55, 48 };

            expectEquals (data->totalChords, 4);
            for (int i = 0; i < 4; ++i)
            {
                expectEquals ((int) data->notes[(size_t) i].size(), 1);
                expectEquals (data->notes[(size_t) i][0], expectedReverse[(size_t) i]);
                expectWithinAbsoluteError (data->beats[(size_t) i], 1.0, 1.0e-9);
            }
        }
    }
};

static PlaybackBuilderTest playbackBuilderTest;
