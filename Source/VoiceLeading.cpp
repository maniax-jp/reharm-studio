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

// Drops the voice(s) at the given 1-based positions counted from the top of an
// ascending stack down an octave, then re-sorts. fromTopOneBased == {2} is the
// standard jazz drop-2 (second-highest voice moved below the rest); {3} is
// drop-3; {2, 4} is drop-2-and-4. Positions beyond the stack size are a no-op
// (callers guard with a size check before using drop3/drop24).
std::vector<int> dropVoicing (std::vector<int> stack, std::initializer_list<int> fromTopOneBased)
{
    const int n = static_cast<int> (stack.size());
    for (int pos : fromTopOneBased)
    {
        const int idx = n - pos;
        if (idx >= 0 && idx < n)
            stack[static_cast<size_t> (idx)] -= 12;
    }
    std::sort (stack.begin(), stack.end());
    return stack;
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

bool VoiceLeader::hasInherentSemitone (const Chord& c)
{
    const auto ivals = ChordModel::effectiveIntervals (c);
    for (size_t i = 1; i < ivals.size(); ++i)
        if (ivals[i] - ivals[i - 1] == 1)
            return true;
    return false;
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
            std::vector<int> closedCand;
            closedCand.reserve (static_cast<size_t> (n));
            for (int i = 0; i < n; ++i)
                closedCand.push_back (o.anchorLowest + rootPc + 12 * k + shapes[static_cast<size_t> (r)][static_cast<size_t> (i)]);

            // For each closed rotation, also offer its standard drop derivatives.
            // A closed voicing's non-root inversions can be forced into an
            // adjacent semitone (e.g. every closed inversion of a maj7 but the
            // root position); opening the stack via drop-2/3/24 gives the search
            // a way to keep the same pitch classes without that clash.
            std::vector<std::vector<int>> variants;
            variants.push_back (closedCand);
            if (n >= 3)
                variants.push_back (dropVoicing (closedCand, { 2 }));       // drop2
            if (n >= 4)
            {
                variants.push_back (dropVoicing (closedCand, { 3 }));       // drop3
                variants.push_back (dropVoicing (closedCand, { 2, 4 }));    // drop2-and-4
            }

            for (auto& cand : variants)
            {
                if (cand[0] < o.lowestMin || cand[0] > o.lowestMax)
                    continue;

                if (cand.back() - cand.front() > o.maxSpan)
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

    // Avoid a minor 2nd between adjacent voices: the algorithm should not create a
    // half-step clash the chord itself did not ask for. Counted per occurrence, and
    // deliberately a penalty rather than a hard veto -- so that a candidate with a
    // semitone can still win when it saves enough movement (a soft preference, not
    // a ban). In practice a semitone-free option is almost always available anyway:
    // candidates() includes drop voicings alongside closed ones, and a drop-2 dodges
    // the clash every non-root closed inversion of a maj7 has, e.g. {-1,4,7,12}
    // (= B E G C) for CM7. A large movement saving can still outweigh the penalty.
    // Note: exactly 1 semitone only. A minor 9th (13) between adjacent voices is a
    // normal open sonority and is not penalised.
    for (size_t i = 1; i < candStack.size(); ++i)
        if (candStack[i] - candStack[i - 1] == 1)
            result += o.semitonePenalty;

    // Prefer compact voicings. Without this, drop forms win by default: a wider
    // voicing can sit near more of the previous chord's notes, so each chord of a
    // repeating loop spreads a little further and the bottom ratchets downwards --
    // the same four bars sounded lower on every repeat. Spans up to preferredSpan
    // are free, so the standard drop-2 of a seventh chord (span 13, e.g. B E G C
    // for CM7) is still reachable when it genuinely leads better.
    if (candStack.size() >= 2)
    {
        const int span = candStack.back() - candStack.front();
        if (span > o.preferredSpan)
            result += o.spanWeight * (span - o.preferredSpan);
    }

    return result;
}

std::vector<int> VoiceLeader::next (const Chord& c)
{
    // Auto considers both closed and drop voicings as candidates (see
    // candidates()), but it does not call into or duplicate Voicing::Style::Open's
    // drop-2 implementation -- candidates() enumerates its own placements, so this
    // search never needs to stay in sync with Open's.

    if (! prevStack.has_value())
    {
        prevStack = closeStack (c, opts.anchorLowest);
        return Voicing::midiNotes (c, Voicing::Style::Close);
    }

    // Chords whose own close voicing already contains a semitone (Csus4(b5),
    // C6(#5), ...) are exempt: penalising them would only push the voicing apart
    // to escape an interval that defines the chord.
    Options searchOpts = opts;
    if (hasInherentSemitone (c))
        searchOpts.semitonePenalty = 0;

    const auto cands = candidates (c, searchOpts);
    if (cands.empty())
        return Voicing::midiNotes (c, Voicing::Style::Close);

    size_t bestIdx = 0;
    int bestCost = cost (*prevStack, cands[0], searchOpts);

    for (size_t i = 1; i < cands.size(); ++i)
    {
        const int cCost = cost (*prevStack, cands[i], searchOpts);
        if (cCost < bestCost
            || (cCost == bestCost && vectorLess (cands[i], cands[bestIdx])))
        {
            bestCost = cCost;
            bestIdx = i;
        }
    }

    auto stack = cands[bestIdx];
    prevStack = stack;

    // The explicit (slash-chord) bass is excluded from the search/cost entirely and
    // placed below the lowest stack note here: it is a semitone-agnostic insertion,
    // not a candidate the penalty evaluates, so a user-specified C/B is never
    // second-guessed by this rule.
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
