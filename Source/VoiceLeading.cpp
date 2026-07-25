#include "VoiceLeading.h"

#include <algorithm>
#include <cstdlib>

namespace reharm
{

namespace
{
int normalizePc (int pc) noexcept
{
    return ((pc % 12) + 12) % 12;
}

int minAbsDist (int x, const std::vector<int>& Y)
{
    if (Y.empty())
        return 0;

    int best = std::abs (x - Y[0]);
    for (size_t i = 1; i < Y.size(); ++i)
    {
        const int d = std::abs (x - Y[i]);
        if (d < best)
            best = d;
    }
    return best;
}

bool vectorLess (const std::vector<int>& a, const std::vector<int>& b)
{
    const size_t n = std::min (a.size(), b.size());
    for (size_t i = 0; i < n; ++i)
    {
        if (a[i] < b[i]) return true;
        if (a[i] > b[i]) return false;
    }
    return a.size() < b.size();
}
} // namespace

VoiceLeader::VoiceLeader() = default;

VoiceLeader::VoiceLeader (Options o)
    : opts (std::move (o))
{
}

void VoiceLeader::reset() noexcept
{
    prevStack.reset();
}

bool VoiceLeader::hasPrevious() const noexcept
{
    return prevStack.has_value();
}

std::vector<int> VoiceLeader::closeStack (const Chord& c, int octaveBase)
{
    const int rootPc = normalizePc (c.rootPitchClass);
    const int rootMidi = octaveBase + rootPc;

    std::vector<int> notes;
    for (int interval : ChordModel::effectiveIntervals (c))
        notes.push_back (rootMidi + interval);

    return notes;
}

std::vector<std::vector<int>> VoiceLeader::candidates (const Chord& c, const Options& o)
{
    const auto ivals = ChordModel::effectiveIntervals (c);
    const int n = static_cast<int> (ivals.size());
    if (n == 0)
        return {};

    const int rootPc = normalizePc (c.rootPitchClass);

    // shape(0) = intervals; shape(r) lifts the lowest tone by an octave and re-sorts.
    // For triads this yields root / 1st / 2nd inversion. With add9 etc. the stack can
    // span more than one octave so it is not a strict cyclic rotation, but the pitch
    // class set is preserved and every candidate remains a valid n-note voicing.
    std::vector<std::vector<int>> shapes;
    shapes.reserve (static_cast<size_t> (n));
    shapes.push_back (ivals);

    for (int r = 1; r < n; ++r)
    {
        auto shape = shapes[static_cast<size_t> (r - 1)];
        auto minIt = std::min_element (shape.begin(), shape.end());
        *minIt += 12;
        std::sort (shape.begin(), shape.end());
        shapes.push_back (std::move (shape));
    }

    // Never empty for a non-empty chord: r=0 candidates have
    //   cand[0] = anchorLowest + rootPc + 12*k.
    // rootPc in [0,3]  -> k=0 gives 60..63; [4,7] -> k=0 gives 64..67;
    // rootPc in [8,11] -> k=-1 gives 56..59. All lie inside [lowestMin, lowestMax]
    // for the default window [52, 67].
    std::vector<std::vector<int>> result;
    for (int r = 0; r < n; ++r)
    {
        for (int k = -o.octaveRadius; k <= o.octaveRadius; ++k)
        {
            std::vector<int> cand;
            cand.reserve (static_cast<size_t> (n));
            for (int i = 0; i < n; ++i)
                cand.push_back (o.anchorLowest + rootPc + 12 * k + shapes[static_cast<size_t> (r)][static_cast<size_t> (i)]);

            if (cand[0] < o.lowestMin || cand[0] > o.lowestMax)
                continue;

            bool duplicate = false;
            for (const auto& existing : result)
            {
                if (existing == cand)
                {
                    duplicate = true;
                    break;
                }
            }
            if (! duplicate)
                result.push_back (std::move (cand));
        }
    }

    return result;
}

int VoiceLeader::cost (const std::vector<int>& prevStack,
                       const std::vector<int>& candStack,
                       const Options& o)
{
    // Bidirectional nearest-neighbour sum: no explicit voice pairing is needed,
    // so 3-note -> 4-note (and reverse) needs no special case. Shared tones score
    // 0 in both directions, encoding "hold common tones" directly. Symmetry means
    // neither side is biased by having more notes.
    int movement = 0;
    if (! prevStack.empty())
    {
        for (int note : candStack)
            movement += minAbsDist (note, prevStack);
        for (int note : prevStack)
            movement += minAbsDist (note, candStack);
    }

    int result = o.movementWeight * movement;

    if (! candStack.empty())
        result += o.centerWeight * std::abs (candStack[0] - o.anchorLowest);

    // Bottom-cluster penalty: without it, C{60,64,67} -> CM7 prefers {59,60,64,67}
    // (movement 1 -> cost 3 before penalty) over {60,64,67,71} (movement 4 -> cost 8).
    // Penalty 8 flips that to 11 vs 8. Applied only to the bottom adjacent pair:
    // a major-7 half-step at the top is a normal sonority and must not be punished.
    if (candStack.size() >= 2 && candStack[1] - candStack[0] < 3)
        result += o.bottomClusterPenalty;

    return result;
}

std::vector<int> VoiceLeader::next (const Chord& c)
{
    // Auto rendering is close-position only; Open drop-2 is never applied here
    // (avoids duplicating Open logic and the risk of drifting out of sync with it).

    if (! prevStack.has_value())
    {
        prevStack = closeStack (c, opts.anchorLowest);
        return Voicing::midiNotes (c, Voicing::Style::Close);
    }

    const auto cands = candidates (c, opts);
    if (cands.empty())
        return Voicing::midiNotes (c, Voicing::Style::Close);

    size_t bestIdx = 0;
    int bestCost = cost (*prevStack, cands[0], opts);

    for (size_t i = 1; i < cands.size(); ++i)
    {
        const int cCost = cost (*prevStack, cands[i], opts);
        if (cCost < bestCost
            || (cCost == bestCost && vectorLess (cands[i], cands[bestIdx])))
        {
            bestCost = cCost;
            bestIdx = i;
        }
    }

    auto stack = cands[bestIdx];
    prevStack = stack;

    if (c.hasBass())
    {
        const int low = stack[0];
        const int bassPc = normalizePc (c.bassPitchClass);
        const int delta = ((low - bassPc) % 12 + 12) % 12;
        const int bass = low - (delta == 0 ? 12 : delta);
        stack.insert (stack.begin(), bass);
    }

    return stack;
}

} // namespace reharm
