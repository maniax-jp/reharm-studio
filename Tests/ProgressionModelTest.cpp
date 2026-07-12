#include <JuceHeader.h>
#include "../Source/ProgressionModel.h"

using namespace reharm;

class ProgressionModelTest : public juce::UnitTest
{
public:
    ProgressionModelTest()
        : juce::UnitTest ("ProgressionModelTest")
    {
    }

    void runTest() override
    {
        beginTest ("Initial state: numBars=4, totalSlots=4, flatten size=4, all chords nullopt, beats=4.0");
        {
            ProgressionModel model;
            expectEquals (model.getNumBars(), 4);
            expectEquals (model.totalSlots(), 4);

            auto flat = model.flatten();
            expectEquals ((int) flat.size(), 4);
            expectEquals (flat [0].beats, 4.0);

            for (int b = 0; b < 4; ++b)
            {
                for (int s = 0; s < model.getBar (b).numSlots(); ++s)
                {
                    auto chord = model.getChord (b, s);
                    expect (!chord.has_value());
                }
            }
        }

        beginTest ("setChord and getChord round-trip; out-of-range set is ignored");
        {
            ProgressionModel model;
            Chord c {3, ChordQuality::Minor7, -1};
            model.setChord (0, 0, c);
            auto got = model.getChord (0, 0);
            expect (got.has_value());
            expect (*got == c);

            // Out-of-range set should be no-op
            model.setChord (-1, 0, c);
            model.setChord (10, 0, c);
            model.setChord (0, 5, c);
            // bar 0 slot 0 should still be c
            expect (model.getChord (0, 0) == c);
            // out-of-range get should return nullopt
            expect (!model.getChord (-1, 0).has_value());
            expect (!model.getChord (10, 0).has_value());
            expect (!model.getChord (0, 5).has_value());
        }

        beginTest ("setSubdivision to Four: totalSlots=7, flatten[0].beats=1.0");
        {
            ProgressionModel model;
            model.setSubdivision (0, BarSubdivision::Four);
            expectEquals (model.totalSlots(), 7); // 4 + 1 + 1 + 1

            auto flat = model.flatten();
            expectEquals (flat [0].barIndex, 0);
            expectEquals (flat [0].slotIndex, 0);
            expectEquals (flat [0].beats, 1.0);
        }

        beginTest ("Subdivision Four->One preserves slot0 content");
        {
            ProgressionModel model;
            Chord c {7, ChordQuality::Dominant7, -1};
            model.setSubdivision (0, BarSubdivision::Four);
            model.setChord (0, 0, c);
            model.setSubdivision (0, BarSubdivision::One);
            auto got = model.getChord (0, 0);
            expect (got.has_value());
            expect (*got == c);
        }

        beginTest ("setNumBars clamping: 0->1, 9->8");
        {
            ProgressionModel model;
            model.setNumBars (0);
            expectEquals (model.getNumBars(), 1);
            model.setNumBars (9);
            expectEquals (model.getNumBars(), 8);
            model.setNumBars (1);
            expectEquals (model.getNumBars(), 1);
        }

        beginTest ("flatten barIndex and slotIndex correctness");
        {
            ProgressionModel model;
            model.setSubdivision (0, BarSubdivision::Two);
            model.setSubdivision (1, BarSubdivision::Four);

            auto flat = model.flatten();
            // bar0: 2 slots, bar1: 4 slots, bar2: 1 slot, bar3: 1 slot = 8 total
            expectEquals ((int) flat.size(), 8);

            expectEquals (flat [0].barIndex, 0);
            expectEquals (flat [0].slotIndex, 0);
            expectEquals (flat [1].barIndex, 0);
            expectEquals (flat [1].slotIndex, 1);
            expectEquals (flat [2].barIndex, 1);
            expectEquals (flat [2].slotIndex, 0);
            expectEquals (flat [5].barIndex, 1);
            expectEquals (flat [5].slotIndex, 3);
            expectEquals (flat [6].barIndex, 2);
            expectEquals (flat [6].slotIndex, 0);
        }

        beginTest ("onChanged fires on setChord");
        {
            ProgressionModel model;
            int counter = 0;
            model.onChanged = [&counter] { ++counter; };

            model.setChord (0, 0, Chord {0, ChordQuality::Major, -1});
            expectEquals (counter, 1);

            model.setChord (1, 0, Chord {5, ChordQuality::Major, -1});
            expectEquals (counter, 2);
        }

        beginTest ("onChanged nullptr does not crash");
        {
            ProgressionModel model;
            model.onChanged = nullptr;
            model.setChord (0, 0, Chord {0, ChordQuality::Major, -1});
            model.setNumBars (2);
            model.clear();
            model.setKey ({2, false});
            // No crash = pass
            expect (true);
        }

        beginTest ("Presets: all() size and entry counts");
        {
            const auto& presets = ProgressionPresets::all();
            expectEquals ((int) presets.size(), 6);
            expectEquals ((int) presets [0].entries.size(), 4); // Royal Road
            expectEquals ((int) presets [1].entries.size(), 8); // Canon
            expectEquals ((int) presets [2].entries.size(), 4); // Pop Punk
            expectEquals ((int) presets [3].entries.size(), 4); // Komuro
            expectEquals ((int) presets [4].entries.size(), 4); // Marunouchi
            expectEquals ((int) presets [5].entries.size(), 8); // Autumn Leaves
        }

        beginTest ("applyToModel: Royal Road in C major");
        {
            ProgressionModel model;
            model.setKey ({0, true}); // C major

            const auto& presets = ProgressionPresets::all();
            ProgressionPresets::applyToModel (presets [0], model);

            expectEquals (model.getNumBars(), 4);

            auto c0 = model.getChord (0, 0);
            expect (c0.has_value());
            expectEquals (c0->rootPitchClass, 5);
            expectEquals ((int) c0->quality, (int) ChordQuality::Major7);

            auto c3 = model.getChord (3, 0);
            expect (c3.has_value());
            expectEquals (c3->rootPitchClass, 9);
            expectEquals ((int) c3->quality, (int) ChordQuality::Minor);
        }

        beginTest ("applyToModel: Pop Punk in D major (tonic=2)");
        {
            ProgressionModel model;
            model.setKey ({2, true}); // D major

            const auto& presets = ProgressionPresets::all();
            ProgressionPresets::applyToModel (presets [2], model);

            // bar0: I = D major -> root = (2+0)%12 = 2
            auto c0 = model.getChord (0, 0);
            expect (c0.has_value());
            expectEquals (c0->rootPitchClass, 2);

            // bar2: vi = B minor -> root = (2+9)%12 = 11
            auto c2 = model.getChord (2, 0);
            expect (c2.has_value());
            expectEquals (c2->rootPitchClass, 11);
        }

        beginTest ("clear() empties all slots but keeps numBars and subdivisions");
        {
            ProgressionModel model;
            model.setChord (0, 0, Chord {0, ChordQuality::Major, -1});
            model.setSubdivision (1, BarSubdivision::Two);
            model.clear();

            expectEquals (model.getNumBars(), 4);
            expect (model.getSubdivision (1) == BarSubdivision::Two);
            expect (!model.getChord (0, 0).has_value());
            expect (!model.getChord (1, 0).has_value());
            expect (!model.getChord (1, 1).has_value());
        }

        beginTest ("setKey fires onChanged");
        {
            ProgressionModel model;
            int counter = 0;
            model.onChanged = [&counter] { ++counter; };
            model.setKey ({7, false});
            expectEquals (counter, 1);
            expectEquals (model.getKey().tonicPitchClass, 7);
            expect (!model.getKey().isMajor);
        }

        beginTest ("applyToModel: Pop Punk in G minor uses relative major (A#)");
        {
            ProgressionModel model;
            model.setKey ({7, false}); // G minor

            const auto& presets = ProgressionPresets::all();
            ProgressionPresets::applyToModel (presets [2], model); // Pop Punk

            // tonic = (7+3)%12 = 10 (A# major, relative major of G minor)
            // bar0: I = A# major -> root = (10+0)%12 = 10
            auto c0 = model.getChord (0, 0);
            expect (c0.has_value());
            expectEquals (c0->rootPitchClass, 10);
            expectEquals ((int) c0->quality, (int) ChordQuality::Major);

            // bar2: vi = G minor -> root = (10+9)%12 = 7
            auto c2 = model.getChord (2, 0);
            expect (c2.has_value());
            expectEquals (c2->rootPitchClass, 7);
            expectEquals ((int) c2->quality, (int) ChordQuality::Minor);
        }

        beginTest ("applyToModel: Royal Road in A minor uses relative major (C)");
        {
            ProgressionModel model;
            model.setKey ({9, false}); // A minor

            const auto& presets = ProgressionPresets::all();
            ProgressionPresets::applyToModel (presets [0], model); // Royal Road

            // tonic = (9+3)%12 = 0 (C major, relative major of A minor)
            // bar0: IV = F Major7 -> root = (0+5)%12 = 5
            auto c0 = model.getChord (0, 0);
            expect (c0.has_value());
            expectEquals (c0->rootPitchClass, 5);
            expectEquals ((int) c0->quality, (int) ChordQuality::Major7);
        }
    }
};

static ProgressionModelTest progressionModelTest;
