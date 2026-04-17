#include <JuceHeader.h>
#include "../Source/ChordTheory.h"
#include <vector>

class ChordTheoryTest : public juce::UnitTest
{
public:
    ChordTheoryTest() : juce::UnitTest ("ChordTheoryTest") {}

    void runTest() override
    {
        beginTest ("Major Scale Chords (C Major)");
        {
            // I: C, E, G (60, 64, 67)
            auto notes = ChordTheory::getChordNotes ("I", 0, true);
            expect (notes == std::vector<int> {60, 64, 67});

            // IV: F, A, C (65, 69, 72)
            notes = ChordTheory::getChordNotes ("IV", 0, true);
            expect (notes == std::vector<int> {65, 69, 72});

            // V: G, B, D (67, 71, 74)
            notes = ChordTheory::getChordNotes ("V", 0, true);
            expect (notes == std::vector<int> {67, 71, 74});
        }

        beginTest ("Minor Scale Chords (A Minor)");
        {
            // i: A, C, E (60+9=69, 69+3=72, 69+7=76) -> A=9
            auto notes = ChordTheory::getChordNotes ("i", 9, false);
            expect (notes == std::vector<int> {69, 72, 76});

            // iv: D, F, A (60+9+5=74, 74+3=77, 74+7=81)
            notes = ChordTheory::getChordNotes ("iv", 9, false);
            expect (notes == std::vector<int> {74, 77, 81});
        }

        beginTest ("Root Note Shift (G Major)");
        {
            // I: G, B, D (60+7=67, 67+4=71, 67+7=74)
            auto notes = ChordTheory::getChordNotes ("I", 7, true);
            expect (notes == std::vector<int> {67, 71, 74});
        }

        beginTest ("Unknown Symbol");
        {
            auto notes = ChordTheory::getChordNotes ("Unknown", 0, true);
            expect (notes.empty());
        }
    }
};

static ChordTheoryTest chordTheoryTest;
