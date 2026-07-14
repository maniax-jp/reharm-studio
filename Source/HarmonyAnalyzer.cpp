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

bool matchesSecondaryDominant (const Chord& c,
                               const std::optional<Chord>& next,
                               const KeyContext& key)
{
    if (! isDominantQuality (c.quality) || ! next.has_value())
        return false;

    // Case A: resolve down a perfect fifth (not to tonic).
    if (normalizePc (c.rootPitchClass + 5) == normalizePc (next->rootPitchClass)
        && rootOffset (*next, key) != 0)
        return true;

    // Case B: III7 -> IV (deceptive resolution).
    if (rootOffset (c, key) == 4 && rootOffset (*next, key) == 5)
        return true;

    // Case C: VI7 -> IV (deceptive resolution).
    if (rootOffset (c, key) == 9 && rootOffset (*next, key) == 5)
        return true;

    return false;
}

bool matchesTritoneSubstitution (const Chord& c,
                                 const std::optional<Chord>& next,
                                 const KeyContext& key)
{
    if (! isDominantQuality (c.quality))
        return false;

    if (rootOffset (c, key) == 1)
        return true;

    // Half-step descent to next root.
    if (next.has_value()
        && normalizePc (c.rootPitchClass + 11) == normalizePc (next->rootPitchClass))
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

    // 1. Diatonic
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

    // 3. Double Dominant (major key II7)
    if (key.isMajor && isDominantQuality (c.quality) && offset == 2)
        return { false, NonDiatonicTechnique::DoubleDominant, "Dbl.Dom", {} };

    // 4-pre. Minor-key V7 from harmonic minor (before secondary dominant)
    if (! key.isMajor && offset == 7 && isDominantQuality (c.quality))
    {
        ChordAnalysis a;
        a.diatonic = false;
        a.technique = NonDiatonicTechnique::ModalInterchange;
        a.label = "V7";
        a.borrowedScale = ChordModel::pitchClassName (key.tonicPitchClass) + " Harmonic minor";
        return a;
    }

    // 4. Secondary Dominant
    if (matchesSecondaryDominant (c, next, key))
    {
        // Roman numeral is the target degree of V7/x (root + perfect fifth).
        const juce::String roman = ChordModel::degreeRoman (c.rootPitchClass + 5, key);
        return { false, NonDiatonicTechnique::SecondaryDominant,
                 "Sec.Dom (V7/" + roman + ")", {} };
    }

    // 5. Tritone Substitution
    if (matchesTritoneSubstitution (c, next, key))
        return { false, NonDiatonicTechnique::TritoneSubstitution, "Tritone Sub", {} };

    // 6. Passing Diminished
    if (isDiminishedQuality (c.quality))
    {
        const bool hasPrev = prev.has_value();
        const bool hasNext = next.has_value();
        const bool matchPrev = hasPrev
            && normalizePc (c.rootPitchClass) == normalizePc (prev->rootPitchClass + 1);
        const bool matchNext = hasNext
            && normalizePc (c.rootPitchClass) == normalizePc (next->rootPitchClass + 11);

        bool pass = false;
        if (hasPrev && hasNext)
            pass = matchPrev && matchNext;
        else if (hasPrev)
            pass = matchPrev;
        else if (hasNext)
            pass = matchNext;

        if (pass)
            return { false, NonDiatonicTechnique::PassingDiminished, "Pass.Dim", {} };
    }

    // 7. Related II minor (look-ahead: next is Sec.Dom or Tritone Sub)
    if (isRelatedTwoQuality (c.quality) && next.has_value()
        && normalizePc (c.rootPitchClass + 5) == normalizePc (next->rootPitchClass))
    {
        // Classify next with full priority (without depending on this chord's result).
        const ChordAnalysis nextAnalysis = classifyReal (real, realIndex + 1, key);
        if (nextAnalysis.technique == NonDiatonicTechnique::SecondaryDominant
            || nextAnalysis.technique == NonDiatonicTechnique::TritoneSubstitution)
        {
            return { false, NonDiatonicTechnique::RelatedTwoMinor, "Rel.IIm", {} };
        }
    }

    // 8. Subdominant Minor (major key only)
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

        if (! isDominantQuality (a.quality))
            continue;

        if (isDominantQuality (b.quality))
            continue;

        const int step = normalizePc (b.rootPitchClass - a.rootPitchClass);
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
