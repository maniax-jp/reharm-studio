#pragma once

#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

namespace reharm
{

/** All chord qualities supported by Reharm Studio. */
enum class ChordQuality
{
    // dyad (2 notes)
    Power5 = 0,       // C5      {0,7}
    // triads (3 notes) + 6th chords
    Major,            // C       {0,4,7}
    Minor,            // Cm      {0,3,7}
    Sus2,             // Csus2   {0,2,7}
    Sus4,             // Csus4   {0,5,7}
    Augmented,        // Caug    {0,4,8}
    Diminished,       // Cdim    {0,3,6}
    Sixth,            // C6      {0,4,7,9}
    Minor6,           // Cm6     {0,3,7,9}
    SixthSus4,        // C6sus4  {0,5,7,9}
    // 7th chords (4 notes)
    Dominant7,        // C7      {0,4,7,10}
    Major7,           // CM7     {0,4,7,11}
    Minor7,           // Cm7     {0,3,7,10}
    MinorMajor7,      // CmM7    {0,3,7,11}
    Dominant7Sus2,    // C7sus2  {0,2,7,10}
    Dominant7Sus4,    // C7sus4  {0,5,7,10}
    Major7Sus2,       // CM7sus2 {0,2,7,11}
    Major7Sus4,       // CM7sus4 {0,5,7,11}
    Minor7Flat5,      // Cm7b5   {0,3,6,10}
    Diminished7,      // Cdim7   {0,3,6,9}
    Augmented7,       // Caug7   {0,4,8,10}
    AugmentedMajor7,  // CaugM7  {0,4,8,11}
    Blackadder,       // Cblk    {0,2,6,10}
    NumQualities
};

/** Alteration of the perfect fifth. */
enum class FifthAlt { None = 0, Flat, Sharp };

/** Bit flags for omitted chord tones. */
enum OmitFlags { Omit3 = 1 << 0, Omit5 = 1 << 1, Omit7 = 1 << 2 };

/** Bit flags for added tensions (index order matters: used by UI and naming). */
enum AddFlags
{
    AddFlat9  = 1 << 0,  // +13 semitones
    AddNine   = 1 << 1,  // +14
    AddSharp9 = 1 << 2,  // +15
    AddEleven = 1 << 3,  // +17
    AddSharp11= 1 << 4,  // +18
    AddFlat13 = 1 << 5,  // +20
    AddThirteen = 1 << 6 // +21
};

/** A chord: root pitch class, quality, and optional slash-chord bass. */
struct Chord
{
    int rootPitchClass = 0;                       // 0 = C ... 11 = B
    ChordQuality quality = ChordQuality::Major;
    int bassPitchClass = -1;                      // -1 = none; >= 0 makes an on-chord (e.g. C/E)
    int omitMask = 0;                             // OmitFlags
    FifthAlt fifthAlt = FifthAlt::None;
    int addMask = 0;                              // AddFlags

    bool hasBass() const noexcept { return bassPitchClass >= 0 && bassPitchClass != rootPitchClass; }

    bool hasOptions() const noexcept
    {
        return omitMask != 0 || addMask != 0 || fifthAlt != FifthAlt::None;
    }

    bool operator== (const Chord& other) const noexcept
    {
        return rootPitchClass == other.rootPitchClass
            && quality == other.quality
            && (hasBass() ? bassPitchClass : -1) == (other.hasBass() ? other.bassPitchClass : -1)
            && omitMask == other.omitMask
            && fifthAlt == other.fifthAlt
            && addMask == other.addMask;
    }
    bool operator!= (const Chord& other) const noexcept { return ! (*this == other); }
};

/** The key context used for degree names and analysis. */
struct KeyContext
{
    int tonicPitchClass = 0;   // 0 = C ... 11 = B
    bool isMajor = true;
};

/** Static chord-theory utilities: intervals, naming, degree names. */
class ChordModel
{
public:
    /** Semitone intervals from the root for a quality (root = 0 always included). */
    static const std::vector<int>& intervals (ChordQuality q);

    /** Intervals after applying omit / fifth alteration / added tensions. Ascending. */
    static std::vector<int> effectiveIntervals (const Chord& c);

    /** Which options can be toggled ON for this chord's quality and current selections. */
    struct OptionAvailability
    {
        bool omit3 = false, omit5 = false, omit7 = false;
        bool flat5 = false, sharp5 = false;
        bool add[7] = {};   // index = AddFlags bit index (0=b9 ... 6=13)
    };
    static OptionAvailability optionAvailability (const Chord& c);

    /** Drops selected options that are no longer valid (e.g. after a quality change). */
    static void sanitizeOptions (Chord& c);

    struct QualityGroup
    {
        const char* label;
        std::vector<ChordQuality> qualities;
    };
    /** Quality selector groups for the editor UI: dyad / triad / 6th / 7th. */
    static const std::vector<QualityGroup>& qualityGroups();

    /** Suffix appended to the root name, e.g. "m7", "M7sus4", "" for major. */
    static juce::String qualitySuffix (ChordQuality q);

    /** Human-readable name for UI quality selectors (same as suffix but "maj" for Major). */
    static juce::String qualityDisplayName (ChordQuality q);

    /** Stable string ID for the save format. Same as qualitySuffix but Major is "maj"
        (an empty suffix would be ambiguous as a JSON key). */
    static juce::String qualityId (ChordQuality q);

    /** Reverse lookup for qualityId. Returns nullopt when unrecognised. */
    static std::optional<ChordQuality> qualityFromId (const juce::String& id);

    /** Pitch-class name, e.g. 1 -> "C#" (or "Db" when preferFlat). */
    static juce::String pitchClassName (int pitchClass, bool preferFlat = false);

    /** Full chord name, e.g. "F#m7b5", "C/E". */
    static juce::String chordName (const Chord& c, bool preferFlat = false);

    /** Degree name relative to the key, e.g. "IVM7", "bVII7", on-chord: "I/III".
        Roman numeral is always uppercase; minor quality is expressed via suffix ("IIm7"). */
    static juce::String degreeName (const Chord& c, const KeyContext& key);

    /** Roman numeral (with leading b/#) for a pitch class relative to the tonic, e.g. "bVI". */
    static juce::String degreeRoman (int pitchClass, const KeyContext& key);

    /** Chord tones as pitch classes 0..11 (bass excluded). */
    static std::vector<int> chordTonePitchClasses (const Chord& c);

    /** True if every chord tone fits the key's diatonic scale
        (major scale, or natural minor for minor keys). */
    static bool isDiatonic (const Chord& c, const KeyContext& key);

    /** The 7 diatonic pitch classes of the key. */
    static std::vector<int> scalePitchClasses (const KeyContext& key);
};

/** Converts chords to concrete MIDI notes in close or open voicing. */
class Voicing
{
public:
    enum class Style { Close, Open };

    /** Returns MIDI notes (ascending) for the chord.
        Close: root position stacked from the root near octaveBase (default C4 = 60);
               explicit bass (on-chord) is added below the root.
        Open:  drop-2 of the close voicing (second-highest note dropped one octave),
               plus a bass note one octave below the root (or the explicit bass). */
    static std::vector<int> midiNotes (const Chord& c, Style style, int octaveBase = 60);
};

} // namespace reharm
