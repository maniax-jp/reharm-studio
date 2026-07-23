#include "ChordModel.h"

#include <algorithm>
#include <array>
#include <set>

namespace reharm
{

namespace
{
int normalizePitchClass (int pc) noexcept
{
    return ((pc % 12) + 12) % 12;
}

int bassMidiNote (int rootMidi, int rootPc, int bassPc) noexcept
{
    int bassMidi = rootMidi - 12 + normalizePitchClass (bassPc - rootPc);
    if (bassMidi >= rootMidi)
        bassMidi -= 12;
    return bassMidi;
}

struct ToneRoles
{
    int third;   // -1 if none
    int fifth;   // -1 if none
    int seventh; // -1 if none
};

const ToneRoles& rolesFor (ChordQuality q) noexcept
{
    static const std::array<ToneRoles, static_cast<size_t> (ChordQuality::NumQualities)> table = {{
        /* Power5          */ { -1, 7, -1 },
        /* Major           */ {  4, 7, -1 },
        /* Minor           */ {  3, 7, -1 },
        /* Sus2            */ { -1, 7, -1 },
        /* Sus4            */ { -1, 7, -1 },
        /* Augmented       */ {  4, 8, -1 },
        /* Diminished      */ {  3, 6, -1 },
        /* Sixth           */ {  4, 7, -1 },
        /* Minor6          */ {  3, 7, -1 },
        /* SixthSus4       */ { -1, 7, -1 },
        /* Dominant7       */ {  4, 7, 10 },
        /* Major7          */ {  4, 7, 11 },
        /* Minor7          */ {  3, 7, 10 },
        /* MinorMajor7     */ {  3, 7, 11 },
        /* Dominant7Sus2   */ { -1, 7, 10 },
        /* Dominant7Sus4   */ { -1, 7, 10 },
        /* Major7Sus2      */ { -1, 7, 11 },
        /* Major7Sus4      */ { -1, 7, 11 },
        /* Minor7Flat5     */ {  3, 6, 10 },
        /* Diminished7     */ {  3, 6,  9 },
        /* Augmented7      */ {  4, 8, 10 },
        /* AugmentedMajor7 */ {  4, 8, 11 },
        /* Blackadder      */ { -1, 6, 10 },
    }};

    const auto index = static_cast<size_t> (q);
    if (index >= table.size())
        return table[0];

    return table[index];
}

constexpr int kAddIntervals[7] = { 13, 14, 15, 17, 18, 20, 21 };
constexpr const char* kAddNames[7] = { "b9", "9", "#9", "11", "#11", "b13", "13" };

void removeOneInterval (std::vector<int>& ivals, int value)
{
    if (value < 0)
        return;

    auto it = std::find (ivals.begin(), ivals.end(), value);
    if (it != ivals.end())
        ivals.erase (it);
}

/** Intervals after omit and fifth alteration, without added tensions. */
std::vector<int> baseEffectiveIntervals (const Chord& c)
{
    std::vector<int> result = ChordModel::intervals (c.quality);
    const auto& roles = rolesFor (c.quality);

    if ((c.omitMask & Omit3) != 0)
        removeOneInterval (result, roles.third);

    if ((c.omitMask & Omit5) != 0)
        removeOneInterval (result, roles.fifth);

    if ((c.omitMask & Omit7) != 0)
        removeOneInterval (result, roles.seventh);

    if (roles.fifth == 7 && c.fifthAlt != FifthAlt::None)
    {
        auto it = std::find (result.begin(), result.end(), 7);
        if (it != result.end())
            *it = (c.fifthAlt == FifthAlt::Flat) ? 6 : 8;
    }

    return result;
}

bool pitchClassCollides (const std::vector<int>& baseIvals, int tensionSemi)
{
    const int pc = normalizePitchClass (tensionSemi);
    for (int iv : baseIvals)
        if (normalizePitchClass (iv) == pc)
            return true;
    return false;
}

bool hasAddBit (int mask, int bitIndex) noexcept
{
    return (mask & (1 << bitIndex)) != 0;
}

/** True if add bit index can be selected given current mask and base intervals. */
bool isAddAvailable (int bitIndex, int addMask, const std::vector<int>& baseIvals) noexcept
{
    if (hasAddBit (addMask, bitIndex))
        return true;

    // Exclusive pairs: 9 vs b9, 9 vs #9; 11 vs #11; 13 vs b13.
    // b9 and #9 may coexist.
    if (bitIndex == 1) // 9
    {
        if (hasAddBit (addMask, 0) || hasAddBit (addMask, 2))
            return false;
    }
    else if (bitIndex == 0 || bitIndex == 2) // b9 or #9
    {
        if (hasAddBit (addMask, 1))
            return false;
    }
    else if (bitIndex == 3) // 11
    {
        if (hasAddBit (addMask, 4))
            return false;
    }
    else if (bitIndex == 4) // #11
    {
        if (hasAddBit (addMask, 3))
            return false;
    }
    else if (bitIndex == 5) // b13
    {
        if (hasAddBit (addMask, 6))
            return false;
    }
    else if (bitIndex == 6) // 13
    {
        if (hasAddBit (addMask, 5))
            return false;
    }

    if (pitchClassCollides (baseIvals, kAddIntervals[bitIndex]))
        return false;

    return true;
}

/** Common-name suffix when addMask is an exact match; empty if none. */
juce::String commonNameSuffix (ChordQuality q, int addMask)
{
    if (addMask == AddNine)
    {
        switch (q)
        {
            case ChordQuality::Major:           return "add9";
            case ChordQuality::Minor:           return "madd9";
            case ChordQuality::Dominant7:       return "9";
            case ChordQuality::Major7:          return "M9";
            case ChordQuality::Minor7:          return "m9";
            case ChordQuality::MinorMajor7:     return "mM9";
            case ChordQuality::Dominant7Sus4:   return "9sus4";
            default: break;
        }
    }
    else if (addMask == (AddNine | AddEleven))
    {
        switch (q)
        {
            case ChordQuality::Dominant7: return "11";
            case ChordQuality::Major7:    return "M11";
            case ChordQuality::Minor7:    return "m11";
            default: break;
        }
    }
    else if (addMask == (AddNine | AddEleven | AddThirteen))
    {
        switch (q)
        {
            case ChordQuality::Dominant7: return "13";
            case ChordQuality::Major7:    return "M13";
            case ChordQuality::Minor7:    return "m13";
            default: break;
        }
    }

    return {};
}

bool isSixthFamily (ChordQuality q) noexcept
{
    return q == ChordQuality::Sixth
        || q == ChordQuality::Minor6
        || q == ChordQuality::SixthSus4;
}

/** Prefix before parentheses when folding 6 into option list. */
const char* sixthFamilyPrefix (ChordQuality q) noexcept
{
    switch (q)
    {
        case ChordQuality::Minor6:    return "m";
        case ChordQuality::SixthSus4: return "sus4";
        default:                      return "";
    }
}

juce::String optionSuffix (const Chord& c)
{
    const juce::String common = commonNameSuffix (c.quality, c.addMask);
    juce::String result;

    if (common.isNotEmpty())
    {
        result = common;

        if (c.fifthAlt == FifthAlt::Flat)
            result += "(b5)";
        else if (c.fifthAlt == FifthAlt::Sharp)
            result += "(#5)";
    }
    else if (isSixthFamily (c.quality) && c.addMask != 0)
    {
        // Fold the quality's "6" into the option list: C(6,9), Cm(6,9), Csus4(6,9).
        juce::StringArray parts;
        parts.add ("6");

        if (c.fifthAlt == FifthAlt::Flat)
            parts.add ("b5");
        else if (c.fifthAlt == FifthAlt::Sharp)
            parts.add ("#5");

        for (int i = 0; i < 7; ++i)
            if (hasAddBit (c.addMask, i))
                parts.add (kAddNames[i]);

        result = juce::String (sixthFamilyPrefix (c.quality))
                 + "(" + parts.joinIntoString (",") + ")";
    }
    else
    {
        result = ChordModel::qualitySuffix (c.quality);

        juce::StringArray parts;
        if (c.fifthAlt == FifthAlt::Flat)
            parts.add ("b5");
        else if (c.fifthAlt == FifthAlt::Sharp)
            parts.add ("#5");

        for (int i = 0; i < 7; ++i)
            if (hasAddBit (c.addMask, i))
                parts.add (kAddNames[i]);

        if (parts.size() > 0)
            result += "(" + parts.joinIntoString (",") + ")";
    }

    if (c.omitMask != 0)
    {
        juce::StringArray omitParts;
        if ((c.omitMask & Omit3) != 0)
            omitParts.add ("3");
        if ((c.omitMask & Omit5) != 0)
            omitParts.add ("5");
        if ((c.omitMask & Omit7) != 0)
            omitParts.add ("7");

        if (omitParts.size() > 0)
            result += "(omit" + omitParts.joinIntoString (",") + ")";
    }

    return result;
}
} // namespace

const std::vector<int>& ChordModel::intervals (ChordQuality q)
{
    static const std::array<std::vector<int>, static_cast<size_t> (ChordQuality::NumQualities)> table = {{
        /* Power5          */ { 0, 7 },
        /* Major           */ { 0, 4, 7 },
        /* Minor           */ { 0, 3, 7 },
        /* Sus2            */ { 0, 2, 7 },
        /* Sus4            */ { 0, 5, 7 },
        /* Augmented       */ { 0, 4, 8 },
        /* Diminished      */ { 0, 3, 6 },
        /* Sixth           */ { 0, 4, 7, 9 },
        /* Minor6          */ { 0, 3, 7, 9 },
        /* SixthSus4       */ { 0, 5, 7, 9 },
        /* Dominant7       */ { 0, 4, 7, 10 },
        /* Major7          */ { 0, 4, 7, 11 },
        /* Minor7          */ { 0, 3, 7, 10 },
        /* MinorMajor7     */ { 0, 3, 7, 11 },
        /* Dominant7Sus2   */ { 0, 2, 7, 10 },
        /* Dominant7Sus4   */ { 0, 5, 7, 10 },
        /* Major7Sus2      */ { 0, 2, 7, 11 },
        /* Major7Sus4      */ { 0, 5, 7, 11 },
        /* Minor7Flat5     */ { 0, 3, 6, 10 },
        /* Diminished7     */ { 0, 3, 6, 9 },
        /* Augmented7      */ { 0, 4, 8, 10 },
        /* AugmentedMajor7 */ { 0, 4, 8, 11 },
        /* Blackadder      */ { 0, 2, 6, 10 },
    }};

    const auto index = static_cast<size_t> (q);
    if (index >= table.size())
        return table[0];

    return table[index];
}

std::vector<int> ChordModel::effectiveIntervals (const Chord& c)
{
    auto result = baseEffectiveIntervals (c);

    for (int i = 0; i < 7; ++i)
        if (hasAddBit (c.addMask, i))
            result.push_back (kAddIntervals[i]);

    std::sort (result.begin(), result.end());
    return result;
}

ChordModel::OptionAvailability ChordModel::optionAvailability (const Chord& c)
{
    OptionAvailability a;
    const auto& roles = rolesFor (c.quality);
    const auto baseIvals = baseEffectiveIntervals (c);

    // omit3
    if ((c.omitMask & Omit3) != 0 || roles.third != -1)
        a.omit3 = true;

    // omit5: needs fifth role, not Power5, and no b5/#5 (unless already selected)
    if ((c.omitMask & Omit5) != 0)
        a.omit5 = true;
    else if (roles.fifth != -1
             && c.quality != ChordQuality::Power5
             && c.fifthAlt == FifthAlt::None)
        a.omit5 = true;

    // omit7
    if ((c.omitMask & Omit7) != 0 || roles.seventh != -1)
        a.omit7 = true;

    // flat5 / sharp5: perfect fifth only, not when omit5; exclusive with each other
    const bool canAlterFifth = (roles.fifth == 7) && ((c.omitMask & Omit5) == 0);

    if (c.fifthAlt == FifthAlt::Flat)
        a.flat5 = true;
    else if (canAlterFifth && c.fifthAlt != FifthAlt::Sharp)
        a.flat5 = true;

    if (c.fifthAlt == FifthAlt::Sharp)
        a.sharp5 = true;
    else if (canAlterFifth && c.fifthAlt != FifthAlt::Flat)
        a.sharp5 = true;

    for (int i = 0; i < 7; ++i)
        a.add[i] = isAddAvailable (i, c.addMask, baseIvals);

    return a;
}

void ChordModel::sanitizeOptions (Chord& c)
{
    const auto& roles = rolesFor (c.quality);

    // (1) Drop omit bits with no corresponding role.
    if (roles.third == -1)
        c.omitMask &= ~Omit3;
    if (roles.fifth == -1 || c.quality == ChordQuality::Power5)
        c.omitMask &= ~Omit5;
    if (roles.seventh == -1)
        c.omitMask &= ~Omit7;

    // (2) fifthAlt only valid when fifth is perfect 7.
    if (roles.fifth != 7)
        c.fifthAlt = FifthAlt::None;

    // Prefer fifthAlt over omit5 when both would apply.
    if (c.fifthAlt != FifthAlt::None && (c.omitMask & Omit5) != 0)
        c.omitMask &= ~Omit5;

    // (3) Mutual exclusion among adds, then pitch-class collisions.
    if ((c.addMask & AddNine) != 0)
        c.addMask &= ~(AddFlat9 | AddSharp9);
    if ((c.addMask & AddEleven) != 0)
        c.addMask &= ~AddSharp11;
    if ((c.addMask & AddThirteen) != 0)
        c.addMask &= ~AddFlat13;

    const auto baseIvals = baseEffectiveIntervals (c);
    for (int i = 0; i < 7; ++i)
    {
        if (hasAddBit (c.addMask, i) && pitchClassCollides (baseIvals, kAddIntervals[i]))
            c.addMask &= ~(1 << i);
    }
}

const std::vector<ChordModel::QualityGroup>& ChordModel::qualityGroups()
{
    static const std::vector<QualityGroup> groups = {
        { "DYAD",  { ChordQuality::Power5 } },
        { "TRIAD", { ChordQuality::Major, ChordQuality::Minor, ChordQuality::Sus2, ChordQuality::Sus4,
                     ChordQuality::Augmented, ChordQuality::Diminished } },
        { "6TH",   { ChordQuality::Sixth, ChordQuality::Minor6, ChordQuality::SixthSus4 } },
        { "7TH",   { ChordQuality::Dominant7, ChordQuality::Major7, ChordQuality::Minor7, ChordQuality::MinorMajor7,
                     ChordQuality::Dominant7Sus2, ChordQuality::Dominant7Sus4, ChordQuality::Major7Sus2, ChordQuality::Major7Sus4,
                     ChordQuality::Minor7Flat5, ChordQuality::Diminished7, ChordQuality::Augmented7,
                     ChordQuality::AugmentedMajor7, ChordQuality::Blackadder } },
    };
    return groups;
}

juce::String ChordModel::qualitySuffix (ChordQuality q)
{
    switch (q)
    {
        case ChordQuality::Power5:          return "5";
        case ChordQuality::Major:           return {};
        case ChordQuality::Minor:           return "m";
        case ChordQuality::Sus2:            return "sus2";
        case ChordQuality::Sus4:            return "sus4";
        case ChordQuality::Augmented:       return "aug";
        case ChordQuality::Diminished:      return "dim";
        case ChordQuality::Sixth:           return "6";
        case ChordQuality::Minor6:          return "m6";
        case ChordQuality::SixthSus4:       return "6sus4";
        case ChordQuality::Dominant7:       return "7";
        case ChordQuality::Major7:          return "M7";
        case ChordQuality::Minor7:          return "m7";
        case ChordQuality::MinorMajor7:     return "mM7";
        case ChordQuality::Dominant7Sus2:   return "7sus2";
        case ChordQuality::Dominant7Sus4:   return "7sus4";
        case ChordQuality::Major7Sus2:      return "M7sus2";
        case ChordQuality::Major7Sus4:      return "M7sus4";
        case ChordQuality::Minor7Flat5:     return "m7b5";
        case ChordQuality::Diminished7:     return "dim7";
        case ChordQuality::Augmented7:      return "aug7";
        case ChordQuality::AugmentedMajor7: return "augM7";
        case ChordQuality::Blackadder:      return "blk";
        case ChordQuality::NumQualities:    break;
    }

    return {};
}

juce::String ChordModel::qualityDisplayName (ChordQuality q)
{
    if (q == ChordQuality::Major)
        return "maj";

    return qualitySuffix (q);
}

juce::String ChordModel::qualityId (ChordQuality q)
{
    if (q == ChordQuality::Major)
        return "maj";

    return qualitySuffix (q);
}

std::optional<ChordQuality> ChordModel::qualityFromId (const juce::String& id)
{
    for (int i = 0; i < static_cast<int> (ChordQuality::NumQualities); ++i)
    {
        const auto q = static_cast<ChordQuality> (i);
        if (qualityId (q) == id)
            return q;
    }

    return std::nullopt;
}

juce::String ChordModel::pitchClassName (int pitchClass, bool preferFlat)
{
    static const char* const sharpNames[12] = {
        "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
    };
    static const char* const flatNames[12] = {
        "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"
    };

    const int pc = normalizePitchClass (pitchClass);
    return preferFlat ? flatNames[pc] : sharpNames[pc];
}

juce::String ChordModel::chordName (const Chord& c, bool preferFlat)
{
    juce::String name = pitchClassName (c.rootPitchClass, preferFlat)
                        + optionSuffix (c);

    if (c.hasBass())
        name += "/" + pitchClassName (c.bassPitchClass, preferFlat);

    return name;
}

juce::String ChordModel::degreeRoman (int pitchClass, const KeyContext& key)
{
    static const char* const romans[12] = {
        "I", "bII", "II", "bIII", "III", "IV",
        "bV", "V", "bVI", "VI", "bVII", "VII"
    };

    const int offset = normalizePitchClass (pitchClass - key.tonicPitchClass);
    return romans[offset];
}

juce::String ChordModel::degreeName (const Chord& c, const KeyContext& key)
{
    juce::String name = degreeRoman (c.rootPitchClass, key) + optionSuffix (c);

    if (c.hasBass())
        name += "/" + degreeRoman (c.bassPitchClass, key);

    return name;
}

std::vector<int> ChordModel::chordTonePitchClasses (const Chord& c)
{
    std::vector<int> result;
    std::set<int> seen;

    for (int interval : effectiveIntervals (c))
    {
        const int pc = normalizePitchClass (c.rootPitchClass + interval);
        if (seen.insert (pc).second)
            result.push_back (pc);
    }

    return result;
}

std::vector<int> ChordModel::scalePitchClasses (const KeyContext& key)
{
    static const int majorIntervals[]  = { 0, 2, 4, 5, 7, 9, 11 };
    static const int minorIntervals[]  = { 0, 2, 3, 5, 7, 8, 10 };

    const int* intervalsPtr = key.isMajor ? majorIntervals : minorIntervals;
    std::vector<int> result;
    result.reserve (7);

    for (int i = 0; i < 7; ++i)
        result.push_back (normalizePitchClass (key.tonicPitchClass + intervalsPtr[i]));

    return result;
}

bool ChordModel::isDiatonic (const Chord& c, const KeyContext& key)
{
    const auto scale = scalePitchClasses (key);
    const auto isInScale = [&scale] (int pc)
    {
        return std::find (scale.begin(), scale.end(), normalizePitchClass (pc)) != scale.end();
    };

    for (int pc : chordTonePitchClasses (c))
    {
        if (! isInScale (pc))
            return false;
    }

    if (c.hasBass() && ! isInScale (c.bassPitchClass))
        return false;

    return true;
}

std::vector<int> Voicing::midiNotes (const Chord& c, Style style, int octaveBase)
{
    const int rootPc = normalizePitchClass (c.rootPitchClass);
    const int rootMidi = octaveBase + rootPc;

    std::vector<int> notes;
    for (int interval : ChordModel::effectiveIntervals (c))
        notes.push_back (rootMidi + interval);

    if (style == Style::Close)
    {
        if (c.hasBass())
        {
            const int bass = bassMidiNote (rootMidi, rootPc, c.bassPitchClass);
            notes.insert (notes.begin(), bass);
        }

        // Close voicing is already ascending (intervals are sorted).
        return notes;
    }

    // Open: drop-2 of the close voicing, then add bass, then sort.
    if (! notes.empty())
    {
        if (notes.size() <= 3)
        {
            if (notes.size() >= 2)
                notes[1] -= 12;
        }
        else
        {
            // Second-highest note is at index size-2 when sorted ascending.
            notes[notes.size() - 2] -= 12;
        }
    }

    const int bass = c.hasBass()
                         ? bassMidiNote (rootMidi, rootPc, c.bassPitchClass)
                         : rootMidi - 12;
    notes.push_back (bass);

    std::sort (notes.begin(), notes.end());
    return notes;
}

} // namespace reharm
