#pragma once

#include "ChordModel.h"

namespace reharm
{

/** Arpeggio pattern applied when building playback data from the progression. */
enum class ArpPattern { Off = 0, Up, Down, UpDown };

/** Arpeggio step rate, expressed as a fraction of a beat. */
enum class ArpRate { Quarter = 0, Eighth, Sixteenth };   // 1/4, 1/8, 1/16

/** Settings that control how ProgressionModel is turned into playable ChordData. */
struct PlaybackSettings
{
    Voicing::Style voicing = Voicing::Style::Close;
    ArpPattern arpPattern = ArpPattern::Off;
    ArpRate arpRate = ArpRate::Eighth;
};

} // namespace reharm
