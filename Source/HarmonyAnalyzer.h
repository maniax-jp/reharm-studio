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
    NeapolitanSixth,       // bII / bIImaj7, typically in first inversion (bII6)
    ItalianSixth,          // It+6: b6, 1, #4 -- resolving to V
    FrenchSixth,           // Fr+6: b6, 1, 2, #4 -- resolving to V
    GermanSixth,           // Ger+6: b6, 1, b3, #4 -- resolving to V
    Unknown                // non-diatonic but no technique recognised
};

/** Which parent scale a minor-key diatonic chord comes from. */
enum class MinorDerivation
{
    None = 0,       // not applicable (major key, or natural minor)
    HarmonicMinor,  // V7, viio7, bIII+ etc.
    MelodicMinor    // IV (major), ii7, VI7 etc.
};

/** Analysis result for a single slot. */
struct ChordAnalysis
{
    bool diatonic = true;
    NonDiatonicTechnique technique = NonDiatonicTechnique::None;
    juce::String label;           // short badge text, e.g. "Sec.Dom (V7/VI)" / "SDM"
    juce::String borrowedScale;   // for ModalInterchange: e.g. "C Dorian"; empty otherwise

    /** For minor keys: set when the chord is diatonic to harmonic/melodic minor
        rather than natural minor. `diatonic` stays true in that case; this only
        records the derivation so the UI can show a subdued origin badge. */
    MinorDerivation derivation = MinorDerivation::None;
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
