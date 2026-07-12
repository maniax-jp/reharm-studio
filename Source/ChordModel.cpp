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
} // namespace

const std::vector<int>& ChordModel::intervals (ChordQuality q)
{
    static const std::array<std::vector<int>, static_cast<size_t> (ChordQuality::NumQualities)> table = {{
        /* Major            */ { 0, 4, 7 },
        /* Minor            */ { 0, 3, 7 },
        /* Dominant7        */ { 0, 4, 7, 10 },
        /* Major7           */ { 0, 4, 7, 11 },
        /* Minor7           */ { 0, 3, 7, 10 },
        /* MinorMajor7      */ { 0, 3, 7, 11 },
        /* Add9             */ { 0, 4, 7, 14 },
        /* Sus2             */ { 0, 2, 7 },
        /* Sus4             */ { 0, 5, 7 },
        /* Augmented        */ { 0, 4, 8 },
        /* Diminished       */ { 0, 3, 6 },
        /* Dominant9        */ { 0, 4, 7, 10, 14 },
        /* Sixth            */ { 0, 4, 7, 9 },
        /* Add11            */ { 0, 4, 7, 17 },
        /* Add13            */ { 0, 4, 7, 21 },
        /* Major7Sus4       */ { 0, 5, 7, 11 },
        /* Dominant7Sus4    */ { 0, 5, 7, 10 },
        /* Minor7Flat5      */ { 0, 3, 6, 10 },
        /* Diminished7      */ { 0, 3, 6, 9 },
        /* Augmented7       */ { 0, 4, 8, 10 },
        /* AugmentedMajor7  */ { 0, 4, 8, 11 },
        /* Blackadder       */ { 0, 2, 6, 10 },
    }};

    const auto index = static_cast<size_t> (q);
    if (index >= table.size())
        return table[0];

    return table[index];
}

juce::String ChordModel::qualitySuffix (ChordQuality q)
{
    switch (q)
    {
        case ChordQuality::Major:           return {};
        case ChordQuality::Minor:           return "m";
        case ChordQuality::Dominant7:       return "7";
        case ChordQuality::Major7:          return "M7";
        case ChordQuality::Minor7:          return "m7";
        case ChordQuality::MinorMajor7:     return "mM7";
        case ChordQuality::Add9:            return "add9";
        case ChordQuality::Sus2:            return "sus2";
        case ChordQuality::Sus4:            return "sus4";
        case ChordQuality::Augmented:       return "aug";
        case ChordQuality::Diminished:      return "dim";
        case ChordQuality::Dominant9:       return "9";
        case ChordQuality::Sixth:           return "6";
        case ChordQuality::Add11:           return "add11";
        case ChordQuality::Add13:           return "add13";
        case ChordQuality::Major7Sus4:      return "M7sus4";
        case ChordQuality::Dominant7Sus4:   return "7sus4";
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
                        + qualitySuffix (c.quality);

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
    juce::String name = degreeRoman (c.rootPitchClass, key) + qualitySuffix (c.quality);

    if (c.hasBass())
        name += "/" + degreeRoman (c.bassPitchClass, key);

    return name;
}

std::vector<int> ChordModel::chordTonePitchClasses (const Chord& c)
{
    std::vector<int> result;
    std::set<int> seen;

    for (int interval : intervals (c.quality))
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
    for (int interval : ChordModel::intervals (c.quality))
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
