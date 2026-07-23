#include "PlaybackBuilder.h"
#include <cmath>

namespace reharm
{

double PlaybackBuilder::beatsPerStep (ArpRate r)
{
    switch (r)
    {
        case ArpRate::Quarter:   return 1.0;
        case ArpRate::Eighth:    return 0.5;
        case ArpRate::Sixteenth: return 0.25;
    }
    return 0.5;
}

namespace
{
/** Builds the one-cycle pattern sequence (ascending source notes) for the
    given arpeggio pattern. Up: n0..nk-1. Down: nk-1..n0. UpDown: n0..nk-1
    then nk-2..n1 (k==1 -> just n0; k==2 -> n0,n1). */
std::vector<int> buildPatternSequence (const std::vector<int>& notes, ArpPattern pattern)
{
    std::vector<int> seq;
    const int k = (int) notes.size();
    if (k == 0)
        return seq;

    switch (pattern)
    {
        case ArpPattern::Up:
            seq = notes;
            break;

        case ArpPattern::Down:
            seq.assign (notes.rbegin(), notes.rend());
            break;

        case ArpPattern::UpDown:
        {
            seq = notes; // n0 .. nk-1
            for (int i = k - 2; i >= 1; --i)
                seq.push_back (notes[(size_t) i]);
            break;
        }

        case ArpPattern::Off:
        default:
            seq = notes;
            break;
    }

    return seq;
}
} // namespace

std::shared_ptr<ChordData> PlaybackBuilder::build (const ProgressionModel& model,
                                                    const PlaybackSettings& settings)
{
    auto flat = model.flatten();

    // Trim trailing rests so playback loops right after the last populated slot.
    // Interior rests (followed by a chord later) are preserved.
    while (! flat.empty() && ! flat.back().chord.has_value())
        flat.pop_back();

    std::vector<std::vector<int>> notes;
    std::vector<double> beats;
    std::vector<int> displayIndices;

    notes.reserve (flat.size());
    beats.reserve (flat.size());
    displayIndices.reserve (flat.size());

    for (int i = 0; i < (int) flat.size(); ++i)
    {
        const auto& fc = flat[(size_t) i];

        if (! fc.chord.has_value())
        {
            // Rest: single engine slot, never subdivided.
            notes.emplace_back();
            beats.push_back (fc.beats);
            displayIndices.push_back (i);
            continue;
        }

        auto chordNotes = Voicing::midiNotes (*fc.chord, settings.voicing);

        if (chordNotes.empty())
        {
            // Should not happen in practice, but treat like a rest.
            notes.emplace_back();
            beats.push_back (fc.beats);
            displayIndices.push_back (i);
            continue;
        }

        if (settings.arpPattern == ArpPattern::Off)
        {
            notes.push_back (chordNotes);
            beats.push_back (fc.beats);
            displayIndices.push_back (i);
            continue;
        }

        const auto seq = buildPatternSequence (chordNotes, settings.arpPattern);
        const int seqLen = (int) seq.size();
        if (seqLen == 0)
        {
            notes.emplace_back();
            beats.push_back (fc.beats);
            displayIndices.push_back (i);
            continue;
        }

        const double step = beatsPerStep (settings.arpRate);
        const int count = (int) std::round (fc.beats / step);

        for (int j = 0; j < count; ++j)
        {
            notes.push_back ({ seq[(size_t) (j % seqLen)] });
            beats.push_back (step);
            displayIndices.push_back (i);
        }
    }

    auto data = std::make_shared<ChordData> (std::move (notes), std::move (beats));
    data->displayIndices = std::move (displayIndices);
    return data;
}

} // namespace reharm
