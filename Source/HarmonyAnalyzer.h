#pragma once

#include "ProgressionModel.h"

namespace reharm
{

/** How a non-diatonic chord is being used. */
enum class NonDiatonicTechnique
{
    None = 0,              // chord is diatonic
    SecondaryDominant,     // V7/x resolving to a diatonic chord
    DoubleDominant,        // V7/V (doppel dominant)
    TritoneSubstitution,   // bII7 for V7 (or subV/x)
    SubdominantMinor,      // IVm, bVI, bVII etc. in a major key (from parallel minor)
    ModalInterchange,      // borrowed from a parallel mode (borrowedScale tells which)
    PassingDiminished,     // dim7 chromatically connecting two diatonic chords
    PicardyThird,          // major tonic at the end of a minor-key progression
    RelatedTwoMinor,       // IIm7 paired with a following secondary dominant
    CommonToneDiminished,  // dim7 sharing its root with the following chord
    BluesSeventh,          // non-functional I7 / IV7 (blues)
    BackdoorDominant,      // bVII7 resolving to I
    ChromaticApproach,     // same-quality chord a semitone from the next chord
    LineCliche,            // passing chord harmonizing a single chromatic voice line
    Unknown                // non-diatonic but no technique recognised
};

/** Analysis result for a single slot. */
struct ChordAnalysis
{
    bool diatonic = true;
    NonDiatonicTechnique technique = NonDiatonicTechnique::None;
    juce::String label;           // short badge text, e.g. "Sec.Dom (V7/VI)" / "SDM"
    juce::String borrowedScale;   // for ModalInterchange: e.g. "C Dorian"; empty otherwise
};

/** A named pattern found in the progression (indices into flatten()). */
struct DetectedPattern
{
    juce::String name;            // e.g. Two-Five-One, Royal Road, Cliche, Strong Cadence
    int startIndex = 0;           // inclusive, index into ProgressionModel::flatten()
    int endIndex = 0;             // inclusive
};

/** A replacement suggestion for a chord. */
struct SubstitutionCandidate
{
    Chord chord;
    juce::String label;           // e.g. "Dairi (VIm7)" / "Ura code (bII7)"
};

/**
 * Static harmonic analysis over a ProgressionModel:
 *  - per-chord diatonic / non-diatonic technique classification
 *  - named pattern detection (two-five-one, strong/leading progressions,
 *    cliche lines, and the built-in famous progressions)
 *  - substitution candidates (diatonic substitutes and tritone subs)
 */
class HarmonyAnalyzer
{
public:
    /** One analysis per flatten() entry (rests get a default diatonic entry). */
    static std::vector<ChordAnalysis> analyzeAll (const ProgressionModel& model);

    /** All named patterns found, in ascending startIndex order. */
    static std::vector<DetectedPattern> detectPatterns (const ProgressionModel& model);

    /** Substitution candidates for a chord in a key, best first.
        Includes diatonic function substitutes (dairi) and the tritone sub
        (ura code) when the chord is dominant-quality. */
    static std::vector<SubstitutionCandidate> substitutions (const Chord& chord,
                                                             const KeyContext& key);
};

} // namespace reharm
