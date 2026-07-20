#include <JuceHeader.h>
#include "../Source/HarmonyAnalyzer.h"

using namespace reharm;

class HarmonyAnalyzerTest : public juce::UnitTest
{
public:
    HarmonyAnalyzerTest()
        : juce::UnitTest ("HarmonyAnalyzerTest")
    {
    }

    void runTest() override
    {
        const KeyContext cMajor { 0, true };
        const KeyContext aMinor { 9, false };

        beginTest ("Diatonic chords in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (7);

            // C, Dm7, Em7, FM7, G7, Am7, Bm7b5
            model.setChord (0, 0, Chord { 0,  ChordQuality::Major,        -1 });
            model.setChord (1, 0, Chord { 2,  ChordQuality::Minor7,       -1 });
            model.setChord (2, 0, Chord { 4,  ChordQuality::Minor7,       -1 });
            model.setChord (3, 0, Chord { 5,  ChordQuality::Major7,       -1 });
            model.setChord (4, 0, Chord { 7,  ChordQuality::Dominant7,    -1 });
            model.setChord (5, 0, Chord { 9,  ChordQuality::Minor7,       -1 });
            model.setChord (6, 0, Chord { 11, ChordQuality::Minor7Flat5,  -1 });

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 7);

            for (const auto& a : analysis)
            {
                expect (a.diatonic);
                expect (a.technique == NonDiatonicTechnique::None);
                expect (a.label.isEmpty());
            }
        }

        beginTest ("SecondaryDominant: A7 resolving to Dm7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (1, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[0].label.contains ("V7/II"));
        }

        beginTest ("SecondaryDominant: E7 to FM7 (III7 -> IV)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (1, 0, Chord { 5, ChordQuality::Major7,    -1 }); // FM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/VI)"));
        }

        beginTest ("SecondaryDominant: A7 to FM7 (VI7 -> IV)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (1, 0, Chord { 5, ChordQuality::Major7,    -1 }); // FM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/II)"));
        }

        beginTest ("SecondaryDominant: E7 to Am7 (fifth resolution regression)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor7,    -1 }); // Am7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/VI)"));
        }

        beginTest ("SecondaryDominant: E7 to G7 is not Sec.Dom");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (analysis[0].technique != NonDiatonicTechnique::SecondaryDominant);
        }


        beginTest ("SecondaryDominant: final A7 in C major (no next chord)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 }); // C
            model.setChord (1, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[1].label.contains ("V7/II"));
        }

        beginTest ("SecondaryDominant: final Db7 in C major -> TritoneSub");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 }); // C
            model.setChord (1, 0, Chord { 1, ChordQuality::Dominant7, -1 }); // Db7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique != NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[1].technique == NonDiatonicTechnique::TritoneSubstitution);
        }

        beginTest ("Regression: final G7 in C major stays diatonic");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (analysis[1].diatonic);
        }

        beginTest ("SecondaryDominant: final B7 in A minor (no next chord)");
        {
            ProgressionModel model;
            model.setKey (KeyContext { 9, false });
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am
            model.setChord (1, 0, Chord { 11, ChordQuality::Dominant7, -1 }); // B7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
        }
        beginTest ("DoubleDominant: D7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (1);
            model.setChord (0, 0, Chord { 2, ChordQuality::Dominant7, -1 }); // D7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 1);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::DoubleDominant);
            expectEquals (analysis[0].label, juce::String ("Dbl.Dom"));
        }

        beginTest ("TritoneSubstitution: Db7 to C");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 1, ChordQuality::Dominant7, -1 }); // Db7
            model.setChord (1, 0, Chord { 0, ChordQuality::Major,     -1 }); // C

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::TritoneSubstitution);
            expectEquals (analysis[0].label, juce::String ("Tritone Sub"));
        }

        beginTest ("SubdominantMinor: Fm in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (1);
            model.setChord (0, 0, Chord { 5, ChordQuality::Minor, -1 }); // Fm

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SubdominantMinor);
            expectEquals (analysis[0].label, juce::String ("Subdom.Minor"));
            expectEquals (analysis[0].borrowedScale, juce::String ("C Natural minor"));
        }

        beginTest ("PassingDiminished: C - C#dim7 - Dm7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,       -1 }); // C
            model.setChord (1, 0, Chord { 1, ChordQuality::Diminished7, -1 }); // C#dim7
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,      -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::PassingDiminished);
            expectEquals (analysis[1].label, juce::String ("Pass.Dim"));
        }

        beginTest ("PassingDiminished descending: Am - D#dim - Dm7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor,      -1 }); // Am
            model.setChord (1, 0, Chord { 3, ChordQuality::Diminished, -1 }); // D#dim
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,     -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::PassingDiminished);
            expectEquals (analysis[1].label, juce::String ("Pass.Dim"));
        }

        beginTest ("PassingDiminished descending: Am - D#dim7 - Dm7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor,       -1 }); // Am
            model.setChord (1, 0, Chord { 3, ChordQuality::Diminished7, -1 }); // D#dim7
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,      -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::PassingDiminished);
            expectEquals (analysis[1].label, juce::String ("Pass.Dim"));
        }

        beginTest ("PassingDiminished negative: C - F#dim - C is not Pass.Dim");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,      -1 }); // C
            model.setChord (1, 0, Chord { 6, ChordQuality::Diminished, -1 }); // F#dim
            model.setChord (2, 0, Chord { 0, ChordQuality::Major,      -1 }); // C

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[1].technique != NonDiatonicTechnique::PassingDiminished);
        }

        beginTest ("CommonToneDiminished: C - Cdim7 - C");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,       -1 }); // C
            model.setChord (1, 0, Chord { 0, ChordQuality::Diminished7, -1 }); // Cdim7
            model.setChord (2, 0, Chord { 0, ChordQuality::Major,       -1 }); // C

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::CommonToneDiminished);
            expectEquals (analysis[1].label, juce::String ("C.T.Dim"));
        }

        beginTest ("CommonToneDiminished: C - Cdim - CM7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,      -1 }); // C
            model.setChord (1, 0, Chord { 0, ChordQuality::Diminished, -1 }); // Cdim
            model.setChord (2, 0, Chord { 0, ChordQuality::Major7,     -1 }); // CM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::CommonToneDiminished);
            expectEquals (analysis[1].label, juce::String ("C.T.Dim"));
        }

        beginTest ("CommonToneDiminished trailing: C - Cdim7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,       -1 }); // C
            model.setChord (1, 0, Chord { 0, ChordQuality::Diminished7, -1 }); // Cdim7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::CommonToneDiminished);
            expectEquals (analysis[1].label, juce::String ("C.T.Dim"));
        }

        beginTest ("CommonToneDiminished regression: C - C#dim7 - Dm7 stays Pass.Dim");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,       -1 }); // C
            model.setChord (1, 0, Chord { 1, ChordQuality::Diminished7, -1 }); // C#dim7
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,      -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[1].technique == NonDiatonicTechnique::PassingDiminished);
            expect (analysis[1].technique != NonDiatonicTechnique::CommonToneDiminished);
            expectEquals (analysis[1].label, juce::String ("Pass.Dim"));
        }

        beginTest ("BluesSeventh: C7 -> G7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 0, ChordQuality::Dominant7, -1 }); // C7
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::BluesSeventh);
            expectEquals (analysis[0].label, juce::String ("Blues 7th"));
        }

        beginTest ("BluesSeventh: F7 -> C in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 5, ChordQuality::Dominant7, -1 }); // F7
            model.setChord (1, 0, Chord { 0, ChordQuality::Major,       -1 }); // C

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::BluesSeventh);
        }

        beginTest ("BluesSeventh regression: C7 -> F in C major stays Sec.Dom");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 0, ChordQuality::Dominant7, -1 }); // C7
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,       -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[0].label.contains ("V7/IV"));
        }

        beginTest ("BluesSeventh regression: F7 -> Bb in C major stays Sec.Dom");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 5, ChordQuality::Dominant7, -1 }); // F7
            model.setChord (1, 0, Chord { 10, ChordQuality::Major,     -1 }); // Bb

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
        }

        beginTest ("RelatedTwoMinor: Em7b5 - A7 - Dm7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 4, ChordQuality::Minor7Flat5, -1 }); // Em7b5
            model.setChord (1, 0, Chord { 9, ChordQuality::Dominant7,   -1 }); // A7
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,      -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::RelatedTwoMinor);
            expectEquals (analysis[0].label, juce::String ("Rel.IIm"));
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
        }

        beginTest ("PicardyThird: final A major in A minor");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor, -1 }); // Dm
            model.setChord (1, 0, Chord { 9, ChordQuality::Major, -1 }); // A (Picardy)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::PicardyThird);
            expectEquals (analysis[1].label, juce::String ("Picardy"));
        }

        beginTest ("Minor-key V7 ModalInterchange (E7 in Am)");
        {
            // E7 -> F (not tonic): harmonic-minor V7 path
            {
                ProgressionModel model;
                model.setKey (aMinor);
                model.setNumBars (2);
                model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
                model.setChord (1, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

                const auto analysis = HarmonyAnalyzer::analyzeAll (model);
                expect (! analysis[0].diatonic);
                expect (analysis[0].technique == NonDiatonicTechnique::ModalInterchange);
                expectEquals (analysis[0].label, juce::String ("V7"));
                expectEquals (analysis[0].borrowedScale, juce::String ("A Harmonic minor"));
            }

            // E7 -> Am (tonic): same V7 harmonic-minor result
            {
                ProgressionModel model;
                model.setKey (aMinor);
                model.setNumBars (2);
                model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
                model.setChord (1, 0, Chord { 9, ChordQuality::Minor,      -1 }); // Am

                const auto analysis = HarmonyAnalyzer::analyzeAll (model);
                expect (! analysis[0].diatonic);
                expect (analysis[0].technique == NonDiatonicTechnique::ModalInterchange);
                expectEquals (analysis[0].label, juce::String ("V7"));
                expectEquals (analysis[0].borrowedScale, juce::String ("A Harmonic minor"));
            }
        }

        beginTest ("detectPatterns: Two-Five-One Dm7-G7-C");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7
            model.setChord (2, 0, Chord { 0, ChordQuality::Major,     -1 }); // C

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Two-Five-One")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Royal Road preset match");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            // FM7 - G7 - Em7 - Am
            model.setChord (0, 0, Chord { 5, ChordQuality::Major7,    -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 });
            model.setChord (2, 0, Chord { 4, ChordQuality::Minor7,    -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor,     -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Royal Road (IV-V-iii-vi)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Royal Road triad form (maj/m as M7/m7)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            // F - G7 - Em - Am (triad form of Royal Road)
            model.setChord (0, 0, Chord { 5, ChordQuality::Major,      -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7,  -1 });
            model.setChord (2, 0, Chord { 4, ChordQuality::Minor,      -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor,      -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Royal Road (IV-V-iii-vi)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found, "Royal Road triad form should match via maj/m <-> M7/m7");
        }

        beginTest ("detectPatterns: Royal Road all-seventh form (vi as m7)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            // FM7 - G7 - Em7 - Am7 (vi as Minor7; bidirectional quality match)
            model.setChord (0, 0, Chord { 5, ChordQuality::Major7,    -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 });
            model.setChord (2, 0, Chord { 4, ChordQuality::Minor7,    -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor7,    -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Royal Road (IV-V-iii-vi)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found, "Royal Road with Am7 should match (m7 <-> m)");
        }

        beginTest ("detectPatterns: Royal Road rejects Dominant7 on iii");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            // F - G7 - E7 - Am (iii as Dominant7 must not match Minor/Minor7)
            model.setChord (0, 0, Chord { 5, ChordQuality::Major,      -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7,  -1 });
            model.setChord (2, 0, Chord { 4, ChordQuality::Dominant7,  -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor,      -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Royal Road (IV-V-iii-vi)")
                    found = true;
            }
            expect (! found, "Dominant7 on iii must not match Royal Road");
        }

        beginTest ("detectPatterns: Royal Road V7 slot as Major triad");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            // F - G - Em - Am (preset Dominant7 may be played as Major)
            model.setChord (0, 0, Chord { 5, ChordQuality::Major, -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Major, -1 });
            model.setChord (2, 0, Chord { 4, ChordQuality::Minor, -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor, -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Royal Road (IV-V-iii-vi)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found, "Royal Road V7 slot played as Major should match");
        }

        beginTest ("detectPatterns: Autumn Leaves m7b5 slot as Diminished");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (8);
            // Dm7 - G7 - CM7 - FM7 - Bdim - E7 - Am7 - Am7
            model.setChord (0, 0, Chord { 2,  ChordQuality::Minor7,      -1 });
            model.setChord (1, 0, Chord { 7,  ChordQuality::Dominant7,   -1 });
            model.setChord (2, 0, Chord { 0,  ChordQuality::Major7,      -1 });
            model.setChord (3, 0, Chord { 5,  ChordQuality::Major7,      -1 });
            model.setChord (4, 0, Chord { 11, ChordQuality::Diminished,   -1 });
            model.setChord (5, 0, Chord { 4,  ChordQuality::Dominant7,   -1 });
            model.setChord (6, 0, Chord { 9,  ChordQuality::Minor7,      -1 });
            model.setChord (7, 0, Chord { 9,  ChordQuality::Minor7,      -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Autumn Leaves (ii-V-I-IV-vii-III7-vi)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 7);
                }
            }
            expect (found, "Autumn Leaves m7b5 slot played as dim should match");
        }

        beginTest ("detectPatterns: Canon rejects Dominant7 on I (no reverse)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (8);
            // C7 - G - Am - Em - F - C - F - G (I as Dominant7 must not match Major)
            model.setChord (0, 0, Chord { 0, ChordQuality::Dominant7, -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Major,     -1 });
            model.setChord (2, 0, Chord { 9, ChordQuality::Minor,     -1 });
            model.setChord (3, 0, Chord { 4, ChordQuality::Minor,     -1 });
            model.setChord (4, 0, Chord { 5, ChordQuality::Major,     -1 });
            model.setChord (5, 0, Chord { 0, ChordQuality::Major,     -1 });
            model.setChord (6, 0, Chord { 5, ChordQuality::Major,     -1 });
            model.setChord (7, 0, Chord { 7, ChordQuality::Major,     -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Canon (I-V-vi-iii-IV-I-IV-V)")
                    found = true;
            }
            expect (! found, "Canon I as Dominant7 must not match (no reverse)");
        }

        beginTest ("detectPatterns: Royal Road rejects Major7 on V7 slot");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            // F - GM7 - Em - Am (Major7 must not match preset Dominant7)
            model.setChord (0, 0, Chord { 5, ChordQuality::Major,   -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Major7,  -1 });
            model.setChord (2, 0, Chord { 4, ChordQuality::Minor,   -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor,   -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Royal Road (IV-V-iii-vi)")
                    found = true;
            }
            expect (! found, "Major7 on V7 slot must not match Royal Road");
        }

        beginTest ("substitutions: C Major dairi and G7 ura");
        {
            const Chord cMajorChord { 0, ChordQuality::Major, -1 };
            const auto subsC = HarmonyAnalyzer::substitutions (cMajorChord, cMajor);

            bool hasAm7 = false;
            bool hasEm7 = false;
            for (const auto& s : subsC)
            {
                if (s.chord == Chord { 9, ChordQuality::Minor7, -1 })
                    hasAm7 = true;
                if (s.chord == Chord { 4, ChordQuality::Minor7, -1 })
                    hasEm7 = true;
            }
            expect (hasAm7);
            expect (hasEm7);

            const Chord g7 { 7, ChordQuality::Dominant7, -1 };
            const auto subsG = HarmonyAnalyzer::substitutions (g7, cMajor);

            bool hasDb7 = false;
            for (const auto& s : subsG)
            {
                if (s.chord == Chord { 1, ChordQuality::Dominant7, -1 })
                    hasDb7 = true;
            }
            expect (hasDb7);
        }

        beginTest ("Empty model (all rests)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            // Default: 4 bars, all rests

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), model.totalSlots());
            for (const auto& a : analysis)
            {
                expect (a.diatonic);
                expect (a.technique == NonDiatonicTechnique::None);
                expect (a.label.isEmpty());
                expect (a.borrowedScale.isEmpty());
            }

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            expect (patterns.empty());
        }

        beginTest ("detectPatterns: Royal Road substring match with extra chord");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            // Royal Road: FM7 - G7 - Em7 - Am, plus extra C Major at bar 4
            model.setChord (0, 0, Chord { 5, ChordQuality::Major7,    -1 });
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 });
            model.setChord (2, 0, Chord { 4, ChordQuality::Minor7,    -1 });
            model.setChord (3, 0, Chord { 9, ChordQuality::Minor,     -1 });
            model.setChord (4, 0, Chord { 0, ChordQuality::Major,     -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Royal Road (IV-V-iii-vi)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found, "Royal Road should be detected as substring match");
        }

        beginTest ("detectPatterns: Pop Punk in G minor");
        {
            ProgressionModel model;
            model.setKey ({7, false}); // G minor
            model.setNumBars (4);
            // Pop Punk in G minor: relative major is A# (10)
            // I = A# Major (10), V = F Major (5), vi = G Minor (7), IV = D# Major (3)
            model.setChord (0, 0, Chord { 10, ChordQuality::Major, -1 });
            model.setChord (1, 0, Chord { 5,  ChordQuality::Major, -1 });
            model.setChord (2, 0, Chord { 7,  ChordQuality::Minor, -1 });
            model.setChord (3, 0, Chord { 3,  ChordQuality::Major, -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Pop Punk (I-V-vi-IV)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found, "Pop Punk should be detected in G minor key");
        }

        beginTest ("SusDominant: E7 -> Em7/A -> CM7 in C major (functional root reinterpretation)");
        {
            // E7's plain root offset from C is 4 (not 2), so this does not collide
            // with the DoubleDominant rule; it resolves via Em7/A's functional
            // root (A, from the sus reinterpretation) rather than Em7/A's literal
            // root (E), which would land on the tonic-adjacent III degree instead.
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7,     9 }); // Em7/A
            model.setChord (2, 0, Chord { 0, ChordQuality::Major7,    -1 }); // CM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 3);

            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[0].label.contains ("V7/"));

            // Em7/A itself is diatonic in C major (E, G, B, D, A all in scale).
            expect (analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::None);
        }

        beginTest ("SusDominant regression: Em7/A stays diatonic in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7,  9 }); // Em7/A
            model.setChord (2, 0, Chord { 0, ChordQuality::Major7, -1 }); // CM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 3);

            expect (analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::None);
        }

        beginTest ("SusDominant: F#m7/B -> Em7 in C major (V7/III via functional root)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 6, ChordQuality::Minor7, 11 }); // F#m7/B
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7, -1 }); // Em7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 2);

            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/III)"));
        }

        beginTest ("detectPatterns: Leading Resolution via sus reinterpretation (Em7/A -> Dm7)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 4, ChordQuality::Minor7, 9 }); // Em7/A
            model.setChord (1, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Leading Resolution")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 1);
                }
            }
            expect (found, "Leading Resolution should be detected for Em7/A -> Dm7 via sus reinterpretation");
        }

        beginTest ("BackdoorDominant: Fm7 - Bb7 - CM7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 5,  ChordQuality::Minor7,    -1 }); // Fm7
            model.setChord (1, 0, Chord { 10, ChordQuality::Dominant7, -1 }); // Bb7
            model.setChord (2, 0, Chord { 0,  ChordQuality::Major7,    -1 }); // CM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expectEquals ((int) analysis.size(), 3);
            expect (analysis[0].technique == NonDiatonicTechnique::SubdominantMinor);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::BackdoorDominant);
            expect (analysis[1].label.startsWith ("Backdoor"));
        }

        beginTest ("BackdoorDominant regression: Bb7 - FM7 stays SubdominantMinor");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 10, ChordQuality::Dominant7, -1 }); // Bb7
            model.setChord (1, 0, Chord { 5,  ChordQuality::Major7,    -1 }); // FM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SubdominantMinor);
        }

        beginTest ("BackdoorDominant regression: Bb7 - EbM7 stays SecondaryDominant");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 10, ChordQuality::Dominant7, -1 }); // Bb7
            model.setChord (1, 0, Chord { 3,  ChordQuality::Major7,    -1 }); // EbM7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
        }

        beginTest ("ChromaticApproach: C#m7 - Dm7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 1, ChordQuality::Minor7, -1 }); // C#m7
            model.setChord (1, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::ChromaticApproach);
            expectEquals (analysis[0].label, juce::String ("Chr.App"));
        }

        beginTest ("ChromaticApproach: Ebm7 - Dm7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 3, ChordQuality::Minor7, -1 }); // Ebm7
            model.setChord (1, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::ChromaticApproach);
        }

        beginTest ("ChromaticApproach regression: Fm7 - Em7 stays SubdominantMinor");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 5, ChordQuality::Minor7, -1 }); // Fm7
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7, -1 }); // Em7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SubdominantMinor);
        }

        beginTest ("ChromaticApproach quality mismatch: C#7 - Dm7 is not ChromaticApproach");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 1, ChordQuality::Dominant7, -1 }); // C#7
            model.setChord (1, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique != NonDiatonicTechnique::ChromaticApproach);
        }

        beginTest ("LineCliche: Fm - Eaug - Fm7/Eb in Eb major");
        {
            const KeyContext ebMajor { 3, true };

            ProgressionModel model;
            model.setKey (ebMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 5, ChordQuality::Minor,  -1 }); // Fm
            model.setChord (1, 0, Chord { 4, ChordQuality::Augmented, -1 }); // Eaug
            model.setChord (2, 0, Chord { 5, ChordQuality::Minor7,  3 }); // Fm7/Eb

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (analysis[2].diatonic);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::LineCliche);
            expectEquals (analysis[1].label, juce::String ("Line Cliche"));
        }

        beginTest ("LineCliche: F - Faug - F6 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 5, ChordQuality::Major,     -1 }); // F
            model.setChord (1, 0, Chord { 5, ChordQuality::Augmented, -1 }); // Faug
            model.setChord (2, 0, Chord { 5, ChordQuality::Sixth,     -1 }); // F6

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::LineCliche);
        }

        beginTest ("LineCliche: Cm - CmM7 - Cm7 in C minor (supersedes M.I.)");
        {
            const KeyContext cMinor { 0, false };

            ProgressionModel model;
            model.setKey (cMinor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Minor,      -1 }); // Cm
            model.setChord (1, 0, Chord { 0, ChordQuality::MinorMajor7, -1 }); // CmM7
            model.setChord (2, 0, Chord { 0, ChordQuality::Minor7,     -1 }); // Cm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::LineCliche);
        }

        beginTest ("LineCliche negative: C - E - F in C major (two moving tones)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C
            model.setChord (1, 0, Chord { 4, ChordQuality::Major, -1 }); // E
            model.setChord (2, 0, Chord { 5, ChordQuality::Major, -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[1].technique != NonDiatonicTechnique::LineCliche);
        }

        beginTest ("LineCliche negative: Eaug - Fm7 with no prev chord");
        {
            const KeyContext ebMajor { 3, true };

            ProgressionModel model;
            model.setKey (ebMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 4, ChordQuality::Augmented, -1 }); // Eaug
            model.setChord (1, 0, Chord { 5, ChordQuality::Minor7,    -1 }); // Fm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique != NonDiatonicTechnique::LineCliche);
        }
    }
};

static HarmonyAnalyzerTest harmonyAnalyzerTest;
