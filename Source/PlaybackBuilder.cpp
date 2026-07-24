#include "PlaybackBuilder.h"
#include <algorithm>
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

std::vector<int> PlaybackBuilder::arpeggioBaseNotes (std::vector<int> notes, int rootPitchClass)
{
    const int k = static_cast<int> (notes.size());

    if (k <= 1 || k >= 4)
        return notes;

    if (k == 2)
    {
        // DYAD: {n0, n1, n0+12, n1+12}
        std::vector<int> result;
        result.reserve (4);
        result.push_back (notes[0]);
        result.push_back (notes[1]);
        result.push_back (notes[0] + 12);
        result.push_back (notes[1] + 12);
        return result;
    }

    // k == 3: TRIAD -> [root, 5th, root+12, 3rd+12]
    // Identify root note
    int rootNote = -1;
    for (int n : notes)
    {
        if ((n % 12) == rootPitchClass)
        {
            rootNote = n;
            break;
        }
    }
    if (rootNote < 0)
        rootNote = notes[0];

    // Find remaining two notes and identify 3rd vs 5th by interval from root
    std::vector<int> remaining;
    for (int n : notes)
    {
        if (n != rootNote)
            remaining.push_back (n);
    }

    // Smaller interval from root = 3rd, larger = 5th
    // (minor 3rd=3, major 3rd=4 are always < perfect 5th=7)
    int thirdNote = -1, fifthNote = -1;
    if (remaining.size() == 2)
    {
        // Normalise to 0..11 so a note below the root (e.g. an on-chord bass
        // ahead of the root) still yields a correct interval for 3rd/5th sorting.
        const int interval0 = ((remaining[0] - rootNote) % 12 + 12) % 12;
        const int interval1 = ((remaining[1] - rootNote) % 12 + 12) % 12;
        if (interval0 < interval1)
        {
            thirdNote = remaining[0];
            fifthNote = remaining[1];
        }
        else
        {
            thirdNote = remaining[1];
            fifthNote = remaining[0];
        }
    }

    if (thirdNote < 0 || fifthNote < 0)
        return notes; // fallback if we couldn't identify roles

    std::vector<int> result;
    result.reserve (4);
    result.push_back (rootNote);      // b0 = root
    result.push_back (fifthNote);     // b1 = 5th
    result.push_back (rootNote + 12); // b2 = root-octave
    result.push_back (thirdNote + 12);// b3 = 3rd-octave
    return result;
}

namespace
{
/** Builds the one-cycle pattern sequence for the given arpeggio pattern.
    expandedFromSmall: true when original chord had 2 or 3 notes (DYAD/TRIAD).
    When true, UpDown uses the new 8-note period pattern.
    When false (4+ note chords), uses the legacy period-6 pattern. */
std::vector<int> buildPatternSequence (const std::vector<int>& base,
                                        ArpPattern pattern,
                                        int rootPitchClass,
                                        bool expandedFromSmall)
{
    std::vector<int> seq;
    const int k = (int) base.size();
    if (k == 0)
        return seq;

    switch (pattern)
    {
        case ArpPattern::Up:
            seq = base;
            break;

        case ArpPattern::Down:
            seq.assign (base.rbegin(), base.rend());
            break;

        case ArpPattern::UpDown:
            if (expandedFromSmall && k == 4)
            {
                // New 8-note period: b0 b1 b2 b3 top b3 b2 b1.
                // The ascending run is b0 b1 b2 b3 top, so top must sit ABOVE
                // b3 (the highest base note): it is the root one octave above
                // b2. Since b2 == rootNote + 12, top == rootNote + 24.
                int rootNote = -1;
                for (int n : base)
                {
                    if ((n % 12) == rootPitchClass)
                    {
                        rootNote = n;
                        break;
                    }
                }
                if (rootNote < 0)
                    rootNote = base[0];

                const int top = rootNote + 24;
                seq.reserve (8);
                seq.push_back (base[0]);
                seq.push_back (base[1]);
                seq.push_back (base[2]);
                seq.push_back (base[3]);
                seq.push_back (top);
                seq.push_back (base[3]);
                seq.push_back (base[2]);
                seq.push_back (base[1]);
            }
            else
            {
                // Legacy pattern: n0..nk-1 then nk-2..n1
                seq = base;
                for (int i = k - 2; i >= 1; --i)
                    seq.push_back (base[(size_t) i]);
            }
            break;

        case ArpPattern::Off:
        default:
            seq = base;
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

        const int origSize = static_cast<int> (chordNotes.size());
        const bool expandedFromSmall = (origSize <= 3);

        // Build arpeggio base notes (expands 2/3-note chords to 4-note base).
        chordNotes = arpeggioBaseNotes (chordNotes, fc.chord->rootPitchClass);

        const auto seq = buildPatternSequence (chordNotes, settings.arpPattern,
                                                fc.chord->rootPitchClass,
                                                expandedFromSmall);
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
