#include "HarmonyAnalyzer.h"

#include <algorithm>
#include <array>
#include <set>

namespace reharm
{

namespace
{

int normalizePc (int pc) noexcept
{
    return ((pc % 12) + 12) % 12;
}

int rootOffset (const Chord& c, const KeyContext& key) noexcept
{
    return normalizePc (c.rootPitchClass - key.tonicPitchClass);
}
// The pitch class that actually anchors the bottom of the chord: the slash-chord
// bass when present, otherwise the root. Used for chromatic passing-tone judgments
// (e.g. passing diminished) where the audible bass line matters, not the harmonic root.
int bassAnchorPitchClass (const Chord& c) noexcept
{
    return normalizePc (c.hasBass() ? c.bassPitchClass : c.rootPitchClass);
}

// Offset relative to the preset reference tonic (relative major for minor keys).
int presetRootOffset (const Chord& c, const KeyContext& key) noexcept
{
    const int ref = key.isMajor ? key.tonicPitchClass : (key.tonicPitchClass + 3) % 12;
    return normalizePc (c.rootPitchClass - ref);
}

bool isDominantQuality (ChordQuality q) noexcept
{
    return q == ChordQuality::Dominant7
        || q == ChordQuality::Dominant7Sus4
        || q == ChordQuality::Augmented7;
}

bool isPicardyQuality (ChordQuality q) noexcept
{
    return q == ChordQuality::Major
        || q == ChordQuality::Major7
        || q == ChordQuality::Sixth;
}

bool isDiminishedQuality (ChordQuality q) noexcept
{
    return q == ChordQuality::Diminished
        || q == ChordQuality::Diminished7;
}

bool isRelatedTwoQuality (ChordQuality q) noexcept
{
    return q == ChordQuality::Minor7
        || q == ChordQuality::Minor7Flat5;
}

// If a slash chord can be reinterpreted as a sus-type dominant rooted at its
// bass note (e.g. Em7/A -> A7sus, F/G -> G9sus), return that bass pitch class.
// Otherwise nullopt.
std::optional<int> susDominantReinterpretationRoot (const Chord& c) noexcept
{
    if (! c.hasBass())
        return std::nullopt;

    const int bassRel = normalizePc (c.bassPitchClass - c.rootPitchClass);
    const bool minorOverFourth = (c.quality == ChordQuality::Minor || c.quality == ChordQuality::Minor7)
                                  && bassRel == 5;   // Em7/A type
    const bool majorOverSecond = (c.quality == ChordQuality::Major || c.quality == ChordQuality::Major7)
                                  && bassRel == 2;   // F/G type

    if (minorOverFourth || majorOverSecond)
        return normalizePc (c.bassPitchClass);

    return std::nullopt;
}

// Functional root: the sus-dominant reinterpretation root if present, else the
// chord's own root.
int functionalRootPitchClass (const Chord& c) noexcept
{
    const auto susRoot = susDominantReinterpretationRoot (c);
    return susRoot.has_value() ? *susRoot : normalizePc (c.rootPitchClass);
}

// True if the chord has dominant function, either by quality or by sus
// reinterpretation of a slash chord.
bool hasDominantFunction (const Chord& c) noexcept
{
    return isDominantQuality (c.quality) || susDominantReinterpretationRoot (c).has_value();
}

// Preset matching treats maj/M7 and m/m7 as interchangeable.
ChordQuality presetCanonicalQuality (ChordQuality q) noexcept
{
    if (q == ChordQuality::Major7)
        return ChordQuality::Major;
    if (q == ChordQuality::Minor7)
        return ChordQuality::Minor;
    return q;
}

bool qualityMatchesPreset (ChordQuality actual, ChordQuality expected) noexcept
{
    if (presetCanonicalQuality (actual) == presetCanonicalQuality (expected))
        return true;

    // Directional relaxations: a preset 7th slot may be played as a plain
    // major triad, and a preset m7b5 slot as a diminished triad. The reverse
    // (preset maj matched by an actual 7th) must NOT hold.
    if (expected == ChordQuality::Dominant7 && actual == ChordQuality::Major)
        return true;
    if (expected == ChordQuality::Minor7Flat5 && actual == ChordQuality::Diminished)
        return true;

    return false;
}

struct RealChord
{
    int flatIndex = 0;
    Chord chord;
};

std::vector<RealChord> collectRealChords (const std::vector<FlatChord>& flat)
{
    std::vector<RealChord> real;
    real.reserve (flat.size());

    for (int i = 0; i < static_cast<int> (flat.size()); ++i)
    {
        if (flat[static_cast<size_t> (i)].chord.has_value())
            real.push_back ({ i, *flat[static_cast<size_t> (i)].chord });
    }

    return real;
}

bool fitsScale (const Chord& c, int tonic, const int* intervals, int count)
{
    std::array<bool, 12> inScale {};
    for (int i = 0; i < count; ++i)
        inScale[static_cast<size_t> (normalizePc (tonic + intervals[i]))] = true;

    for (int pc : ChordModel::chordTonePitchClasses (c))
    {
        if (! inScale[static_cast<size_t> (normalizePc (pc))])
            return false;
    }

    if (c.hasBass() && ! inScale[static_cast<size_t> (normalizePc (c.bassPitchClass))])
        return false;

    return true;
}

// Line cliche: a passing chord harmonizing a single chromatic voice line -- one
// voice moves prev -> x -> next by semitone in the same direction, while every
// other chord tone is shared with both neighbours.
bool matchesLineCliche (const Chord& c,
                        const std::optional<Chord>& prev,
                        const std::optional<Chord>& next)
{
    if (! prev.has_value() || ! next.has_value())
        return false;

    const auto currentTones = ChordModel::chordTonePitchClasses (c);

    if (currentTones.size() < 3)
        return false;

    std::set<int> P, C, N;
    for (int pc : ChordModel::chordTonePitchClasses (*prev)) P.insert (normalizePc (pc));
    for (int pc : currentTones)                              C.insert (normalizePc (pc));
    for (int pc : ChordModel::chordTonePitchClasses (*next)) N.insert (normalizePc (pc));

    std::vector<int> diffP, diffN;
    std::set_difference (C.begin(), C.end(), P.begin(), P.end(), std::back_inserter (diffP));
    std::set_difference (C.begin(), C.end(), N.begin(), N.end(), std::back_inserter (diffN));

    if (diffP.size() != 1 || diffN.size() != 1 || diffP[0] != diffN[0])
        return false;

    const int x = diffP[0];

    for (int d : { 1, -1 })
    {
        if (P.count (normalizePc (x - d)) != 0 && N.count (normalizePc (x + d)) != 0)
            return true;
    }

    return false;
}

// Neapolitan sixth: the major triad (or maj7) on b2, most often heard in first
// inversion (bII6, i.e. a b2 chord over the 4th degree) and resolving towards
// the dominant or the tonic. Dominant-quality b2 chords are excluded -- bII7 is
// a tritone substitute and is handled by that rule instead.
bool matchesNeapolitan (const Chord& c,
                        const std::optional<Chord>& next,
                        const KeyContext& key)
{
    if (rootOffset (c, key) != 1)
        return false;

    if (c.quality != ChordQuality::Major && c.quality != ChordQuality::Major7)
        return false;

    // No following chord: accept on colour alone (the b2 major triad is not
    // diatonic to either mode, so there is no competing reading).
    if (! next.has_value())
        return true;

    // Classic resolutions: to V (often via a cadential 6-4) or straight to i.
    const int nextOffset = rootOffset (*next, key);
    return nextOffset == 7 || nextOffset == 0;
}

// Augmented sixth chords, detected by pitch content rather than spelling: the
// model has no dedicated +6 quality, and b6 + #4 is enharmonically a dominant
// 7th / augmented 6th pair.
//
// All three types share the b6 bass and the #4 (the augmented sixth above it):
//   It+6 = b6, 1, #4        Fr+6 = b6, 1, 2, #4        Ger+6 = b6, 1, b3, #4
// Ger+6 is enharmonically identical to bVI7, so the resolution target
// disambiguates: a +6 resolves to V, whereas bVI7 as a secondary dominant would
// resolve down a fifth to bII.
bool matchesAugmentedSixth (const Chord& c,
                            const std::optional<Chord>& next,
                            const KeyContext& key)
{
    // Must resolve to the dominant -- this is what makes it a +6 rather than
    // an enharmonically identical bVI7.
    if (! next.has_value() || rootOffset (*next, key) != 7)
        return false;

    const int tonic = key.tonicPitchClass;
    const int flatSix = normalizePc (tonic + 8);
    const int sharpFour = normalizePc (tonic + 6);

    // The b6 must be in the bass (explicit slash bass, or the root itself).
    const int bass = c.hasBass() ? normalizePc (c.bassPitchClass)
                                 : normalizePc (c.rootPitchClass);
    if (bass != flatSix)
        return false;

    std::set<int> tones;
    for (int pc : ChordModel::chordTonePitchClasses (c))
        tones.insert (normalizePc (pc));

    // b6 and #4 form the defining augmented sixth interval; 1 is always present.
    return tones.count (flatSix) != 0
        && tones.count (sharpFour) != 0
        && tones.count (normalizePc (tonic)) != 0;
}

// Minor-key extended diatonic: chords built on harmonic or melodic minor are
// part of the key's normal vocabulary, not borrowings. They stay diatonic; the
// derivation is recorded so the UI can show a subdued origin badge.
//
// Only chords that actually use the raised degree are reported: a chord that
// already fits natural minor is plain diatonic and never reaches here, and one
// that fits both raised scales is attributed to harmonic minor (the more
// common source for functional harmony).
MinorDerivation minorDerivationFor (const Chord& c, const KeyContext& key)
{
    static const int harmonicMinor[] = { 0, 2, 3, 5, 7, 8, 11 };
    static const int melodicMinor[]  = { 0, 2, 3, 5, 7, 9, 11 };

    if (key.isMajor)
        return MinorDerivation::None;

    const int tonic = key.tonicPitchClass;

    if (fitsScale (c, tonic, harmonicMinor, 7))
        return MinorDerivation::HarmonicMinor;

    if (fitsScale (c, tonic, melodicMinor, 7))
        return MinorDerivation::MelodicMinor;

    return MinorDerivation::None;
}

struct ModeDef
{
    const char* name;
    const int* intervals;
    int count;
};

// Returns empty analysis if no mode matches.
ChordAnalysis tryModalInterchange (const Chord& c, const KeyContext& key)
{
    static const int ionian[]         = { 0, 2, 4, 5, 7, 9, 11 };
    static const int dorian[]         = { 0, 2, 3, 5, 7, 9, 10 };
    static const int phrygian[]       = { 0, 1, 3, 5, 7, 8, 10 };
    static const int lydian[]         = { 0, 2, 4, 6, 7, 9, 11 };
    static const int mixolydian[]     = { 0, 2, 4, 5, 7, 9, 10 };
    static const int aeolian[]        = { 0, 2, 3, 5, 7, 8, 10 };
    static const int locrian[]        = { 0, 1, 3, 5, 6, 8, 10 };
    static const int harmonicMinor[]  = { 0, 2, 3, 5, 7, 8, 11 };
    static const int melodicMinor[]   = { 0, 2, 3, 5, 7, 9, 11 };

    // Major key: Natural minor (Aeolian), Dorian, Phrygian, Lydian, Mixolydian, Locrian
    static const ModeDef majorModes[] = {
        { "Aeolian",    aeolian,       7 },
        { "Dorian",     dorian,        7 },
        { "Phrygian",   phrygian,      7 },
        { "Lydian",     lydian,        7 },
        { "Mixolydian", mixolydian,    7 },
        { "Locrian",    locrian,       7 },
    };

    // Minor key: Major (Ionian), Dorian, Phrygian, Lydian, Mixolydian,
    // Harmonic minor, Melodic minor
    static const ModeDef minorModes[] = {
        { "Ionian",         ionian,        7 },
        { "Dorian",         dorian,        7 },
        { "Phrygian",       phrygian,      7 },
        { "Lydian",         lydian,        7 },
        { "Mixolydian",     mixolydian,    7 },
        { "Harmonic minor", harmonicMinor, 7 },
        { "Melodic minor",  melodicMinor,  7 },
    };

    const ModeDef* modes = key.isMajor ? majorModes : minorModes;
    const int numModes = key.isMajor ? 6 : 7;
    const int tonic = key.tonicPitchClass;

    for (int m = 0; m < numModes; ++m)
    {
        if (fitsScale (c, tonic, modes[m].intervals, modes[m].count))
        {
            ChordAnalysis a;
            a.diatonic = false;
            a.technique = NonDiatonicTechnique::ModalInterchange;
            a.label = "M.I.";
            a.borrowedScale = ChordModel::pitchClassName (tonic) + " " + modes[m].name;
            return a;
        }
    }

    return {};
}

// Forward: classify one real chord given the real-chord sequence.
ChordAnalysis classifyReal (const std::vector<RealChord>& real,
                            int realIndex,
                            const KeyContext& key);

// Is `pitchClass` a usable degree of the key, i.e. a plausible target for a
// secondary dominant? Minor keys use the union of natural, harmonic and melodic
// minor, so that V (raised 7th's dominant), IV major and the leading tone all
// count -- V7/V in a minor key is common and must not be rejected because the
// natural minor lacks those degrees.
bool isKeyDegree (int pitchClass, const KeyContext& key)
{
    static const int majorScale[]   = { 0, 2, 4, 5, 7, 9, 11 };
    // Natural + harmonic + melodic minor combined: 1 2 b3 4 5 b6 6 b7 7
    static const int minorScale[]   = { 0, 2, 3, 5, 7, 8, 9, 10, 11 };

    const int* scale = key.isMajor ? majorScale : minorScale;
    const int count  = key.isMajor ? 7 : 9;

    for (int i = 0; i < count; ++i)
    {
        if (normalizePc (key.tonicPitchClass + scale[i]) == normalizePc (pitchClass))
            return true;
    }

    return false;
}

bool matchesSecondaryDominant (const Chord& c,
                               const std::optional<Chord>& next,
                               const KeyContext& key)
{
    if (! hasDominantFunction (c))
        return false;

    const int functionalRoot = functionalRootPitchClass (c);

    // Case D (final chord, no next): assume resolution down a perfect fifth.
    if (! next.has_value())
    {
        const int targetRoot = normalizePc (functionalRoot + 5);

        // Must not resolve to tonic.
        if (normalizePc (targetRoot - key.tonicPitchClass) == 0)
            return false;

        // Target must be a degree of the key.
        return isKeyDegree (targetRoot, key);
    }

    const int nextFunctionalRoot = functionalRootPitchClass (*next);

    // Case A: resolve down a perfect fifth (not to tonic). The target need not
    // be diatonic -- V7/bIII and V7/bVII resolve onto borrowed chords.
    if (normalizePc (functionalRoot + 5) == nextFunctionalRoot
        && normalizePc (nextFunctionalRoot - key.tonicPitchClass) != 0)
        return true;

    // Case B: III7 -> IV (deceptive resolution).
    if (normalizePc (functionalRoot - key.tonicPitchClass) == 4 && rootOffset (*next, key) == 5)
        return true;

    // Case C: VI7 -> IV (deceptive resolution).
    if (normalizePc (functionalRoot - key.tonicPitchClass) == 9 && rootOffset (*next, key) == 5)
        return true;

    return false;
}

bool matchesTritoneSubstitution (const Chord& c,
                                 const std::optional<Chord>& next,
                                 const KeyContext& key)
{
    if (! hasDominantFunction (c))
        return false;

    const int functionalRoot = functionalRootPitchClass (c);

    if (normalizePc (functionalRoot - key.tonicPitchClass) == 1)
        return true;

    // Half-step descent to next root.
    if (next.has_value()
        && normalizePc (functionalRoot + 11) == functionalRootPitchClass (*next))
        return true;

    return false;
}

std::optional<Chord> nextRealChord (const std::vector<RealChord>& real, int realIndex)
{
    if (realIndex + 1 < static_cast<int> (real.size()))
        return real[static_cast<size_t> (realIndex + 1)].chord;
    return std::nullopt;
}

std::optional<Chord> prevRealChord (const std::vector<RealChord>& real, int realIndex)
{
    if (realIndex > 0)
        return real[static_cast<size_t> (realIndex - 1)].chord;
    return std::nullopt;
}

ChordAnalysis classifyReal (const std::vector<RealChord>& real,
                            int realIndex,
                            const KeyContext& key)
{
    const Chord& c = real[static_cast<size_t> (realIndex)].chord;
    const int offset = rootOffset (c, key);
    const auto next = nextRealChord (real, realIndex);
    const auto prev = prevRealChord (real, realIndex);

    // 1. Diatonic (natural minor / major)
    if (ChordModel::isDiatonic (c, key))
        return { true, NonDiatonicTechnique::None, {}, {} };

    // 2. Picardy Third
    if (! key.isMajor
        && realIndex == static_cast<int> (real.size()) - 1
        && offset == 0
        && isPicardyQuality (c.quality))
    {
        return { false, NonDiatonicTechnique::PicardyThird, "Picardy", {} };
    }

    // 2b. Minor-key extended diatonic (harmonic / melodic minor).
    // Runs before the technique rules so that the key's own vocabulary --
    // V7, viio7, IV major, iim7b5 -- is not mislabelled as a passing
    // diminished, secondary dominant or modal interchange.
    //
    // A chord that harmonizes a chromatic voice line is excluded: chords like
    // CmM7 in C minor fit harmonic minor, but when the neighbours make the
    // line audible (Cm - CmM7 - Cm7) the line reading is the meaningful one,
    // so it is left to rule 8c.
    if (! key.isMajor && ! matchesLineCliche (c, prev, next))
    {
        const MinorDerivation derivation = minorDerivationFor (c, key);

        if (derivation != MinorDerivation::None)
        {
            ChordAnalysis a;
            a.diatonic = true;
            a.technique = NonDiatonicTechnique::None;
            a.borrowedScale = ChordModel::pitchClassName (key.tonicPitchClass)
                            + (derivation == MinorDerivation::HarmonicMinor
                                   ? " Harmonic minor"
                                   : " Melodic minor");
            a.derivation = derivation;
            return a;
        }
    }

    // 2c. Augmented Sixth (It/Fr/Ger on b6, resolving to V).
    // Before the dominant-function rules: Ger+6 is enharmonically bVI7 and
    // would otherwise be taken as a secondary dominant or tritone sub.
    if (matchesAugmentedSixth (c, next, key))
        return { false, NonDiatonicTechnique::AugmentedSixth, "Aug.6th", {} };

    // 2d. Neapolitan Sixth (bII / bIImaj7). Before the tritone-sub rule, which
    // also keys on the b2 degree; dominant-quality bII7 is left to that rule.
    if (matchesNeapolitan (c, next, key))
        return { false, NonDiatonicTechnique::NeapolitanSixth, "Neapolitan", {} };

    // 3. Double Dominant (II7). In a major key the II7 colour alone is enough;
    // in a minor key the dominant is itself an altered chord, so require the
    // chord to actually land on V before calling it a double dominant --
    // otherwise a lone II7 is better described as a secondary dominant.
    if (isDominantQuality (c.quality) && offset == 2
        && (key.isMajor
            || (next.has_value() && rootOffset (*next, key) == 7)))
        return { false, NonDiatonicTechnique::DoubleDominant, "Dbl.Dom", {} };

    // 4. Secondary Dominant
    if (matchesSecondaryDominant (c, next, key))
    {
        // Roman numeral is the target degree of V7/x (functional root + perfect fifth).
        const juce::String roman = ChordModel::degreeRoman (functionalRootPitchClass (c) + 5, key);
        return { false, NonDiatonicTechnique::SecondaryDominant,
                 "Sec.Dom (V7/" + roman + ")", {} };
    }

    // 5. Tritone Substitution
    if (matchesTritoneSubstitution (c, next, key))
        return { false, NonDiatonicTechnique::TritoneSubstitution, "Tritone Sub", {} };

    // 5b. Backdoor Dominant (bVII7 -> I). Major keys only by nature: in a minor
    // key bVII7 is built entirely from natural-minor degrees, so it is already
    // diatonic and never reaches this point.
    if (key.isMajor && isDominantQuality (c.quality) && offset == 10 && next.has_value()
        && normalizePc (functionalRootPitchClass (*next) - key.tonicPitchClass) == 0
        && (next->quality == ChordQuality::Major
            || next->quality == ChordQuality::Major7
            || next->quality == ChordQuality::Sixth))
    {
        return { false, NonDiatonicTechnique::BackdoorDominant, "Backdoor (bVII7)", {} };
    }

    // 5c. Blues Seventh (non-functional I7 / IV7), in either mode: a minor
    // blues uses the same dominant-quality tonic and subdominant.
    if (isDominantQuality (c.quality) && (offset == 0 || offset == 5))
        return { false, NonDiatonicTechnique::BluesSeventh, "Blues 7th", {} };

    // 6. Passing Diminished
    // Ascending: dim root is a chromatic step above prev and/or below next.
    // Descending: dim root is a chromatic step above next (prev is ignored).
    if (isDiminishedQuality (c.quality))
    {
        const bool hasPrev = prev.has_value();
        const bool hasNext = next.has_value();
        const bool matchPrev = hasPrev
            && normalizePc (c.rootPitchClass) == normalizePc (bassAnchorPitchClass (*prev) + 1);
        const bool matchNext = hasNext
            && normalizePc (c.rootPitchClass) == normalizePc (bassAnchorPitchClass (*next) + 11);

        bool pass = false;
        if (hasPrev && hasNext)
            pass = matchPrev && matchNext;
        else if (hasPrev)
            pass = matchPrev;
        else if (hasNext)
            pass = matchNext;

        // Descending: dim root == next root + 1 (requires next; prev irrelevant).
        const bool matchNextDesc = hasNext
            && normalizePc (c.rootPitchClass) == normalizePc (bassAnchorPitchClass (*next) + 1);
        pass = pass || matchNextDesc;

        if (pass)
            return { false, NonDiatonicTechnique::PassingDiminished, "Pass.Dim", {} };

        // 6b. Common-tone diminished: same root as next (or prev when trailing).
        if ((hasNext && normalizePc (next->rootPitchClass) == normalizePc (c.rootPitchClass))
            || (! hasNext && hasPrev
                && normalizePc (prev->rootPitchClass) == normalizePc (c.rootPitchClass)))
        {
            return { false, NonDiatonicTechnique::CommonToneDiminished, "C.T.Dim", {} };
        }
    }

    // 7. Related II minor (look-ahead: next is Sec.Dom or Tritone Sub)
    if (isRelatedTwoQuality (c.quality) && next.has_value()
        && normalizePc (c.rootPitchClass + 5) == functionalRootPitchClass (*next))
    {
        // Classify next with full priority (without depending on this chord's result).
        const ChordAnalysis nextAnalysis = classifyReal (real, realIndex + 1, key);
        if (nextAnalysis.technique == NonDiatonicTechnique::SecondaryDominant
            || nextAnalysis.technique == NonDiatonicTechnique::TritoneSubstitution)
        {
            return { false, NonDiatonicTechnique::RelatedTwoMinor, "Rel.IIm", {} };
        }
    }

    // 8. Subdominant Minor (major key): IVm, bVI, bVII, IIm7b5 from the
    // parallel minor.
    if (key.isMajor)
    {
        bool isSdm = false;

        if (offset == 5
            && (c.quality == ChordQuality::Minor
                || c.quality == ChordQuality::Minor7
                || c.quality == ChordQuality::MinorMajor7))
            isSdm = true;
        else if (offset == 8
                 && (c.quality == ChordQuality::Major
                     || c.quality == ChordQuality::Major7))
            isSdm = true;
        else if (offset == 10
                 && (c.quality == ChordQuality::Major
                     || c.quality == ChordQuality::Dominant7))
            isSdm = true;
        else if (offset == 2 && c.quality == ChordQuality::Minor7Flat5)
            isSdm = true;

        if (isSdm)
        {
            ChordAnalysis a;
            a.diatonic = false;
            a.technique = NonDiatonicTechnique::SubdominantMinor;
            a.label = "Subdom.Minor";
            a.borrowedScale = ChordModel::pitchClassName (key.tonicPitchClass) + " Natural minor";
            return a;
        }
    }
    else
    {
        // Minor key: chords borrowed from the parallel major. The raised-degree
        // chords (V7, viio7, IV major) are already handled as extended diatonic
        // by rule 2b, so what remains are the major-mode 3rd, 6th and 7th
        // degrees -- III minor, VI minor and the major-mode VII.
        bool isBorrowed = false;

        if (offset == 4
            && (c.quality == ChordQuality::Minor
                || c.quality == ChordQuality::Minor7))
            isBorrowed = true;
        else if (offset == 9
                 && (c.quality == ChordQuality::Minor
                     || c.quality == ChordQuality::Minor7))
            isBorrowed = true;
        else if (offset == 11 && c.quality == ChordQuality::Minor7Flat5)
            isBorrowed = true;

        if (isBorrowed)
        {
            ChordAnalysis a;
            a.diatonic = false;
            a.technique = NonDiatonicTechnique::ModalInterchange;
            a.label = "M.I.";
            a.borrowedScale = ChordModel::pitchClassName (key.tonicPitchClass) + " Ionian";
            return a;
        }
    }

    // 8b. Chromatic Approach (same-quality chord a semitone above/below next)
    if (next.has_value() && c.quality == next->quality
        && (normalizePc (c.rootPitchClass + 1) == normalizePc (next->rootPitchClass)
            || normalizePc (c.rootPitchClass - 1) == normalizePc (next->rootPitchClass)))
    {
        return { false, NonDiatonicTechnique::ChromaticApproach, "Chr.App", {} };
    }

    // 8c. Line Cliche
    if (matchesLineCliche (c, prev, next))
        return { false, NonDiatonicTechnique::LineCliche, "Line Cliche", {} };

    // 9. Modal Interchange
    {
        const ChordAnalysis mi = tryModalInterchange (c, key);
        if (mi.technique == NonDiatonicTechnique::ModalInterchange)
            return mi;
    }

    // 10. Unknown
    return { false, NonDiatonicTechnique::Unknown, "Non-diatonic", {} };
}

ChordQuality diatonicSeventhQualityMajor (int offset) noexcept
{
    switch (offset)
    {
        case 0:  return ChordQuality::Major7;
        case 2:  return ChordQuality::Minor7;
        case 4:  return ChordQuality::Minor7;
        case 5:  return ChordQuality::Major7;
        case 7:  return ChordQuality::Dominant7;
        case 9:  return ChordQuality::Minor7;
        case 11: return ChordQuality::Minor7Flat5;
        default: return ChordQuality::Major7;
    }
}

ChordQuality diatonicSeventhQualityMinor (int offset) noexcept
{
    switch (offset)
    {
        case 0:  return ChordQuality::Minor7;
        case 3:  return ChordQuality::Major7;
        case 5:  return ChordQuality::Minor7;
        case 7:  return ChordQuality::Minor7;
        case 8:  return ChordQuality::Major7;
        case 10: return ChordQuality::Dominant7;
        default: return ChordQuality::Minor7;
    }
}

void appendCandidate (std::vector<SubstitutionCandidate>& out,
                      const Chord& original,
                      const Chord& candidate,
                      const juce::String& label)
{
    if (out.size() >= 8)
        return;

    if (candidate == original)
        return;

    for (const auto& existing : out)
    {
        if (existing.chord == candidate)
            return;
    }

    out.push_back ({ candidate, label });
}

} // namespace

//==============================================================================
std::vector<ChordAnalysis> HarmonyAnalyzer::analyzeAll (const ProgressionModel& model)
{
    const auto flat = model.flatten();
    const auto& key = model.getKey();
    const auto real = collectRealChords (flat);

    std::vector<ChordAnalysis> result (flat.size(),
                                       ChordAnalysis { true, NonDiatonicTechnique::None, {}, {} });

    // Map flat index -> real index for classification.
    for (int r = 0; r < static_cast<int> (real.size()); ++r)
    {
        const int flatIdx = real[static_cast<size_t> (r)].flatIndex;
        result[static_cast<size_t> (flatIdx)] = classifyReal (real, r, key);
    }

    return result;
}

//==============================================================================
std::vector<DetectedPattern> HarmonyAnalyzer::detectPatterns (const ProgressionModel& model)
{
    const auto flat = model.flatten();
    const auto& key = model.getKey();
    const auto real = collectRealChords (flat);

    std::vector<DetectedPattern> patterns;

    if (real.empty())
        return patterns;

    // ---- 1. Two-Five-One ----
    std::vector<std::pair<int, int>> twoFiveOneRanges; // real-index [start, end] inclusive

    for (int i = 0; i + 2 < static_cast<int> (real.size()); ++i)
    {
        const auto& a = real[static_cast<size_t> (i)].chord;
        const auto& b = real[static_cast<size_t> (i + 1)].chord;
        const auto& c = real[static_cast<size_t> (i + 2)].chord;

        const int step1 = normalizePc (b.rootPitchClass - a.rootPitchClass);
        const int step2 = normalizePc (c.rootPitchClass - b.rootPitchClass);

        if (step1 != 5 || step2 != 5)
            continue;

        if (! isRelatedTwoQuality (a.quality))
            continue;

        if (! isDominantQuality (b.quality))
            continue;

        if (rootOffset (c, key) != 0)
            continue;

        patterns.push_back ({
            "Two-Five-One",
            real[static_cast<size_t> (i)].flatIndex,
            real[static_cast<size_t> (i + 2)].flatIndex
        });
        twoFiveOneRanges.push_back ({ i, i + 2 });
    }

    auto coveredByTwoFiveOne = [&] (int realA, int realB) -> bool
    {
        // A pair of adjacent real chords is covered if both lie inside some 2-5-1.
        for (const auto& range : twoFiveOneRanges)
        {
            if (realA >= range.first && realB <= range.second)
                return true;
        }
        return false;
    };

    // ---- 2. Strong Progression (+5 chains, excluding 2-5-1 pairs) ----
    {
        int i = 0;
        while (i + 1 < static_cast<int> (real.size()))
        {
            // Find longest chain of +5 steps starting at i, skipping pairs inside 2-5-1.
            if (normalizePc (real[static_cast<size_t> (i + 1)].chord.rootPitchClass
                             - real[static_cast<size_t> (i)].chord.rootPitchClass) != 5
                || coveredByTwoFiveOne (i, i + 1))
            {
                ++i;
                continue;
            }

            int end = i + 1;
            while (end + 1 < static_cast<int> (real.size())
                   && normalizePc (real[static_cast<size_t> (end + 1)].chord.rootPitchClass
                                   - real[static_cast<size_t> (end)].chord.rootPitchClass) == 5
                   && ! coveredByTwoFiveOne (end, end + 1))
            {
                ++end;
            }

            patterns.push_back ({
                "Strong Progression",
                real[static_cast<size_t> (i)].flatIndex,
                real[static_cast<size_t> (end)].flatIndex
            });
            i = end;
        }
    }

    // ---- 3. Cliche (same root, 3+ consecutive, all distinct qualities) ----
    {
        int i = 0;
        while (i < static_cast<int> (real.size()))
        {
            const int root = real[static_cast<size_t> (i)].chord.rootPitchClass;
            int j = i + 1;
            while (j < static_cast<int> (real.size())
                   && real[static_cast<size_t> (j)].chord.rootPitchClass == root)
                ++j;

            const int len = j - i;
            if (len >= 3)
            {
                std::set<ChordQuality> qualities;
                bool allDistinct = true;
                for (int k = i; k < j; ++k)
                {
                    const auto q = real[static_cast<size_t> (k)].chord.quality;
                    if (! qualities.insert (q).second)
                    {
                        allDistinct = false;
                        break;
                    }
                }

                if (allDistinct)
                {
                    patterns.push_back ({
                        "Cliche",
                        real[static_cast<size_t> (i)].flatIndex,
                        real[static_cast<size_t> (j - 1)].flatIndex
                    });
                }
            }

            i = j;
        }
    }

    // ---- 4. Leading Resolution ----
    for (int i = 0; i + 1 < static_cast<int> (real.size()); ++i)
    {
        const auto& a = real[static_cast<size_t> (i)].chord;
        const auto& b = real[static_cast<size_t> (i + 1)].chord;

        if (! hasDominantFunction (a))
            continue;

        if (hasDominantFunction (b))
            continue;

        const int step = normalizePc (functionalRootPitchClass (b) - functionalRootPitchClass (a));
        if (step != 5 && step != 11)
            continue;

        patterns.push_back ({
            "Leading Resolution",
            real[static_cast<size_t> (i)].flatIndex,
            real[static_cast<size_t> (i + 1)].flatIndex
        });
    }

    // ---- 5. Famous progression presets (consecutive substring match) ----
    {
        const auto& presets = ProgressionPresets::all();
        for (const auto& preset : presets)
        {
            const int n = static_cast<int> (preset.entries.size());
            for (int s = 0; s + n <= static_cast<int> (real.size()); ++s)
            {
                bool match = true;
                for (int k = 0; k < n; ++k)
                {
                    const auto& chord = real[static_cast<size_t> (s + k)].chord;
                    const auto& entry = preset.entries[static_cast<size_t> (k)];
                    if (presetRootOffset (chord, key) != entry.degreeSemitone
                        || ! qualityMatchesPreset (chord.quality, entry.quality))
                    {
                        match = false;
                        break;
                    }
                }

                if (match)
                {
                    patterns.push_back ({
                        preset.name,
                        real[static_cast<size_t> (s)].flatIndex,
                        real[static_cast<size_t> (s + n - 1)].flatIndex
                    });
                }
            }
        }
    }

    std::sort (patterns.begin(), patterns.end(),
               [] (const DetectedPattern& a, const DetectedPattern& b)
               {
                   if (a.startIndex != b.startIndex)
                       return a.startIndex < b.startIndex;
                   if (a.endIndex != b.endIndex)
                       return a.endIndex < b.endIndex;
                   return a.name < b.name;
               });

    return patterns;
}

//==============================================================================
std::vector<SubstitutionCandidate> HarmonyAnalyzer::substitutions (const Chord& chord,
                                                                   const KeyContext& key)
{
    std::vector<SubstitutionCandidate> out;
    const int offset = rootOffset (chord, key);

    // 1. Diatonic substitutes (Dairi)
    if (key.isMajor)
    {
        static const int tonicGroup[]       = { 0, 4, 9 };
        static const int subdominantGroup[] = { 5, 2 };
        static const int dominantGroup[]    = { 7, 11 };

        const int* group = nullptr;
        int groupSize = 0;

        auto inGroup = [&] (const int* g, int n) -> bool
        {
            for (int i = 0; i < n; ++i)
                if (g[i] == offset)
                    return true;
            return false;
        };

        if (inGroup (tonicGroup, 3))
        {
            group = tonicGroup;
            groupSize = 3;
        }
        else if (inGroup (subdominantGroup, 2))
        {
            group = subdominantGroup;
            groupSize = 2;
        }
        else if (inGroup (dominantGroup, 2))
        {
            group = dominantGroup;
            groupSize = 2;
        }

        if (group != nullptr)
        {
            for (int i = 0; i < groupSize; ++i)
            {
                const int deg = group[i];
                if (deg == offset)
                    continue;

                Chord cand;
                cand.rootPitchClass = normalizePc (key.tonicPitchClass + deg);
                cand.quality = diatonicSeventhQualityMajor (deg);
                cand.bassPitchClass = -1;

                const juce::String label = "Dairi (" + ChordModel::degreeName (cand, key) + ")";
                appendCandidate (out, chord, cand, label);
            }
        }
    }
    else
    {
        static const int tonicGroup[]       = { 0, 3 };
        static const int subdominantGroup[] = { 5, 8 };
        static const int dominantGroup[]    = { 7, 10 };

        const int* group = nullptr;
        int groupSize = 0;

        auto inGroup = [&] (const int* g, int n) -> bool
        {
            for (int i = 0; i < n; ++i)
                if (g[i] == offset)
                    return true;
            return false;
        };

        if (inGroup (tonicGroup, 2))
        {
            group = tonicGroup;
            groupSize = 2;
        }
        else if (inGroup (subdominantGroup, 2))
        {
            group = subdominantGroup;
            groupSize = 2;
        }
        else if (inGroup (dominantGroup, 2))
        {
            group = dominantGroup;
            groupSize = 2;
        }

        if (group != nullptr)
        {
            for (int i = 0; i < groupSize; ++i)
            {
                const int deg = group[i];
                if (deg == offset)
                    continue;

                Chord cand;
                cand.rootPitchClass = normalizePc (key.tonicPitchClass + deg);
                cand.quality = diatonicSeventhQualityMinor (deg);
                cand.bassPitchClass = -1;

                const juce::String label = "Dairi (" + ChordModel::degreeName (cand, key) + ")";
                appendCandidate (out, chord, cand, label);
            }
        }
    }

    // 2. Tritone sub (Ura)
    if (isDominantQuality (chord.quality))
    {
        Chord cand;
        cand.rootPitchClass = normalizePc (chord.rootPitchClass + 6);
        cand.quality = ChordQuality::Dominant7;
        cand.bassPitchClass = -1;

        const juce::String label = "Ura (" + ChordModel::degreeName (cand, key) + ")";
        appendCandidate (out, chord, cand, label);
    }

    // 3. Dominantize (Minor7 -> Dominant7 same root)
    if (chord.quality == ChordQuality::Minor7)
    {
        Chord cand;
        cand.rootPitchClass = chord.rootPitchClass;
        cand.quality = ChordQuality::Dominant7;
        cand.bassPitchClass = -1;

        const juce::String label = "Dominantize (" + ChordModel::degreeName (cand, key) + ")";
        appendCandidate (out, chord, cand, label);
    }

    return out;
}

} // namespace reharm
