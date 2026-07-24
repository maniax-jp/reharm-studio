#pragma once

#include "PlaybackSettings.h"
#include "ProgressionModel.h"
#include "AudioEngine.h"
#include <memory>

namespace reharm
{

/** Builds AudioEngine-ready ChordData from a ProgressionModel.
    (Moved from the former MainComponent::pushChordDataToEngine logic, plus
    arpeggio expansion: an arpeggio subdivides one chord slot into several
    single-note engine slots at the configured rate.) */
class PlaybackBuilder
{
public:
    /** Beats per arpeggio step for the given rate: 1.0 / 0.5 / 0.25. */
    static double beatsPerStep (ArpRate r);

    /** OPEN-voicing arpeggio base. Re-stacks small chords by position (not by
        harmonic role), so the run leads with the outer tones before the
        octave-up pair. notes must be ascending (a Close voicing).
        2 notes (DYAD):  {n0, n1, n0+12, n1+12}.
        3 notes (TRIAD): {n0, n2, n0+12, n1+12}.
        4+ or 1-or-fewer notes: returned unchanged.
        rootPitchClass is unused for 3-note chords - the ordering is purely
        positional, which keeps sus2/sus4/dim/b5 chords working the same way. */
    static std::vector<int> arpeggioBaseNotes (std::vector<int> notes, int rootPitchClass);

    /** CLOSE-voicing arpeggio base (the pre-v19.2 behaviour). Extends the note
        list to at least 4 notes by octave-shifting, keeping ascending order:
        2 notes -> {n0, n1, n0+12, n1+12}; 3 notes -> add the root one octave up;
        4+ or 1-or-fewer notes returned unchanged. notes must be ascending. */
    static std::vector<int> ensureMinArpeggioNotes (std::vector<int> notes, int rootPitchClass);

    /** flatten -> trim trailing rests -> (expand arpeggio) -> ChordData.
        displayIndices[engineSlot] = UI flat slot index is always populated
        (identity even when Off; trimmed trailing rests are not included). */
    static std::shared_ptr<ChordData> build (const ProgressionModel& model,
                                             const PlaybackSettings& settings);
};

} // namespace reharm
