#include <JuceHeader.h>
#include "../Source/ChordModel.h"
#include <vector>

using namespace reharm;

class ChordModelTest : public juce::UnitTest
{
public:
    ChordModelTest() : juce::UnitTest ("ChordModelTest") {}

    void runTest() override
    {
        beginTest ("Intervals for all 22 qualities");
        {
            expect (ChordModel::intervals (ChordQuality::Major)           == std::vector<int> ({ 0, 4, 7 }));
            expect (ChordModel::intervals (ChordQuality::Minor)           == std::vector<int> ({ 0, 3, 7 }));
            expect (ChordModel::intervals (ChordQuality::Dominant7)       == std::vector<int> ({ 0, 4, 7, 10 }));
            expect (ChordModel::intervals (ChordQuality::Major7)          == std::vector<int> ({ 0, 4, 7, 11 }));
            expect (ChordModel::intervals (ChordQuality::Minor7)          == std::vector<int> ({ 0, 3, 7, 10 }));
            expect (ChordModel::intervals (ChordQuality::MinorMajor7)     == std::vector<int> ({ 0, 3, 7, 11 }));
            expect (ChordModel::intervals (ChordQuality::Add9)            == std::vector<int> ({ 0, 4, 7, 14 }));
            expect (ChordModel::intervals (ChordQuality::Sus2)            == std::vector<int> ({ 0, 2, 7 }));
            expect (ChordModel::intervals (ChordQuality::Sus4)            == std::vector<int> ({ 0, 5, 7 }));
            expect (ChordModel::intervals (ChordQuality::Augmented)       == std::vector<int> ({ 0, 4, 8 }));
            expect (ChordModel::intervals (ChordQuality::Diminished)      == std::vector<int> ({ 0, 3, 6 }));
            expect (ChordModel::intervals (ChordQuality::Dominant9)       == std::vector<int> ({ 0, 4, 7, 10, 14 }));
            expect (ChordModel::intervals (ChordQuality::Sixth)           == std::vector<int> ({ 0, 4, 7, 9 }));
            expect (ChordModel::intervals (ChordQuality::Add11)           == std::vector<int> ({ 0, 4, 7, 17 }));
            expect (ChordModel::intervals (ChordQuality::Add13)           == std::vector<int> ({ 0, 4, 7, 21 }));
            expect (ChordModel::intervals (ChordQuality::Major7Sus4)      == std::vector<int> ({ 0, 5, 7, 11 }));
            expect (ChordModel::intervals (ChordQuality::Dominant7Sus4)   == std::vector<int> ({ 0, 5, 7, 10 }));
            expect (ChordModel::intervals (ChordQuality::Minor7Flat5)     == std::vector<int> ({ 0, 3, 6, 10 }));
            expect (ChordModel::intervals (ChordQuality::Diminished7)     == std::vector<int> ({ 0, 3, 6, 9 }));
            expect (ChordModel::intervals (ChordQuality::Augmented7)      == std::vector<int> ({ 0, 4, 8, 10 }));
            expect (ChordModel::intervals (ChordQuality::AugmentedMajor7) == std::vector<int> ({ 0, 4, 8, 11 }));
            expect (ChordModel::intervals (ChordQuality::Blackadder)      == std::vector<int> ({ 0, 2, 6, 10 }));
        }

        beginTest ("chordName");
        {
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Major }), juce::String ("C"));
            expectEquals (ChordModel::chordName ({ 9, ChordQuality::Minor }), juce::String ("Am"));
            expectEquals (ChordModel::chordName ({ 7, ChordQuality::Dominant7 }), juce::String ("G7"));
            expectEquals (ChordModel::chordName ({ 10, ChordQuality::Major7 }, true), juce::String ("BbM7"));
            expectEquals (ChordModel::chordName ({ 6, ChordQuality::Minor7Flat5 }), juce::String ("F#m7b5"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Add9 }), juce::String ("Cadd9"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::AugmentedMajor7 }), juce::String ("CaugM7"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Blackadder }), juce::String ("Cblk"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Major, 4 }), juce::String ("C/E"));
            expectEquals (ChordModel::chordName ({ 2, ChordQuality::Minor7, 7 }), juce::String ("Dm7/G"));
        }

        beginTest ("degreeName in C major");
        {
            const KeyContext cMajor { 0, true };

            expectEquals (ChordModel::degreeName ({ 5, ChordQuality::Major }, cMajor), juce::String ("IV"));
            expectEquals (ChordModel::degreeName ({ 7, ChordQuality::Dominant7 }, cMajor), juce::String ("V7"));
            expectEquals (ChordModel::degreeName ({ 2, ChordQuality::Minor7 }, cMajor), juce::String ("IIm7"));
            expectEquals (ChordModel::degreeName ({ 8, ChordQuality::Major }, cMajor), juce::String ("bVI"));
            expectEquals (ChordModel::degreeName ({ 11, ChordQuality::Minor7Flat5 }, cMajor), juce::String ("VIIm7b5"));
            expectEquals (ChordModel::degreeName ({ 0, ChordQuality::Major, 4 }, cMajor), juce::String ("I/III"));
        }

        beginTest ("degreeName in A minor");
        {
            const KeyContext aMinor { 9, false };

            expectEquals (ChordModel::degreeName ({ 9, ChordQuality::Minor }, aMinor), juce::String ("Im"));
            // G (7) relative to A (9): offset = ((7-9)%12+12)%12 = 10 -> bVII
            expectEquals (ChordModel::degreeName ({ 7, ChordQuality::Major }, aMinor), juce::String ("bVII"));
        }

        beginTest ("isDiatonic");
        {
            const KeyContext cMajor { 0, true };
            const KeyContext aMinor { 9, false };

            expect (ChordModel::isDiatonic ({ 0, ChordQuality::Major }, cMajor));
            expect (ChordModel::isDiatonic ({ 2, ChordQuality::Minor7 }, cMajor));
            expect (ChordModel::isDiatonic ({ 7, ChordQuality::Dominant7 }, cMajor));
            expect (ChordModel::isDiatonic ({ 11, ChordQuality::Minor7Flat5 }, cMajor));

            expect (! ChordModel::isDiatonic ({ 5, ChordQuality::Minor }, cMajor));       // Fm
            expect (! ChordModel::isDiatonic ({ 4, ChordQuality::Dominant7 }, cMajor));    // E7 (D#)
            expect (! ChordModel::isDiatonic ({ 3, ChordQuality::Diminished }, cMajor));  // Ebdim

            expect (ChordModel::isDiatonic ({ 9, ChordQuality::Minor }, aMinor));  // Am
            expect (ChordModel::isDiatonic ({ 2, ChordQuality::Minor }, aMinor));  // Dm
            expect (ChordModel::isDiatonic ({ 7, ChordQuality::Major }, aMinor));  // G
            expect (! ChordModel::isDiatonic ({ 4, ChordQuality::Dominant7 }, aMinor)); // E7 has G#
        }

        beginTest ("Voicing Close C major and C/E");
        {
            const auto cMaj = Voicing::midiNotes ({ 0, ChordQuality::Major }, Voicing::Style::Close, 60);
            expect (cMaj == std::vector<int> ({ 60, 64, 67 }));

            const auto cOverE = Voicing::midiNotes ({ 0, ChordQuality::Major, 4 }, Voicing::Style::Close, 60);
            expect (cOverE == std::vector<int> ({ 52, 60, 64, 67 }));
        }

        beginTest ("Voicing Open C major triad");
        {
            // close={60,64,67}; size<=3 so drop index1 -> {60,52,67}; bass root-12=48; sort
            const auto notes = Voicing::midiNotes ({ 0, ChordQuality::Major }, Voicing::Style::Open, 60);
            expect (notes == std::vector<int> ({ 48, 52, 60, 67 }));
        }

        beginTest ("Voicing Open CM7");
        {
            // close={60,64,67,71}; drop second-highest (67) -> 55; bass 48; sort
            const auto notes = Voicing::midiNotes ({ 0, ChordQuality::Major7 }, Voicing::Style::Open, 60);
            expect (notes == std::vector<int> ({ 48, 55, 60, 64, 71 }));
        }

        beginTest ("pitchClassName normalizes negative input");
        {
            expectEquals (ChordModel::pitchClassName (-1), juce::String ("B"));
            expectEquals (ChordModel::pitchClassName (-13), juce::String ("B"));
            expectEquals (ChordModel::pitchClassName (12), juce::String ("C"));
        }
    }
};

static ChordModelTest chordModelTest;
