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

    // k == 3: TRIAD -> {n0, n2, n0+12, n1+12}, i.e. the outer two tones first,
    // then the octave-up root followed by the octave-up middle tone. Because the
    // input is a Close voicing it is already ascending, so n1/n2 are simply the
    // lower and upper non-root tones - no interval analysis is needed.
    //
    // Deliberately positional rather than role-based (3rd/5th): for sus2, sus4,
    // dim and b5 chords the middle tone is not a 3rd, but the intended run is
    // still "outer tones, then octave-up root, then octave-up middle tone".
    // (rootPitchClass is therefore unused on this path.)
    static_cast<void> (rootPitchClass);

    return { notes[0], notes[2], notes[0] + 12, notes[1] + 12 };
}

std::vector<int> PlaybackBuilder::ensureMinArpeggioNotes (std::vector<int> notes, int rootPitchClass)
{
    const int k = static_cast<int> (notes.size());
    if (k <= 1 || k >= 4)
        return notes;

    if (k == 2)
    {
        notes.push_back (notes[0] + 12);
        notes.push_back (notes[1] + 12);
        std::sort (notes.begin(), notes.end());
        return notes;
    }

    // k == 3: add the root one octave up (skip past any duplicate).
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

    int candidate = rootNote + 12;
    while (std::find (notes.begin(), notes.end(), candidate) != notes.end())
        candidate += 12;

    notes.push_back (candidate);
    std::sort (notes.begin(), notes.end());
    return notes;
}

namespace
{
/** Builds the one-cycle pattern sequence for the given arpeggio pattern.
    useOpenUpDown: true only for the Open-voicing DYAD/TRIAD case, where UpDown
    uses the new 8-note period. Otherwise (Close voicing, or 4+ note chords) the
    legacy period pattern (n0..nk-1 then nk-2..n1) is used. */
std::vector<int> buildPatternSequence (const std::vector<int>& base,
                                        ArpPattern pattern,
                                        int rootPitchClass,
                                        bool useOpenUpDown)
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
            if (useOpenUpDown && k == 4)
            {
                // 8-note period: b0 b1 b2 b3 top b3 b2 b1.
                // The ascending run is b0 b1 b2 b3 top, so top must sit above b3.
                // It is the lowest octave transposition of the root that clears
                // b3, computed by stepping up in octaves rather than assuming a
                // fixed +24, so the invariant holds for any base spacing.
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

                int top = rootNote;
                while (top <= base[3])
                    top += 12;
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

        if (settings.arpPattern == ArpPattern::Off)
        {
            // Block chord: the voicing (Close/Open) applies here.
            auto chordNotes = Voicing::midiNotes (*fc.chord, settings.voicing);
            if (chordNotes.empty())
            {
                notes.emplace_back();
                beats.push_back (fc.beats);
                displayIndices.push_back (i);
                continue;
            }
            notes.push_back (chordNotes);
            beats.push_back (fc.beats);
            displayIndices.push_back (i);
            continue;
        }

        // Number of chord tones is decided from the chord itself (independent of
        // voicing and of any explicit bass): DYAD=2, TRIAD/sus/aug/dim=3, else 4+.
        const int chordToneCount = static_cast<int> (ChordModel::effectiveIntervals (*fc.chord).size());
        const bool isSmallChord = (chordToneCount <= 3);
        const bool openArp = (settings.voicing == Voicing::Style::Open);

        // Arpeggio note material:
        //  - 4+ note chords: walk the voiced notes as-is, so the Close/Open
        //    setting applies directly (Open = drop-2 + bass).
        //  - DYAD/TRIAD: always take the plain stacked (Close) tones as material;
        //    the Close/Open choice only changes how they are re-stacked below.
        //    For Open, the run starts one octave lower so that C5, C and CM7 (and
        //    likewise D5, D, DM7 for any root) all begin on the same pitch. A 4-note
        //    Open voicing adds its bass at rootMidi - 12, so its lowest note is an
        //    octave below the default base; starting small chords at 48 matches that.
        static constexpr int kDefaultOctaveBase = 60; // C4
        static constexpr int kOpenArpOctaveBase = 48; // C3 (= lowest note of an Open 4-note voicing)
        const int octaveBase = (isSmallChord && openArp) ? kOpenArpOctaveBase
                                                         : kDefaultOctaveBase;
        auto chordNotes = Voicing::midiNotes (*fc.chord,
                                              isSmallChord ? Voicing::Style::Close
                                                           : settings.voicing,
                                              octaveBase);

        if (chordNotes.empty())
        {
            // Should not happen in practice, but treat like a rest.
            notes.emplace_back();
            beats.push_back (fc.beats);
            displayIndices.push_back (i);
            continue;
        }

        // Small chords (DYAD/TRIAD) get re-stacked to a 4-note base:
        //  - Open : role-order stacking (root/5th/root'/3rd') + 8-note UpDown.
        //  - Close: pre-v19.2 ascending octave-fill + legacy UpDown.
        // 4+ note chords keep the voiced notes and always use the legacy UpDown.
        const bool useOpenUpDown = isSmallChord && openArp;
        if (isSmallChord)
            chordNotes = openArp ? arpeggioBaseNotes (chordNotes, fc.chord->rootPitchClass)
                                 : ensureMinArpeggioNotes (chordNotes, fc.chord->rootPitchClass);

        const auto seq = buildPatternSequence (chordNotes, settings.arpPattern,
                                                fc.chord->rootPitchClass,
                                                useOpenUpDown);
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
