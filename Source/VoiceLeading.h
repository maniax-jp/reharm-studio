#pragma once

#include "ChordModel.h"
#include <optional>
#include <vector>

namespace reharm
{

/** Chooses inversions / octave placements so that consecutive chords share tones
    and move as little as possible. Stateful: one instance per progression pass. */
class VoiceLeader
{
public:
    struct Options
    {
        int anchorLowest = 60;               // C4; same as Voicing's default octaveBase
        int lowestMin = 52;                  // E3
        int lowestMax = 67;                   // G4
        int octaveRadius = 2;
        int movementWeight = 2;
        int centerWeight = 1;
        int bottomClusterPenalty = 8;         // bottom pair closer than 3 semitones
        int semitonePenalty = 8;              // any adjacent pair exactly 1 semitone apart
    };

    // Two overloads rather than `Options o = {}`: a defaulted argument here would
    // need Options' default member initialisers while VoiceLeader is still an
    // incomplete type, which Clang rejects.
    VoiceLeader();
    explicit VoiceLeader (Options o);

    /** Sounding notes (ascending) for c, connected to the previously returned chord.
        The first call (and the first after reset()) returns
        Voicing::midiNotes (c, Voicing::Style::Close) verbatim.
        An explicit bass (on-chord) is always the lowest note. Updates internal state. */
    std::vector<int> next (const Chord& c);

    void reset() noexcept;
    bool hasPrevious() const noexcept;

    // --- pure helpers, exposed for unit tests ---

    /** True when the chord's own basic close voicing already has a semitone between
        adjacent tones -- e.g. Csus4(b5) = {0,5,6} or C6(#5) = {0,4,8,9}. For those
        chords the clash is part of the chord itself, so the semitone penalty is
        waived rather than spreading the voicing apart to escape it. */
    static bool hasInherentSemitone (const Chord& c);

    /** Ascending chord tones stacked from the root at octaveBase. No explicit bass. */
    static std::vector<int> closeStack (const Chord& c, int octaveBase = 60);

    /** All in-window candidate stacks, in canonical order (rotation asc, octave asc),
        de-duplicated. Never empty for a chord with at least one tone. */
    static std::vector<std::vector<int>> candidates (const Chord& c, const Options& o);

    /** Cost of moving from prevStack to candStack. Lower is better. */
    static int cost (const std::vector<int>& prevStack,
                     const std::vector<int>& candStack,
                     const Options& o);

private:
    Options opts;
    std::optional<std::vector<int>> prevStack;   // close stack, explicit bass excluded
};

} // namespace reharm
