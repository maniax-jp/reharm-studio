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
        beginTest ("Intervals for all qualities");
        {
            expect (ChordModel::intervals (ChordQuality::Power5)          == std::vector<int> ({ 0, 7 }));
            expect (ChordModel::intervals (ChordQuality::Major)           == std::vector<int> ({ 0, 4, 7 }));
            expect (ChordModel::intervals (ChordQuality::Minor)           == std::vector<int> ({ 0, 3, 7 }));
            expect (ChordModel::intervals (ChordQuality::Sus2)            == std::vector<int> ({ 0, 2, 7 }));
            expect (ChordModel::intervals (ChordQuality::Sus4)            == std::vector<int> ({ 0, 5, 7 }));
            expect (ChordModel::intervals (ChordQuality::Augmented)       == std::vector<int> ({ 0, 4, 8 }));
            expect (ChordModel::intervals (ChordQuality::Diminished)      == std::vector<int> ({ 0, 3, 6 }));
            expect (ChordModel::intervals (ChordQuality::Sixth)           == std::vector<int> ({ 0, 4, 7, 9 }));
            expect (ChordModel::intervals (ChordQuality::Minor6)          == std::vector<int> ({ 0, 3, 7, 9 }));
            expect (ChordModel::intervals (ChordQuality::Dominant7)       == std::vector<int> ({ 0, 4, 7, 10 }));
            expect (ChordModel::intervals (ChordQuality::Major7)          == std::vector<int> ({ 0, 4, 7, 11 }));
            expect (ChordModel::intervals (ChordQuality::Minor7)          == std::vector<int> ({ 0, 3, 7, 10 }));
            expect (ChordModel::intervals (ChordQuality::MinorMajor7)     == std::vector<int> ({ 0, 3, 7, 11 }));
            expect (ChordModel::intervals (ChordQuality::Dominant7Sus2)   == std::vector<int> ({ 0, 2, 7, 10 }));
            expect (ChordModel::intervals (ChordQuality::Dominant7Sus4)   == std::vector<int> ({ 0, 5, 7, 10 }));
            expect (ChordModel::intervals (ChordQuality::Major7Sus2)      == std::vector<int> ({ 0, 2, 7, 11 }));
            expect (ChordModel::intervals (ChordQuality::Major7Sus4)      == std::vector<int> ({ 0, 5, 7, 11 }));
            expect (ChordModel::intervals (ChordQuality::Minor7Flat5)     == std::vector<int> ({ 0, 3, 6, 10 }));
            expect (ChordModel::intervals (ChordQuality::Diminished7)     == std::vector<int> ({ 0, 3, 6, 9 }));
            expect (ChordModel::intervals (ChordQuality::Augmented7)      == std::vector<int> ({ 0, 4, 8, 10 }));
            expect (ChordModel::intervals (ChordQuality::AugmentedMajor7) == std::vector<int> ({ 0, 4, 8, 11 }));
            expect (ChordModel::intervals (ChordQuality::Blackadder)      == std::vector<int> ({ 0, 2, 6, 10 }));
        }

        beginTest ("effectiveIntervals");
        {
            Chord c7add9 { 0, ChordQuality::Dominant7 };
            c7add9.addMask = AddNine;
            expect (ChordModel::effectiveIntervals (c7add9) == std::vector<int> ({ 0, 4, 7, 10, 14 }));

            Chord cOmit5 { 0, ChordQuality::Major };
            cOmit5.omitMask = Omit5;
            expect (ChordModel::effectiveIntervals (cOmit5) == std::vector<int> ({ 0, 4 }));

            Chord c7b5 { 0, ChordQuality::Dominant7 };
            c7b5.fifthAlt = FifthAlt::Flat;
            expect (ChordModel::effectiveIntervals (c7b5) == std::vector<int> ({ 0, 4, 6, 10 }));

            Chord c7omit5adds { 0, ChordQuality::Dominant7 };
            c7omit5adds.omitMask = Omit5;
            c7omit5adds.addMask = AddFlat9 | AddThirteen;
            expect (ChordModel::effectiveIntervals (c7omit5adds) == std::vector<int> ({ 0, 4, 10, 13, 21 }));
        }

        beginTest ("chordName");
        {
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Major }), juce::String ("C"));
            expectEquals (ChordModel::chordName ({ 9, ChordQuality::Minor }), juce::String ("Am"));
            expectEquals (ChordModel::chordName ({ 7, ChordQuality::Dominant7 }), juce::String ("G7"));
            expectEquals (ChordModel::chordName ({ 10, ChordQuality::Major7 }, true), juce::String ("BbM7"));
            expectEquals (ChordModel::chordName ({ 6, ChordQuality::Minor7Flat5 }), juce::String ("F#m7b5"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::AugmentedMajor7 }), juce::String ("CaugM7"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Blackadder }), juce::String ("Cblk"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Power5 }), juce::String ("C5"));
            expectEquals (ChordModel::chordName ({ 0, ChordQuality::Major, 4 }), juce::String ("C/E"));
            expectEquals (ChordModel::chordName ({ 2, ChordQuality::Minor7, 7 }), juce::String ("Dm7/G"));

            Chord fAdd9 { 5, ChordQuality::Major };
            fAdd9.addMask = AddNine;
            expectEquals (ChordModel::chordName (fAdd9), juce::String ("Fadd9"));

            Chord f9 { 5, ChordQuality::Dominant7 };
            f9.addMask = AddNine;
            expectEquals (ChordModel::chordName (f9), juce::String ("F9"));

            Chord c69 { 0, ChordQuality::Sixth };
            c69.addMask = AddNine;
            expectEquals (ChordModel::chordName (c69), juce::String ("C69"));

            Chord c13 { 0, ChordQuality::Dominant7 };
            c13.addMask = AddNine | AddThirteen;
            expectEquals (ChordModel::chordName (c13), juce::String ("C13"));

            Chord c7b913 { 0, ChordQuality::Dominant7 };
            c7b913.addMask = AddFlat9 | AddThirteen;
            expectEquals (ChordModel::chordName (c7b913), juce::String ("C7(b9,13)"));

            Chord cOmit5 { 0, ChordQuality::Major };
            cOmit5.omitMask = Omit5;
            expectEquals (ChordModel::chordName (cOmit5), juce::String ("C(omit5)"));

            Chord f9omit5 { 5, ChordQuality::Dominant7 };
            f9omit5.addMask = AddNine;
            f9omit5.omitMask = Omit5;
            expectEquals (ChordModel::chordName (f9omit5), juce::String ("F9(omit5)"));

            Chord c7sharp5 { 0, ChordQuality::Dominant7 };
            c7sharp5.fifthAlt = FifthAlt::Sharp;
            expectEquals (ChordModel::chordName (c7sharp5), juce::String ("C7(#5)"));
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

        beginTest ("optionAvailability");
        {
            {
                const auto a = ChordModel::optionAvailability ({ 0, ChordQuality::Sus2 });
                expect (! a.add[1]); // 9 collides with sus2
                expect (! a.omit3);
            }
            {
                const auto a = ChordModel::optionAvailability ({ 0, ChordQuality::Minor });
                expect (! a.add[2]); // #9 collides with b3
            }
            {
                const auto a = ChordModel::optionAvailability ({ 0, ChordQuality::Sixth });
                expect (! a.add[6]); // 13 collides with 6
                expect (! a.omit7);
            }
            {
                const auto a = ChordModel::optionAvailability ({ 0, ChordQuality::Power5 });
                expect (! a.omit3);
                expect (! a.omit5);
            }
            {
                const auto a = ChordModel::optionAvailability ({ 0, ChordQuality::Dominant7 });
                expect (a.add[0]); // b9
                expect (a.add[2]); // #9
            }
            {
                Chord c7add9 { 0, ChordQuality::Dominant7 };
                c7add9.addMask = AddNine;
                const auto a = ChordModel::optionAvailability (c7add9);
                expect (a.add[1]);  // already selected
                expect (! a.add[0]); // b9 exclusive with 9
                expect (! a.add[2]); // #9 exclusive with 9
            }
            {
                Chord c7omit5 { 0, ChordQuality::Dominant7 };
                c7omit5.omitMask = Omit5;
                const auto a = ChordModel::optionAvailability (c7omit5);
                expect (! a.flat5);
                expect (! a.sharp5);
            }
        }

        beginTest ("sanitizeOptions");
        {
            Chord c { 0, ChordQuality::Major };
            c.addMask = AddNine;
            c.quality = ChordQuality::Sus2;
            ChordModel::sanitizeOptions (c);
            expectEquals (c.addMask, 0);
        }

        beginTest ("operator== options");
        {
            Chord a { 0, ChordQuality::Major };
            Chord b { 0, ChordQuality::Major };
            expect (a == b);

            b.omitMask = Omit5;
            expect (a != b);

            b.omitMask = 0;
            b.addMask = AddNine;
            expect (a != b);

            b.addMask = 0;
            b.fifthAlt = FifthAlt::Flat;
            expect (a != b);
        }

        beginTest ("Voicing Close C major and C/E");
        {
            const auto cMaj = Voicing::midiNotes ({ 0, ChordQuality::Major }, Voicing::Style::Close, 60);
            expect (cMaj == std::vector<int> ({ 60, 64, 67 }));

            const auto cOverE = Voicing::midiNotes ({ 0, ChordQuality::Major, 4 }, Voicing::Style::Close, 60);
            expect (cOverE == std::vector<int> ({ 52, 60, 64, 67 }));
        }

        beginTest ("Voicing Close Power5 and Dominant7 add9");
        {
            const auto c5 = Voicing::midiNotes ({ 0, ChordQuality::Power5 }, Voicing::Style::Close, 60);
            expect (c5 == std::vector<int> ({ 60, 67 }));

            Chord c9 { 0, ChordQuality::Dominant7 };
            c9.addMask = AddNine;
            const auto notes = Voicing::midiNotes (c9, Voicing::Style::Close, 60);
            expect (notes == std::vector<int> ({ 60, 64, 67, 70, 74 }));
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
