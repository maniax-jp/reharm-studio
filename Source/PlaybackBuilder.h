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

    /** Extend arpeggio note sequences to at least 4 notes by octave-shifting.
        notes must be sorted ascending MIDI notes; rootPitchClass is 0..11. */
    static std::vector<int> ensureMinArpeggioNotes (std::vector<int> notes, int rootPitchClass);

    /** flatten -> trim trailing rests -> (expand arpeggio) -> ChordData.
        displayIndices[engineSlot] = UI flat slot index is always populated
        (identity even when Off; trimmed trailing rests are not included). */
    static std::shared_ptr<ChordData> build (const ProgressionModel& model,
                                             const PlaybackSettings& settings);
};

} // namespace reharm
