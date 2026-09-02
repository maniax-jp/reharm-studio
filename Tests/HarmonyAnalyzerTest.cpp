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

        beginTest ("PassingDiminished with slash chord: Dm7 - D#dim - C/E");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7,      -1 }); // Dm7
            model.setChord (1, 0, Chord { 3, ChordQuality::Diminished, -1 }); // D#dim
            model.setChord (2, 0, Chord { 0, ChordQuality::Major,       4 }); // C/E

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::PassingDiminished);
            expectEquals (analysis[1].label, juce::String ("Pass.Dim"));
        }

        beginTest ("PassingDiminished with slash chord: Dm7 - D#dim7 - C/E");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7,       -1 }); // Dm7
            model.setChord (1, 0, Chord { 3, ChordQuality::Diminished7, -1 }); // D#dim7
            model.setChord (2, 0, Chord { 0, ChordQuality::Major,        4 }); // C/E

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::PassingDiminished);
            expectEquals (analysis[1].label, juce::String ("Pass.Dim"));
        }

        beginTest ("PassingDiminished negative: Dm7 - D#dim - C (no slash) is not Pass.Dim");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7,      -1 }); // Dm7
            model.setChord (1, 0, Chord { 3, ChordQuality::Diminished, -1 }); // D#dim
            model.setChord (2, 0, Chord { 0, ChordQuality::Major,       -1 }); // C

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

        beginTest ("SecondaryDominant: final Eaug after Bm7 in C major (7th omitted)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0,  ChordQuality::Major,     -1 }); // C
            model.setChord (1, 0, Chord { 11, ChordQuality::Minor7,    -1 }); // Bm7
            model.setChord (2, 0, Chord { 4,  ChordQuality::Augmented, -1 }); // Eaug

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::RelatedTwoMinor);
            expectEquals (analysis[1].label, juce::String ("Rel.IIm"));
            expect (! analysis[2].diatonic);
            expect (analysis[2].technique == NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[2].label.contains ("V7/VI"));
        }

        beginTest ("SecondaryDominant: Eaug to Am after Bm7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 11, ChordQuality::Minor7,    -1 }); // Bm7
            model.setChord (1, 0, Chord { 4,  ChordQuality::Augmented, -1 }); // Eaug
            model.setChord (2, 0, Chord { 9,  ChordQuality::Minor,     -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::RelatedTwoMinor);
            expect (analysis[0].label.contains ("Rel.IIm"));
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[1].label.contains ("V7/VI"));
        }

        beginTest ("SecondaryDominant: Caug to F in C major (aug V7/IV)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 }); // C
            model.setChord (1, 0, Chord { 0, ChordQuality::Augmented, -1 }); // Caug
            model.setChord (2, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expect (analysis[1].label.contains ("V7/IV"));
        }

        beginTest ("TritoneSubstitution: F#7 to F in C major (pin of existing ura)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 6, ChordQuality::Dominant7, -1 }); // F#7
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::TritoneSubstitution);
            expectEquals (analysis[0].label, juce::String ("Tritone Sub"));
        }

        beginTest ("TritoneSubstitution: F#blk to F in C major (ura of Caug)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 6, ChordQuality::Blackadder, -1 }); // F#blk
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,      -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::TritoneSubstitution);
            expectEquals (analysis[0].label, juce::String ("Tritone Sub"));
        }

        beginTest ("TritoneSubstitution: A#blk to Am in C major (ura of E7)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 10, ChordQuality::Blackadder, -1 }); // A#blk
            model.setChord (1, 0, Chord { 9,  ChordQuality::Minor,      -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::TritoneSubstitution);
            expectEquals (analysis[0].label, juce::String ("Tritone Sub"));
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

        beginTest ("Minor-key V7 is harmonic-minor diatonic (E7 in Am)");
        {
            // E7 -> F (not tonic): harmonic-minor V7 path
            {
                ProgressionModel model;
                model.setKey (aMinor);
                model.setNumBars (2);
                model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
                model.setChord (1, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

                const auto analysis = HarmonyAnalyzer::analyzeAll (model);
                expect (analysis[0].diatonic);
                expect (analysis[0].technique == NonDiatonicTechnique::None);
                expect (analysis[0].derivation == MinorDerivation::HarmonicMinor);
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
                expect (analysis[0].diatonic);
                expect (analysis[0].technique == NonDiatonicTechnique::None);
                expect (analysis[0].derivation == MinorDerivation::HarmonicMinor);
                expectEquals (analysis[0].borrowedScale, juce::String ("A Harmonic minor"));
            }
        }

        beginTest ("Minor-key viio7 is harmonic-minor, not passing diminished");
        {
            // G#dim7 -> Am: previously mislabelled Pass.Dim by the chromatic rule.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 8, ChordQuality::Diminished7, -1 }); // G#dim7
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor,       -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::None);
            expect (analysis[0].derivation == MinorDerivation::HarmonicMinor);
            expectEquals (analysis[0].borrowedScale, juce::String ("A Harmonic minor"));
        }

        beginTest ("Minor-key IV major is melodic-minor derived (D in Am)");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 2, ChordQuality::Major, -1 }); // D
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor, -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::None);
            expect (analysis[0].derivation == MinorDerivation::MelodicMinor);
            expectEquals (analysis[0].borrowedScale, juce::String ("A Melodic minor"));
        }

        beginTest ("Natural-minor chords keep no derivation");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7 (iv)
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor,  -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (analysis[0].derivation == MinorDerivation::None);
            expect (analysis[0].borrowedScale.isEmpty());
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

        // ---- Pop Punk rotations ----------------------------------------------
        // The loop is the pattern, so all four entry points count: I-V-vi-IV,
        // V-vi-IV-I, vi-IV-I-V and IV-I-V-vi.

        beginTest ("detectPatterns: Pop Punk is detected from any of its four bars");
        {
            // C = 0, G = 7, Am = 9, F = 5
            struct Rotation { const char* label; std::array<Chord, 4> chords; };
            const std::array<Rotation, 4> rotations { {
                { "I-V-vi-IV", { Chord { 0, ChordQuality::Major, -1 }, Chord { 7, ChordQuality::Major, -1 },
                                 Chord { 9, ChordQuality::Minor, -1 }, Chord { 5, ChordQuality::Major, -1 } } },
                { "V-vi-IV-I", { Chord { 7, ChordQuality::Major, -1 }, Chord { 9, ChordQuality::Minor, -1 },
                                 Chord { 5, ChordQuality::Major, -1 }, Chord { 0, ChordQuality::Major, -1 } } },
                { "vi-IV-I-V", { Chord { 9, ChordQuality::Minor, -1 }, Chord { 5, ChordQuality::Major, -1 },
                                 Chord { 0, ChordQuality::Major, -1 }, Chord { 7, ChordQuality::Major, -1 } } },
                { "IV-I-V-vi", { Chord { 5, ChordQuality::Major, -1 }, Chord { 0, ChordQuality::Major, -1 },
                                 Chord { 7, ChordQuality::Major, -1 }, Chord { 9, ChordQuality::Minor, -1 } } }
            } };

            for (const auto& rotation : rotations)
            {
                ProgressionModel model;
                model.setKey (cMajor);
                model.setNumBars (4);
                for (int bar = 0; bar < 4; ++bar)
                    model.setChord (bar, 0, rotation.chords [static_cast<size_t> (bar)]);

                const auto patterns = HarmonyAnalyzer::detectPatterns (model);
                int matches = 0;
                for (const auto& p : patterns)
                {
                    if (p.name != "Pop Punk (I-V-vi-IV)")
                        continue;
                    ++matches;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }

                expectEquals (matches, 1, juce::String ("Pop Punk ") + rotation.label
                                              + " should be reported exactly once");
            }
        }

        beginTest ("detectPatterns: repeated Pop Punk loop reports one match per turn");
        {
            // | C G Am F | C G Am F |: the rotations must not turn one loop into
            // an overlapping match at every bar.
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (8);
            const std::array<Chord, 4> loop { Chord { 0, ChordQuality::Major, -1 },
                                              Chord { 7, ChordQuality::Major, -1 },
                                              Chord { 9, ChordQuality::Minor, -1 },
                                              Chord { 5, ChordQuality::Major, -1 } };
            for (int bar = 0; bar < 8; ++bar)
                model.setChord (bar, 0, loop [static_cast<size_t> (bar % 4)]);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            std::vector<int> starts;
            for (const auto& p : patterns)
                if (p.name == "Pop Punk (I-V-vi-IV)")
                    starts.push_back (p.startIndex);

            expect (starts == std::vector<int> { 0, 4 });
        }

        beginTest ("detectPatterns: Komuro is not swallowed by a Pop Punk rotation");
        {
            // vi-IV-V-I is a different cycle from vi-IV-I-V.
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor, -1 }); // Am
            model.setChord (1, 0, Chord { 5, ChordQuality::Major, -1 }); // F
            model.setChord (2, 0, Chord { 7, ChordQuality::Major, -1 }); // G
            model.setChord (3, 0, Chord { 0, ChordQuality::Major, -1 }); // C

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool komuro = false;
            bool popPunk = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Komuro (vi-IV-V-I)")      komuro = true;
                if (p.name == "Pop Punk (I-V-vi-IV)")    popPunk = true;
            }

            expect (komuro, "Komuro should still be detected");
            expect (! popPunk, "vi-IV-V-I is not a Pop Punk rotation");
        }

        // ---- Confirmation ----------------------------------------------------
        // | C | Bm7b5 E7 | Am7 | Gm7 C7 | F |. Two of its bars carry a chord pair,
        // but matching only looks at the chord run, so the bar layout is free.

        beginTest ("detectPatterns: Confirmation preset match");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            model.setChord (0, 0, Chord { 0,  ChordQuality::Major,       -1 }); // C
            model.setSubdivision (1, BarSubdivision::Two);
            model.setChord (1, 0, Chord { 11, ChordQuality::Minor7Flat5, -1 }); // Bm7b5
            model.setChord (1, 1, Chord { 4,  ChordQuality::Dominant7,   -1 }); // E7
            model.setChord (2, 0, Chord { 9,  ChordQuality::Minor7,      -1 }); // Am7
            model.setSubdivision (3, BarSubdivision::Two);
            model.setChord (3, 0, Chord { 7,  ChordQuality::Minor7,      -1 }); // Gm7
            model.setChord (3, 1, Chord { 0,  ChordQuality::Dominant7,   -1 }); // C7
            model.setChord (4, 0, Chord { 5,  ChordQuality::Major,       -1 }); // F

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Confirmation (I-viim7b5-III7-vim7-iim7-V7-IV)")
                    found = true;

            expect (found, "Confirmation should be detected in its canonical bar layout");
        }

        beginTest ("detectPatterns: Confirmation ignores bar boundaries");
        {
            // The same seven chords, one per bar. Matching must not care.
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (7);
            model.setChord (0, 0, Chord { 0,  ChordQuality::Major,       -1 });
            model.setChord (1, 0, Chord { 11, ChordQuality::Minor7Flat5, -1 });
            model.setChord (2, 0, Chord { 4,  ChordQuality::Dominant7,   -1 });
            model.setChord (3, 0, Chord { 9,  ChordQuality::Minor7,      -1 });
            model.setChord (4, 0, Chord { 7,  ChordQuality::Minor7,      -1 });
            model.setChord (5, 0, Chord { 0,  ChordQuality::Dominant7,   -1 });
            model.setChord (6, 0, Chord { 5,  ChordQuality::Major,       -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Confirmation (I-viim7b5-III7-vim7-iim7-V7-IV)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 6);
                }
            }
            expect (found, "Confirmation should match regardless of how bars are split");
        }

        beginTest ("detectPatterns: Confirmation with Bm7 instead of Bm7b5");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            model.setChord (0, 0, Chord { 0,  ChordQuality::Major,     -1 }); // C
            model.setSubdivision (1, BarSubdivision::Two);
            model.setChord (1, 0, Chord { 11, ChordQuality::Minor7,    -1 }); // Bm7 (was Bm7b5)
            model.setChord (1, 1, Chord { 4,  ChordQuality::Dominant7, -1 }); // E7
            model.setChord (2, 0, Chord { 9,  ChordQuality::Minor7,    -1 }); // Am7
            model.setSubdivision (3, BarSubdivision::Two);
            model.setChord (3, 0, Chord { 7,  ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (3, 1, Chord { 0,  ChordQuality::Dominant7, -1 }); // C7
            model.setChord (4, 0, Chord { 5,  ChordQuality::Major,     -1 }); // F

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Confirmation (I-viim7b5-III7-vim7-iim7-V7-IV)")
                    found = true;

            expect (found, "Confirmation should match when vii is a minor 7th");
        }

        beginTest ("detectPatterns: Confirmation with Caug7 instead of C7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            model.setChord (0, 0, Chord { 0,  ChordQuality::Major,        -1 }); // C
            model.setSubdivision (1, BarSubdivision::Two);
            model.setChord (1, 0, Chord { 11, ChordQuality::Minor7Flat5,  -1 }); // Bm7b5
            model.setChord (1, 1, Chord { 4,  ChordQuality::Dominant7,    -1 }); // E7
            model.setChord (2, 0, Chord { 9,  ChordQuality::Minor7,       -1 }); // Am7
            model.setSubdivision (3, BarSubdivision::Two);
            model.setChord (3, 0, Chord { 7,  ChordQuality::Minor7,       -1 }); // Gm7
            model.setChord (3, 1, Chord { 0,  ChordQuality::Augmented7,   -1 }); // Caug7 (was C7)
            model.setChord (4, 0, Chord { 5,  ChordQuality::Major,        -1 }); // F

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Confirmation (I-viim7b5-III7-vim7-iim7-V7-IV)")
                    found = true;

            expect (found, "Confirmation should match when V/IV is an augmented 7th");
        }

        beginTest ("detectPatterns: Confirmation with Caug instead of C7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            model.setChord (0, 0, Chord { 0,  ChordQuality::Major,      -1 }); // C
            model.setSubdivision (1, BarSubdivision::Two);
            model.setChord (1, 0, Chord { 11, ChordQuality::Minor7Flat5, -1 }); // Bm7b5
            model.setChord (1, 1, Chord { 4,  ChordQuality::Dominant7,   -1 }); // E7
            model.setChord (2, 0, Chord { 9,  ChordQuality::Minor7,      -1 }); // Am7
            model.setSubdivision (3, BarSubdivision::Two);
            model.setChord (3, 0, Chord { 7,  ChordQuality::Minor7,      -1 }); // Gm7
            model.setChord (3, 1, Chord { 0,  ChordQuality::Augmented,   -1 }); // Caug (was C7)
            model.setChord (4, 0, Chord { 5,  ChordQuality::Major,       -1 }); // F

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Confirmation (I-viim7b5-III7-vim7-iim7-V7-IV)")
                    found = true;

            expect (found, "Confirmation should match when V/IV is an augmented triad");
        }

        beginTest ("detectPatterns: Autumn Leaves rejects Bm7 in the m7b5 slot");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (8);
            model.setChord (0, 0, Chord { 2,  ChordQuality::Minor7,      -1 }); // Dm7
            model.setChord (1, 0, Chord { 7,  ChordQuality::Dominant7,   -1 }); // G7
            model.setChord (2, 0, Chord { 0,  ChordQuality::Major7,      -1 }); // CM7
            model.setChord (3, 0, Chord { 5,  ChordQuality::Major7,      -1 }); // FM7
            model.setChord (4, 0, Chord { 11, ChordQuality::Minor7,      -1 }); // Bm7 (not m7b5)
            model.setChord (5, 0, Chord { 4,  ChordQuality::Dominant7,   -1 }); // E7
            model.setChord (6, 0, Chord { 9,  ChordQuality::Minor7,      -1 }); // Am7
            model.setChord (7, 0, Chord { 9,  ChordQuality::Minor7,      -1 }); // Am7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Autumn Leaves (ii-V-I-IV-vii-III7-vi)")
                    found = true;

            expect (! found, "the m7b5 relaxation must not apply to Autumn Leaves");
        }

        // ---- Only One --------------------------------------------------------
        // | C | F | G | E | Am7 |, where the III slot also accepts E7 or Abdim.

        beginTest ("detectPatterns: Only One preset match (III as triad)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,  -1 }); // C
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,  -1 }); // F
            model.setChord (2, 0, Chord { 7, ChordQuality::Major,  -1 }); // G
            model.setChord (3, 0, Chord { 4, ChordQuality::Major,  -1 }); // E
            model.setChord (4, 0, Chord { 9, ChordQuality::Minor7, -1 }); // Am7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Only One (I-IV-V-III-vim7)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 4);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Only One III slot as E7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 });
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,     -1 });
            model.setChord (2, 0, Chord { 7, ChordQuality::Major,     -1 });
            model.setChord (3, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (4, 0, Chord { 9, ChordQuality::Minor7,    -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Only One (I-IV-V-III-vim7)")
                    found = true;

            expect (found, "Only One should accept a dominant 7th on the III slot");
        }

        beginTest ("detectPatterns: Only One III slot as Abdim substitute");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (5);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,       -1 });
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,       -1 });
            model.setChord (2, 0, Chord { 7, ChordQuality::Major,       -1 });
            model.setChord (3, 0, Chord { 8, ChordQuality::Diminished,  -1 }); // Abdim
            model.setChord (4, 0, Chord { 9, ChordQuality::Minor7,      -1 });

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Only One (I-IV-V-III-vim7)")
                    found = true;

            expect (found, "Only One should accept Abdim in place of the III chord");
        }

        // ---- Ochiba ----------------------------------------------------------
        // | C | Em/B | Edim/Bb | A7 | Dm | C#aug | F/C | G |. Bar 3 accepts any
        // inversion of Edim, plus anything non-augmented sitting on the Bb bass.

        // Fills every bar but bar 2, which the caller sets to the shape under test.
        auto buildOchiba = [] (ProgressionModel& m, Chord barTwo)
        {
            m.setNumBars (8);
            m.setChord (0, 0, Chord { 0, ChordQuality::Major,      -1 }); // C
            m.setChord (1, 0, Chord { 4, ChordQuality::Minor,      11 }); // Em/B
            m.setChord (2, 0, barTwo);
            m.setChord (3, 0, Chord { 9, ChordQuality::Dominant7,  -1 }); // A7
            m.setChord (4, 0, Chord { 2, ChordQuality::Minor,      -1 }); // Dm
            m.setChord (5, 0, Chord { 1, ChordQuality::Augmented,  -1 }); // C#aug
            m.setChord (6, 0, Chord { 5, ChordQuality::Major,       0 }); // F/C
            m.setChord (7, 0, Chord { 7, ChordQuality::Major,      -1 }); // G
        };

        auto ochibaDetected = [&] (Chord barTwo)
        {
            ProgressionModel model;
            model.setKey (cMajor);
            buildOchiba (model, barTwo);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            for (const auto& p : patterns)
                if (p.name == "Ochiba (I-iii-biiidim-VI7-iim-Iaug-IV-V)")
                    return true;
            return false;
        };

        beginTest ("detectPatterns: Ochiba preset match");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            buildOchiba (model, Chord { 4, ChordQuality::Diminished, 10 }); // Edim/Bb

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Ochiba (I-iii-biiidim-VI7-iim-Iaug-IV-V)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 7);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Ochiba bar 3 accepts any Edim inversion");
        {
            expect (ochibaDetected (Chord { 4, ChordQuality::Diminished, -1 }),
                    "root-position Edim should match");
            expect (ochibaDetected (Chord { 4, ChordQuality::Diminished,  7 }),
                    "Edim/G should match");
        }

        beginTest ("detectPatterns: Ochiba bar 3 accepts other chords over the Bb bass");
        {
            expect (ochibaDetected (Chord { 10, ChordQuality::Major,   -1 }), "Bb");
            expect (ochibaDetected (Chord { 10, ChordQuality::Minor,   -1 }), "Bbm");
            expect (ochibaDetected (Chord { 10, ChordQuality::Sus2,    -1 }), "Bbsus2");
            expect (ochibaDetected (Chord { 10, ChordQuality::Sus4,    -1 }), "Bbsus4");
            expect (ochibaDetected (Chord { 10, ChordQuality::Diminished, -1 }), "Bbdim");
            expect (ochibaDetected (Chord { 10, ChordQuality::Major7,  -1 }), "BbM7");
            expect (ochibaDetected (Chord { 7,  ChordQuality::Minor,   10 }), "Gm/Bb");
            expect (ochibaDetected (Chord { 0,  ChordQuality::Major,   10 }), "C/Bb");
            expect (ochibaDetected (Chord { 9,  ChordQuality::Minor7,  10 }), "Am7/Bb");
        }

        beginTest ("detectPatterns: Ochiba bar 3 rejects augmented chords");
        {
            expect (! ochibaDetected (Chord { 10, ChordQuality::Augmented,       -1 }),
                    "Bbaug breaks the descending line and must not match");
            expect (! ochibaDetected (Chord { 10, ChordQuality::Augmented7,      -1 }),
                    "Bbaug7 must not match");
            expect (! ochibaDetected (Chord { 10, ChordQuality::AugmentedMajor7, -1 }),
                    "BbaugM7 must not match");
            expect (! ochibaDetected (Chord { 1,  ChordQuality::Augmented,       10 }),
                    "C#aug/Bb must not match");
        }

        beginTest ("detectPatterns: Ochiba bar 3 rejects a bass off the line");
        {
            expect (! ochibaDetected (Chord { 7, ChordQuality::Minor, -1 }),
                    "Gm sits on neither the Edim shape nor the Bb bass");
        }

        beginTest ("detectPatterns: Ochiba tolerates inversions elsewhere");
        {
            // Em/B written as a plain Em: slash bases never constrain matching.
            ProgressionModel model;
            model.setKey (cMajor);
            buildOchiba (model, Chord { 4, ChordQuality::Diminished, 10 });
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor, -1 }); // Em
            model.setChord (6, 0, Chord { 5, ChordQuality::Major, -1 }); // F

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
                if (p.name == "Ochiba (I-iii-biiidim-VI7-iim-Iaug-IV-V)")
                    found = true;

            expect (found, "Ochiba should match with the slash bases left off");
        }

        beginTest ("detectPatterns: Ochiba closing V as a dominant 7th");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            buildOchiba (model, Chord { 4, ChordQuality::Diminished, 10 });
            model.setChord (7, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Ochiba (I-iii-biiidim-VI7-iim-Iaug-IV-V)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 7);
                }
            }
            expect (found, "Ochiba should accept G7 on its closing V");
        }

        // ---- Champs-Elysees --------------------------------------------------
        // | C | E7/B | Am | C7/G |, with an augmented cliche allowed in bar 2 and
        // a ii-V split allowed in bar 4.

        beginTest ("detectPatterns: Champs-Elysees preset match");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 }); // C
            model.setChord (1, 0, Chord { 4, ChordQuality::Dominant7, 11 }); // E7/B
            model.setChord (2, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am
            model.setChord (3, 0, Chord { 0, ChordQuality::Dominant7,  7 }); // C7/G

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Champs-Elysees (I-III7-vim-I7)")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Champs-Elysees bar 2 as an augmented cliche");
        {
            auto detectedWithBarTwo = [&] (Chord barTwo)
            {
                ProgressionModel model;
                model.setKey (cMajor);
                model.setNumBars (4);
                model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 });
                model.setChord (1, 0, barTwo);
                model.setChord (2, 0, Chord { 9, ChordQuality::Minor,     -1 });
                model.setChord (3, 0, Chord { 0, ChordQuality::Dominant7,  7 });

                const auto patterns = HarmonyAnalyzer::detectPatterns (model);
                for (const auto& p : patterns)
                    if (p.name == "Champs-Elysees (I-III7-vim-I7)")
                        return true;
                return false;
            };

            expect (detectedWithBarTwo (Chord { 0, ChordQuality::Augmented, -1 }), "Caug");
            expect (detectedWithBarTwo (Chord { 4, ChordQuality::Augmented, -1 }), "Eaug");
        }

        beginTest ("detectPatterns: Champs-Elysees bar 4 split into a ii-V");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 }); // C
            model.setChord (1, 0, Chord { 4, ChordQuality::Dominant7, 11 }); // E7/B
            model.setChord (2, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am
            model.setSubdivision (3, BarSubdivision::Two);
            model.setChord (3, 0, Chord { 7, ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (3, 1, Chord { 0, ChordQuality::Dominant7, -1 }); // C7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Champs-Elysees (I-III7-vim-I7)")
                {
                    found = true;
                    // Five chords now, so the match runs one slot further.
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 4);
                }
            }
            expect (found, "Champs-Elysees should accept Gm7 C7 in place of C7/G");
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

        //======================================================================
        // Neapolitan sixth
        //======================================================================
        beginTest ("Neapolitan: Bb - E7 - Am in A minor");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 10, ChordQuality::Major,     -1 }); // Bb (bII)
            model.setChord (1, 0, Chord { 4,  ChordQuality::Dominant7, -1 }); // E7 (V)
            model.setChord (2, 0, Chord { 9,  ChordQuality::Minor,     -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::NeapolitanSixth);
            expectEquals (analysis[0].label, juce::String ("Neapolitan"));
        }

        beginTest ("Neapolitan: bII6 (Bb/D) resolving to tonic in A minor");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 10, ChordQuality::Major, 2 }); // Bb/D
            model.setChord (1, 0, Chord { 9,  ChordQuality::Minor, -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::NeapolitanSixth);
        }

        beginTest ("Neapolitan: Bb -> Dm (iv) in A minor");
        {
            // The Neapolitan is a predominant; resolving to another predominant
            // (iv) is idiomatic and must still read as a Neapolitan.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 10, ChordQuality::Major, -1 }); // Bb (bII)
            model.setChord (1, 0, Chord { 2,  ChordQuality::Minor, -1 }); // Dm (iv)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::NeapolitanSixth);
        }

        beginTest ("Neapolitan: Bb -> C (III) in A minor");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 10, ChordQuality::Major, -1 }); // Bb (bII)
            model.setChord (1, 0, Chord { 0,  ChordQuality::Major, -1 }); // C (III)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::NeapolitanSixth);
        }

        beginTest ("Neapolitan regression: Bb7 stays TritoneSub, not Neapolitan");
        {
            // Dominant-quality bII is a tritone substitute, not a Neapolitan.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 10, ChordQuality::Dominant7, -1 }); // Bb7
            model.setChord (1, 0, Chord { 9,  ChordQuality::Minor,     -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::TritoneSubstitution);
        }

        //======================================================================
        // Augmented sixths. In A minor the shared core is b6=F, 1=A, #4=D#;
        // the fourth tone names the type (none / 2=B / b3=C).
        //======================================================================
        beginTest ("ItalianSixth: F-A-D# -> E in A minor");
        {
            // It+6 = b6, 1, #4 only. As a chord: F7 with the fifth omitted,
            // giving F A Eb (Eb is D# enharmonically).
            Chord it { 5, ChordQuality::Dominant7, -1 };
            it.omitMask = Omit5;

            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, it);
            model.setChord (1, 0, Chord { 4, ChordQuality::Major, -1 }); // E (V)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::ItalianSixth);
            expectEquals (analysis[0].label, juce::String ("It.Aug.6th"));
        }

        beginTest ("FrenchSixth: F-A-B-D# -> E in A minor");
        {
            // Fr+6 adds the 2nd (B). As a chord: F7b5 = F A B Eb.
            Chord fr { 5, ChordQuality::Dominant7, -1 };
            fr.fifthAlt = FifthAlt::Flat;

            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, fr);
            model.setChord (1, 0, Chord { 4, ChordQuality::Major, -1 }); // E (V)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::FrenchSixth);
            expectEquals (analysis[0].label, juce::String ("Fr.Aug.6th"));
        }

        beginTest ("GermanSixth: F7 (F-A-C-D#) -> E in A minor");
        {
            // Ger+6 adds the b3 (C). As a chord: plain F7 = F A C Eb.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 5, ChordQuality::Dominant7, -1 }); // F7
            model.setChord (1, 0, Chord { 4, ChordQuality::Major,     -1 }); // E (V)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::GermanSixth);
            expectEquals (analysis[0].label, juce::String ("Ger.Aug.6th"));
        }

        beginTest ("AugmentedSixth split: same F7 -> Bb is a secondary dominant");
        {
            // Identical chord, different target: resolving down a fifth makes it
            // bVI7 rather than a German sixth. This is the enharmonic split.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 5,  ChordQuality::Dominant7, -1 }); // F7
            model.setChord (1, 0, Chord { 10, ChordQuality::Major,     -1 }); // Bb

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique != NonDiatonicTechnique::GermanSixth);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
        }

        beginTest ("AugmentedSixth negative: chord without b6 in the bass");
        {
            // C major (C E G) resolving to V has neither the b6 bass nor the #4,
            // so it is not an augmented sixth of any type.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C
            model.setChord (1, 0, Chord { 4, ChordQuality::Major, -1 }); // E (V)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique != NonDiatonicTechnique::ItalianSixth);
            expect (analysis[0].technique != NonDiatonicTechnique::FrenchSixth);
            expect (analysis[0].technique != NonDiatonicTechnique::GermanSixth);
        }

        beginTest ("GermanSixth reaching V through a cadential 6-4");
        {
            // Ger+6 -> i6/4 -> V is the standard approach, used precisely to
            // avoid the parallel fifths of a direct Ger+6 -> V.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 5, ChordQuality::Dominant7, -1 }); // F7 (Ger+6)
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor,      4 }); // Am/E (i6/4)
            model.setChord (2, 0, Chord { 4, ChordQuality::Major,     -1 }); // E (V)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::GermanSixth);
        }

        beginTest ("AugmentedSixth regression: tonic without the 5th in the bass");
        {
            // Am in root position is not a cadential 6-4, so F7 -> Am is a
            // plain bVI7 motion rather than an augmented sixth.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 5, ChordQuality::Dominant7, -1 }); // F7
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique != NonDiatonicTechnique::GermanSixth);
        }

        beginTest ("AugmentedSixth in a major key: It/Fr/Ger on bVI -> V");
        {
            // C major: b6 = Ab, #4 = F#, 2 = D, b3 = Eb.
            const KeyContext cMaj { 0, true };
            const Chord g { 7, ChordQuality::Major, -1 }; // G (V)

            {
                Chord it { 8, ChordQuality::Dominant7, -1 };
                it.omitMask = Omit5;

                ProgressionModel model;
                model.setKey (cMaj);
                model.setNumBars (2);
                model.setChord (0, 0, it);
                model.setChord (1, 0, g);

                const auto analysis = HarmonyAnalyzer::analyzeAll (model);
                expect (analysis[0].technique == NonDiatonicTechnique::ItalianSixth);
            }

            {
                Chord fr { 8, ChordQuality::Dominant7, -1 };
                fr.fifthAlt = FifthAlt::Flat;

                ProgressionModel model;
                model.setKey (cMaj);
                model.setNumBars (2);
                model.setChord (0, 0, fr);
                model.setChord (1, 0, g);

                const auto analysis = HarmonyAnalyzer::analyzeAll (model);
                expect (analysis[0].technique == NonDiatonicTechnique::FrenchSixth);
            }

            {
                ProgressionModel model;
                model.setKey (cMaj);
                model.setNumBars (2);
                model.setChord (0, 0, Chord { 8, ChordQuality::Dominant7, -1 }); // Ab7
                model.setChord (1, 0, g);

                const auto analysis = HarmonyAnalyzer::analyzeAll (model);
                expect (analysis[0].technique == NonDiatonicTechnique::GermanSixth);
            }
        }

        beginTest ("AugmentedSixth from a slash bass: B7b5/F is a French sixth");
        {
            // The b6 need not be the root: any chord whose bass is b6 and whose
            // tones match the Fr+6 set counts (F A B D#).
            Chord fr { 11, ChordQuality::Dominant7, 5 }; // B7b5/F
            fr.fifthAlt = FifthAlt::Flat;

            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, fr);
            model.setChord (1, 0, Chord { 4, ChordQuality::Major, -1 }); // E (V)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::FrenchSixth);
        }

        beginTest ("AugmentedSixth negative: added tension takes it past four tones");
        {
            // F9 = F A C Eb G: five tones, so it is no longer a textbook +6.
            Chord f9 { 5, ChordQuality::Dominant7, -1 };
            f9.addMask = AddNine;

            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, f9);
            model.setChord (1, 0, Chord { 4, ChordQuality::Major, -1 }); // E (V)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique != NonDiatonicTechnique::ItalianSixth);
            expect (analysis[0].technique != NonDiatonicTechnique::FrenchSixth);
            expect (analysis[0].technique != NonDiatonicTechnique::GermanSixth);
        }

        //======================================================================
        // Minor-key techniques previously gated to major keys
        //======================================================================
        beginTest ("DoubleDominant in minor: B7 -> E7 in A minor");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 11, ChordQuality::Dominant7, -1 }); // B7 (II7)
            model.setChord (1, 0, Chord { 4,  ChordQuality::Dominant7, -1 }); // E7 (V7)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::DoubleDominant);
        }

        beginTest ("Minor bVII7 (G7 -> Am in A minor) is plain diatonic");
        {
            // G7 = G B D F, entirely within A natural minor: it is a diatonic
            // chord of the key, not a backdoor borrowing.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7 (bVII7)
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::None);
        }

        beginTest ("BluesSeventh in minor: I7 not resolving down a fifth");
        {
            // A7 -> Am: dominant-quality tonic that does not resolve down a
            // fifth, so it reads as a blues 7th rather than a secondary dominant.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7 (I7)
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::BluesSeventh);
        }

        beginTest ("BluesSeventh regression: A7 -> Dm7 stays Sec.Dom in A minor");
        {
            // Resolving down a fifth to iv makes it a secondary dominant.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (1, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
        }

        beginTest ("Minor borrowed from parallel major: C#m7 (III) in A minor");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 1, ChordQuality::Minor7, -1 }); // C#m7 (III of A major)
            model.setChord (1, 0, Chord { 9, ChordQuality::Minor,  -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::ModalInterchange);
            expectEquals (analysis[0].borrowedScale, juce::String ("A Ionian"));
        }

        //======================================================================
        // Secondary dominant targets on the extended minor scale
        //======================================================================
        beginTest ("SecondaryDominant: final F#7 in A minor targets B (extended)");
        {
            // F#7 -> B: B is the 2nd degree, valid in every minor scale.
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am
            model.setChord (1, 0, Chord { 6, ChordQuality::Dominant7, -1 }); // F#7 (final)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
        }

        beginTest ("SecondaryDominant: final A7 in A minor targets D (iv)");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor,     -1 }); // Am
            model.setChord (1, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7 (final)

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
        }

        //======================================================================
        // Chromatic Mediant
        //======================================================================
        beginTest ("ChromaticMediant: C -> Ab -> C in C major (bVI after tonic)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C
            model.setChord (1, 0, Chord { 8, ChordQuality::Major, -1 }); // Ab
            model.setChord (2, 0, Chord { 0, ChordQuality::Major, -1 }); // C

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::ChromaticMediant);
            expect (analysis[1].label.contains ("bVI"));
        }

        beginTest ("ChromaticMediant: C -> E -> Am in C major (III after tonic)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C
            model.setChord (1, 0, Chord { 4, ChordQuality::Major, -1 }); // E
            model.setChord (2, 0, Chord { 9, ChordQuality::Minor, -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::ChromaticMediant);
            expect (analysis[1].label.contains ("III"));
        }

        beginTest ("ChromaticMediant: Am -> C#m in A minor (minor-minor mediant)");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor, -1 }); // Am
            model.setChord (1, 0, Chord { 1, ChordQuality::Minor, -1 }); // C#m

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::ChromaticMediant);
            expect (analysis[1].label.contains ("III"));
        }

        beginTest ("ChromaticMediant: G7 -> E -> C in C major (diatonic next fallback)");
        {
            // prev (G7) is diatonic but not a mediant partner (dominant
            // quality), so rule 8d must fall through to next (C).
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7
            model.setChord (1, 0, Chord { 4, ChordQuality::Major,     -1 }); // E
            model.setChord (2, 0, Chord { 0, ChordQuality::Major,     -1 }); // C

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::ChromaticMediant);
            expect (analysis[1].label.contains ("III"));
        }

        beginTest ("ChromaticMediant regression: lone Fm in C major stays Subdom.Minor");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (1);
            model.setChord (0, 0, Chord { 5, ChordQuality::Minor, -1 }); // Fm

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SubdominantMinor);
        }

        beginTest ("ChromaticMediant regression: F -> Ab in C major stays Subdom.Minor");
        {
            // prev is not the tonic, so rule 7b must not fire and rule 8
            // (Subdominant Minor) keeps the bVI reading.
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 5, ChordQuality::Major, -1 }); // F
            model.setChord (1, 0, Chord { 8, ChordQuality::Major, -1 }); // Ab

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SubdominantMinor);
        }

        //======================================================================
        // Pedal Point
        //======================================================================
        beginTest ("detectPatterns: Tonic Pedal C - F/C - G/C - C in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,  0 }); // F/C
            model.setChord (2, 0, Chord { 7, ChordQuality::Major,  0 }); // G/C
            model.setChord (3, 0, Chord { 0, ChordQuality::Major, -1 }); // C

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Tonic Pedal")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Dominant Pedal Dm7/G - F/G - G7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7,     7 }); // Dm7/G
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,      7 }); // F/G
            model.setChord (2, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Dominant Pedal")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Pedal negative: C - C7 - C6 without slash stays Cliche");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,     -1 }); // C
            model.setChord (1, 0, Chord { 0, ChordQuality::Dominant7, -1 }); // C7
            model.setChord (2, 0, Chord { 0, ChordQuality::Sixth,     -1 }); // C6

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool foundPedal = false;
            bool foundCliche = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Tonic Pedal" || p.name == "Dominant Pedal"
                    || p.name == "Pedal Point")
                    foundPedal = true;
                if (p.name == "Cliche")
                    foundCliche = true;
            }
            expect (! foundPedal, "same-root run without a slash chord must not be a pedal");
            expect (foundCliche);
        }

        beginTest ("detectPatterns: Pedal negative: two chords C - F/C only");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (2);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major, -1 }); // C
            model.setChord (1, 0, Chord { 5, ChordQuality::Major,  0 }); // F/C

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool foundPedal = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Tonic Pedal" || p.name == "Dominant Pedal"
                    || p.name == "Pedal Point")
                    foundPedal = true;
            }
            expect (! foundPedal, "two chords are too short for a pedal point");
        }

        //======================================================================
        // Constant Structure
        //======================================================================
        beginTest ("detectPatterns: Constant Structure CM7 - EbM7 - AbM7 - DbM7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major7, -1 }); // CM7
            model.setChord (1, 0, Chord { 3, ChordQuality::Major7, -1 }); // EbM7
            model.setChord (2, 0, Chord { 8, ChordQuality::Major7, -1 }); // AbM7
            model.setChord (3, 0, Chord { 1, ChordQuality::Major7, -1 }); // DbM7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Constant Structure")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Constant Structure Dm7 - Em7 - Fm7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7, -1 }); // Em7
            model.setChord (2, 0, Chord { 5, ChordQuality::Minor7, -1 }); // Fm7 (non-diatonic)

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Constant Structure")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
            }
            expect (found);
        }

        beginTest ("detectPatterns: Constant Structure negative: all-diatonic Dm7 - Em7 - Am7");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7, -1 }); // Dm7
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7, -1 }); // Em7
            model.setChord (2, 0, Chord { 9, ChordQuality::Minor7, -1 }); // Am7

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Constant Structure")
                    found = true;
            }
            expect (! found, "an all-diatonic run is a plain sequence, not constant structure");
        }

        beginTest ("detectPatterns: Constant Structure negative: Power5 run C5 - D5 - E5");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Power5, -1 }); // C5
            model.setChord (1, 0, Chord { 2, ChordQuality::Power5, -1 }); // D5
            model.setChord (2, 0, Chord { 4, ChordQuality::Power5, -1 }); // E5

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "Constant Structure")
                    found = true;
            }
            expect (! found, "power chords carry no third, so no constant structure");
        }

        //======================================================================
        // Tonicization (partial modulation) and Secondary Leading Tone
        //======================================================================
        beginTest ("Tonicization: E7 Gm7 A7 Dm7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 4,  ChordQuality::Dominant7, -1 }); // E7
            model.setChord (1, 0, Chord { 7,  ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (2, 0, Chord { 9,  ChordQuality::Dominant7, -1 }); // A7
            model.setChord (3, 0, Chord { 2,  ChordQuality::Minor7,    -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/VI)"));
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::Tonicization);
            expectEquals (analysis[1].label, juce::String ("Tonicization (in Dm)"));
            expect (! analysis[2].diatonic);
            expect (analysis[2].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[2].label, juce::String ("Sec.Dom (V7/II)"));
            expect (analysis[3].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool foundTonicization = false;
            bool foundTwoFiveOne = false;
            bool foundStrong = false;
            bool foundLeading = false;
            for (const auto& p : patterns)
            {
                if (p.name == "4-5-1 in Dm")
                {
                    foundTonicization = true;
                    expectEquals (p.startIndex, 1);
                    expectEquals (p.endIndex, 3);
                }
                if (p.name == "Two-Five-One")
                    foundTwoFiveOne = true;
                if (p.name == "Strong Progression")
                    foundStrong = true;
                if (p.name == "Leading Resolution")
                {
                    foundLeading = true;
                    expectEquals (p.startIndex, 2);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (foundTonicization);
            expect (! foundTwoFiveOne);
            expect (! foundStrong);
            expect (foundLeading);
        }

        beginTest ("Tonicization: Gm7 A7 Dm7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 7, ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (1, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::Tonicization);
            expectEquals (analysis[0].label, juce::String ("Tonicization (in Dm)"));
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[1].label, juce::String ("Sec.Dom (V7/II)"));
            expect (analysis[2].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool foundTonicization = false;
            bool foundStrong = false;
            for (const auto& p : patterns)
            {
                if (p.name == "4-5-1 in Dm")
                {
                    foundTonicization = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
                if (p.name == "Strong Progression")
                    foundStrong = true;
            }
            expect (foundTonicization);
            expect (! foundStrong);
        }

        beginTest ("Tonicization: Gm7 A7 D in C major (major landing)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 7, ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (1, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (2, 0, Chord { 2, ChordQuality::Major,     -1 }); // D

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::Tonicization);
            expectEquals (analysis[0].label, juce::String ("Tonicization (in Dm)"));
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[1].label, juce::String ("Sec.Dom (V7/II)"));
            expect (! analysis[2].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "4-5-1 in D")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
            }
            expect (found);
        }

        beginTest ("Tonicization: E7 Gm7 A7 F in C major (deceptive III)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (1, 0, Chord { 7, ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (2, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (3, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/VI)"));
            expect (analysis[1].technique == NonDiatonicTechnique::Tonicization);
            expectEquals (analysis[1].label, juce::String ("Tonicization (in Dm)"));
            expect (analysis[2].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[2].label, juce::String ("Sec.Dom (V7/II)"));
            expect (analysis[3].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "4-5-3 in Dm")
                {
                    found = true;
                    expectEquals (p.startIndex, 1);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found);
        }

        beginTest ("Tonicization: Gm7 A7 F in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 7, ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (1, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (2, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::Tonicization);
            expectEquals (analysis[0].label, juce::String ("Tonicization (in Dm)"));
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[1].label, juce::String ("Sec.Dom (V7/II)"));
            expect (analysis[2].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "4-5-3 in Dm")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
            }
            expect (found);
        }

        beginTest ("Tonicization: Gm7 A7 Bb in C major (VI landing)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 7,  ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (1, 0, Chord { 9,  ChordQuality::Dominant7, -1 }); // A7
            model.setChord (2, 0, Chord { 10, ChordQuality::Major,     -1 }); // Bb

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::Tonicization);
            expectEquals (analysis[0].label, juce::String ("Tonicization (in Dm)"));
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::Unknown);
            expect (! analysis[2].diatonic);
            expect (analysis[2].technique == NonDiatonicTechnique::SubdominantMinor);
            expectEquals (analysis[2].label, juce::String ("Subdom.Minor"));

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "4-5-6 in Dm")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
            }
            expect (found);
        }

        beginTest ("Tonicization: E7 Em7b5 A7 Dm in C major (ii-form)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7,   -1 }); // E7
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7Flat5, -1 }); // Em7b5
            model.setChord (2, 0, Chord { 9, ChordQuality::Dominant7,   -1 }); // A7
            model.setChord (3, 0, Chord { 2, ChordQuality::Minor,       -1 }); // Dm

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/VI)"));
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::RelatedTwoMinor);
            expectEquals (analysis[1].label, juce::String ("Rel.IIm"));
            expect (analysis[2].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[2].label, juce::String ("Sec.Dom (V7/II)"));
            expect (analysis[3].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "2-5-1 in Dm")
                {
                    found = true;
                    expectEquals (p.startIndex, 1);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found);
        }

        beginTest ("Tonicization: E7 Em7b5 A7 Bb in C major (ii-form deceptive)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 4,  ChordQuality::Dominant7,   -1 }); // E7
            model.setChord (1, 0, Chord { 4,  ChordQuality::Minor7Flat5, -1 }); // Em7b5
            model.setChord (2, 0, Chord { 9,  ChordQuality::Dominant7,   -1 }); // A7
            model.setChord (3, 0, Chord { 10, ChordQuality::Major,       -1 }); // Bb

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/VI)"));
            // Em7b5 is diatonic to C Dorian (iii of the borrowed mode), so with
            // A7 unresolved (Unknown) the run falls through to Modal Interchange.
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::ModalInterchange);
            expectEquals (analysis[1].label, juce::String ("M.I."));
            expect (! analysis[2].diatonic);
            expect (analysis[2].technique == NonDiatonicTechnique::Unknown);
            expect (! analysis[3].diatonic);
            expect (analysis[3].technique == NonDiatonicTechnique::SubdominantMinor);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "2-5-6 in Dm")
                {
                    found = true;
                    expectEquals (p.startIndex, 1);
                    expectEquals (p.endIndex, 3);
                }
            }
            expect (found);
        }

        beginTest ("Tonicization: B7 Dm7 E7 Am in A minor (tonic chain, no chip)");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 11, ChordQuality::Dominant7, -1 }); // B7
            model.setChord (1, 0, Chord { 2,  ChordQuality::Minor7,    -1 }); // Dm7
            model.setChord (2, 0, Chord { 4,  ChordQuality::Dominant7, -1 }); // E7
            model.setChord (3, 0, Chord { 9,  ChordQuality::Minor,     -1 }); // Am

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[0].label, juce::String ("Sec.Dom (V7/V)"));
            expect (analysis[1].diatonic);
            expect (analysis[2].diatonic);
            expect (analysis[2].derivation == MinorDerivation::HarmonicMinor);
            expect (analysis[3].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name.contains (" in Am"))
                    found = true;
            }
            expect (! found);
        }

        beginTest ("Tonicization: Gm7 C7 F in C major (ii-form Rel.IIm + major chip)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 7, ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (1, 0, Chord { 0, ChordQuality::Dominant7, -1 }); // C7
            model.setChord (2, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::RelatedTwoMinor);
            expectEquals (analysis[0].label, juce::String ("Rel.IIm"));
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[1].label, juce::String ("Sec.Dom (V7/IV)"));
            expect (analysis[2].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "2-5-1 in F")
                {
                    found = true;
                    expectEquals (p.startIndex, 0);
                    expectEquals (p.endIndex, 2);
                }
            }
            expect (found);
        }

        beginTest ("Tonicization: Em7 A7 F in C major (diatonic-head gate)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 4, ChordQuality::Minor7,    -1 }); // Em7
            model.setChord (1, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7
            model.setChord (2, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            // Em7 is the diatonic iii7 of C major, so the diatonic-head gate
            // suppresses any temporary-key chip for this run.
            expect (analysis[0].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[1].label, juce::String ("Sec.Dom (V7/II)"));
            expect (analysis[2].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name == "2-5-3 in Dm")
                    found = true;
            }
            expect (! found);
        }

        beginTest ("SecondaryLeadingTone: G D#dim7 Em in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 7, ChordQuality::Major,       -1 }); // G
            model.setChord (1, 0, Chord { 3, ChordQuality::Diminished7, -1 }); // D#dim7
            model.setChord (2, 0, Chord { 4, ChordQuality::Minor,       -1 }); // Em

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryLeadingTone);
            expectEquals (analysis[1].label, juce::String ("Sec.LT (vii/E)"));
            expect (analysis[2].diatonic);
        }

        beginTest ("SecondaryLeadingTone: C F#dim7 G in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,       -1 }); // C
            model.setChord (1, 0, Chord { 6, ChordQuality::Diminished7, -1 }); // F#dim7
            model.setChord (2, 0, Chord { 7, ChordQuality::Major,       -1 }); // G

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryLeadingTone);
            expectEquals (analysis[1].label, juce::String ("Sec.LT (vii/G)"));
            expect (analysis[2].diatonic);
        }

        beginTest ("SecondaryLeadingTone: C F#dim7 G7 in C major");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 0, ChordQuality::Major,       -1 }); // C
            model.setChord (1, 0, Chord { 6, ChordQuality::Diminished7, -1 }); // F#dim7
            model.setChord (2, 0, Chord { 7, ChordQuality::Dominant7,   -1 }); // G7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryLeadingTone);
            expectEquals (analysis[1].label, juce::String ("Sec.LT (vii/G)"));
            expect (analysis[2].diatonic);
        }

        beginTest ("SecondaryLeadingTone: G#dim Em7b5 A7 in C major (delayed V7)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 8, ChordQuality::Diminished,  -1 }); // G#dim
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7Flat5, -1 }); // Em7b5
            model.setChord (2, 0, Chord { 9, ChordQuality::Dominant7,   -1 }); // A7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryLeadingTone);
            expectEquals (analysis[0].label, juce::String ("Sec.LT (vii/A)"));
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::RelatedTwoMinor);
            expectEquals (analysis[1].label, juce::String ("Rel.IIm"));
            expect (analysis[2].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[2].label, juce::String ("Sec.Dom (V7/II)"));
        }

        beginTest ("SecondaryLeadingTone: G#dim Em7b5 A in C major (delayed triad)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 8, ChordQuality::Diminished,  -1 }); // G#dim
            model.setChord (1, 0, Chord { 4, ChordQuality::Minor7Flat5, -1 }); // Em7b5
            model.setChord (2, 0, Chord { 9, ChordQuality::Major,       -1 }); // A

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].technique == NonDiatonicTechnique::SecondaryLeadingTone);
            expectEquals (analysis[0].label, juce::String ("Sec.LT (vii/A)"));
            // Em7b5 is diatonic to C Dorian; A major carries C#, so both stay
            // non-diatonic (Em7b5 via Modal Interchange, A unrecognised).
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::ModalInterchange);
            expectEquals (analysis[1].label, juce::String ("M.I."));
            expect (! analysis[2].diatonic);
            expect (analysis[2].technique == NonDiatonicTechnique::Unknown);
        }

        beginTest ("SecondaryLeadingTone: Am D#dim Dm7 E7 in A minor (Pass.Dim priority)");
        {
            ProgressionModel model;
            model.setKey (aMinor);
            model.setNumBars (4);
            model.setChord (0, 0, Chord { 9, ChordQuality::Minor,      -1 }); // Am
            model.setChord (1, 0, Chord { 3, ChordQuality::Diminished, -1 }); // D#dim
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,     -1 }); // Dm7
            model.setChord (3, 0, Chord { 4, ChordQuality::Dominant7,  -1 }); // E7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (! analysis[1].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::PassingDiminished);
            expectEquals (analysis[1].label, juce::String ("Pass.Dim"));
            expect (analysis[2].diatonic);
            expect (analysis[3].diatonic);
        }

        beginTest ("Tonicization negative: E7 Gm7 A7 in C major (no landing)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (1, 0, Chord { 7, ChordQuality::Minor7,    -1 }); // Gm7
            model.setChord (2, 0, Chord { 9, ChordQuality::Dominant7, -1 }); // A7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::Unknown);
            expect (! analysis[1].diatonic);
            expect (analysis[2].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[2].label, juce::String ("Sec.Dom (V7/II)"));

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name.contains (" in Dm"))
                    found = true;
            }
            expect (! found);
        }

        beginTest ("Tonicization negative: Fm7 G7 Dm7 in C major (R is tonic)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 5, ChordQuality::Minor7,    -1 }); // Fm7
            model.setChord (1, 0, Chord { 7, ChordQuality::Dominant7, -1 }); // G7
            model.setChord (2, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (! analysis[0].diatonic);
            expect (analysis[0].technique == NonDiatonicTechnique::SubdominantMinor);
            expect (analysis[1].diatonic);
            expect (analysis[2].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name.contains (" in Cm"))
                    found = true;
            }
            expect (! found);
        }

        beginTest ("Tonicization negative: Dm7 E7 F in C major (diatonic head gate)");
        {
            ProgressionModel model;
            model.setKey (cMajor);
            model.setNumBars (3);
            model.setChord (0, 0, Chord { 2, ChordQuality::Minor7,    -1 }); // Dm7
            model.setChord (1, 0, Chord { 4, ChordQuality::Dominant7, -1 }); // E7
            model.setChord (2, 0, Chord { 5, ChordQuality::Major,     -1 }); // F

            const auto analysis = HarmonyAnalyzer::analyzeAll (model);
            expect (analysis[0].diatonic);
            expect (analysis[1].technique == NonDiatonicTechnique::SecondaryDominant);
            expectEquals (analysis[1].label, juce::String ("Sec.Dom (V7/VI)"));
            expect (analysis[2].diatonic);

            const auto patterns = HarmonyAnalyzer::detectPatterns (model);
            bool found = false;
            for (const auto& p : patterns)
            {
                if (p.name.contains (" in Am"))
                    found = true;
            }
            expect (! found);
        }
    }
};

static HarmonyAnalyzerTest harmonyAnalyzerTest;
