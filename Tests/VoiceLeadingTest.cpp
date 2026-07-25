#include <JuceHeader.h>
#include "../Source/VoiceLeading.h"

#include <algorithm>
#include <cstdlib>
#include <set>

using namespace reharm;

namespace
{
// Bidirectional nearest-neighbour movement between two stacks, independent of
// VoiceLeader::cost's internal weighting (used by test 3 as a property check).
int bidirMovement (const std::vector<int>& a, const std::vector<int>& b)
{
    auto minDist = [] (int x, const std::vector<int>& Y)
    {
        int best = std::abs (x - Y[0]);
        for (size_t i = 1; i < Y.size(); ++i)
            best = std::min (best, std::abs (x - Y[i]));
        return best;
    };

    int total = 0;
    for (int x : a) total += minDist (x, b);
    for (int y : b) total += minDist (y, a);
    return total;
}

// True if some adjacent pair in the (ascending) stack is exactly 1 semitone apart.
bool hasAdjacentSemitone (const std::vector<int>& stack)
{
    for (size_t i = 1; i < stack.size(); ++i)
        if (stack[i] - stack[i - 1] == 1)
            return true;
    return false;
}
} // namespace

class VoiceLeadingTest : public juce::UnitTest
{
public:
    VoiceLeadingTest() : juce::UnitTest ("VoiceLeading") {}

    void runTest() override
    {
        beginTest ("1. First call == legacy Close, across all qualities/roots/basses");
        {
            const int numQualities = static_cast<int> (ChordQuality::NumQualities);
            for (int qi = 0; qi < numQualities; ++qi)
            {
                const auto q = static_cast<ChordQuality> (qi);
                for (int root = 0; root < 12; ++root)
                {
                    for (int bass : { -1, 4, 11 })
                    {
                        Chord c { root, q, bass };
                        VoiceLeader leader;
                        const auto got = leader.next (c);
                        const auto want = Voicing::midiNotes (c, Voicing::Style::Close);
                        expect (got == want);
                    }
                }
            }
        }

        beginTest ("2. I-IV-V-I fixed expected stacks (spec 4.5)");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord F { 5, ChordQuality::Major, -1 };
            const Chord G { 7, ChordQuality::Major, -1 };

            const auto c1 = leader.next (C);
            const auto f1 = leader.next (F);
            const auto g1 = leader.next (G);
            const auto c2 = leader.next (C);

            expect (c1 == std::vector<int> ({ 60, 64, 67 }));
            expect (f1 == std::vector<int> ({ 60, 65, 69 }));
            expect (g1 == std::vector<int> ({ 59, 62, 67 }));
            expect (c2 == std::vector<int> ({ 60, 64, 67 }));
        }

        beginTest ("3. Auto total movement < Close total movement over I-IV-V-I");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord F { 5, ChordQuality::Major, -1 };
            const Chord G { 7, ChordQuality::Major, -1 };
            const std::vector<Chord> prog { C, F, G, C };

            std::vector<std::vector<int>> autoStacks;
            for (const auto& c : prog)
                autoStacks.push_back (leader.next (c));

            std::vector<std::vector<int>> closeStacks;
            for (const auto& c : prog)
                closeStacks.push_back (Voicing::midiNotes (c, Voicing::Style::Close));

            int autoTotal = 0, closeTotal = 0;
            for (size_t i = 1; i < prog.size(); ++i)
            {
                autoTotal += bidirMovement (autoStacks[i - 1], autoStacks[i]);
                closeTotal += bidirMovement (closeStacks[i - 1], closeStacks[i]);
            }

            expect (autoTotal < closeTotal);
        }

        beginTest ("4. Determinism: two independent leaders on the same 8-chord sequence match");
        {
            const std::vector<Chord> prog {
                Chord { 0, ChordQuality::Major, -1 },
                Chord { 5, ChordQuality::Major, -1 },
                Chord { 7, ChordQuality::Major, -1 },
                Chord { 0, ChordQuality::Major, -1 },
                Chord { 9, ChordQuality::Minor, -1 },
                Chord { 2, ChordQuality::Minor7, -1 },
                Chord { 7, ChordQuality::Dominant7, -1 },
                Chord { 0, ChordQuality::Major7, -1 },
            };

            VoiceLeader leaderA;
            VoiceLeader leaderB;

            for (const auto& c : prog)
            {
                const auto a = leaderA.next (c);
                const auto b = leaderB.next (c);
                expect (a == b);
            }
        }

        beginTest ("5. Explicit bass is always the lowest note, at the given pitch class");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord FoverA { 5, ChordQuality::Major, 9 };   // F/A
            const Chord GoverB { 7, ChordQuality::Major, 11 };  // G/B

            leader.next (C);
            const auto fOverA = leader.next (FoverA);
            const auto gOverB = leader.next (GoverB);

            expect (! fOverA.empty());
            expectEquals (((fOverA[0] % 12) + 12) % 12, 9);
            expect (fOverA[0] < fOverA[1]);

            expect (! gOverB.empty());
            expectEquals (((gOverB[0] % 12) + 12) % 12, 11);
            expect (gOverB[0] < gOverB[1]);
        }

        beginTest ("6. Range: from the 2nd result onward, lowest note in [52,67], no duplicate notes");
        {
            VoiceLeader leader;
            const VoiceLeader::Options opts; // defaults: lowestMin=52, lowestMax=67
            const int numQualities = static_cast<int> (ChordQuality::NumQualities);

            bool first = true;
            for (int root = 0; root < 12; ++root)
            {
                for (int qi = 0; qi < numQualities; ++qi)
                {
                    const auto q = static_cast<ChordQuality> (qi);
                    Chord c { root, q, -1 };
                    const auto stack = leader.next (c);

                    if (first)
                    {
                        first = false;
                        continue;
                    }

                    expect (! stack.empty());
                    expect (stack[0] >= opts.lowestMin && stack[0] <= opts.lowestMax);
                    expect (std::adjacent_find (stack.begin(), stack.end()) == stack.end());
                }
            }
        }

        beginTest ("7. No drift: I-IV-V-I repeated 8 cycles matches cycle 1 exactly, every cycle");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord F { 5, ChordQuality::Major, -1 };
            const Chord G { 7, ChordQuality::Major, -1 };
            const std::vector<Chord> prog { C, F, G, C };

            std::vector<std::vector<int>> firstCycle;
            for (const auto& c : prog)
                firstCycle.push_back (leader.next (c));

            for (int cycle = 1; cycle < 8; ++cycle)
            {
                for (size_t i = 0; i < prog.size(); ++i)
                {
                    const auto result = leader.next (prog[i]);
                    expect (result == firstCycle[i]);
                }
            }
        }

        beginTest ("8. Note-count change: C -> CM7 -> C");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord CM7 { 0, ChordQuality::Major7, -1 };

            leader.next (C);
            const auto cm7Result = leader.next (CM7);
            expect (cm7Result == std::vector<int> ({ 60, 64, 67, 71 }));

            const auto cResult = leader.next (C);
            expect (cResult == std::vector<int> ({ 60, 64, 67 }));
        }

        beginTest ("9. Bottom-cluster penalty rejects {59,60,64,67} for C -> CM7");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord CM7 { 0, ChordQuality::Major7, -1 };

            leader.next (C);
            const auto cm7Result = leader.next (CM7);
            expect (cm7Result != std::vector<int> ({ 59, 60, 64, 67 }));
        }

        beginTest ("10. Same chord repeated 3x gives identical results (no flicker)");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };

            const auto r1 = leader.next (C);
            const auto r2 = leader.next (C);
            const auto r3 = leader.next (C);

            expect (r1 == r2);
            expect (r2 == r3);
        }

        beginTest ("11. reset() clears state; hasPrevious() transitions false->true");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord Dm7 { 2, ChordQuality::Minor7, -1 };

            expect (! leader.hasPrevious());
            const auto first = leader.next (C);
            expect (leader.hasPrevious());
            expect (first == Voicing::midiNotes (C, Voicing::Style::Close));

            leader.next (Dm7); // move state away from C
            leader.reset();
            expect (! leader.hasPrevious());

            const auto afterReset = leader.next (C);
            expect (leader.hasPrevious());
            expect (afterReset == Voicing::midiNotes (C, Voicing::Style::Close));
        }

        beginTest ("12. Degenerate single-tone chord does not corrupt state");
        {
            // Power5 with Omit5 removes the only omittable role (the fifth),
            // leaving effectiveIntervals == {0}: a single-tone "chord". The UI
            // never offers this (omit5 is unavailable for Power5), but a
            // hand-built Chord can still reach it, so VoiceLeader must not
            // crash or corrupt its state when it does.
            Chord single { 0, ChordQuality::Power5, -1, Omit5 };
            expectEquals ((int) ChordModel::effectiveIntervals (single).size(), 1);

            VoiceLeader leader;
            const auto r1 = leader.next (single);
            expectEquals ((int) r1.size(), 1);
            expect (leader.hasPrevious());

            const auto r2 = leader.next (single);
            expectEquals ((int) r2.size(), 1);

            // Following chord must still process normally.
            const Chord C { 0, ChordQuality::Major, -1 };
            const auto r3 = leader.next (C);
            expectEquals ((int) r3.size(), 3);
            expect (std::adjacent_find (r3.begin(), r3.end()) == r3.end());
        }

        beginTest ("13. Pure helpers: candidates() canonical/dedup/in-window, cost() movement 0 for identical stacks");
        {
            const Chord C { 0, ChordQuality::Major, -1 };
            const VoiceLeader::Options opts;

            const auto cands = VoiceLeader::candidates (C, opts);
            expect (! cands.empty());

            std::set<std::vector<int>> seen;
            for (const auto& cand : cands)
            {
                expect (cand[0] >= opts.lowestMin && cand[0] <= opts.lowestMax);
                expect (seen.find (cand) == seen.end()); // no duplicates
                seen.insert (cand);
            }

            VoiceLeader::Options movementOnly;
            movementOnly.movementWeight = 1;
            movementOnly.centerWeight = 0;
            movementOnly.bottomClusterPenalty = 0;

            const std::vector<int> stack { 60, 64, 67 };
            expectEquals (VoiceLeader::cost (stack, stack, movementOnly), 0);
        }

        beginTest ("14. hasInherentSemitone classification");
        {
            // Chords whose own basic close voicing already contains a semitone.
            const Chord sus4Flat5 { 0, ChordQuality::Sus4, -1, 0, FifthAlt::Flat, 0 };       // Csus4(b5)
            const Chord sixthSharp5 { 0, ChordQuality::Sixth, -1, 0, FifthAlt::Sharp, 0 };    // C6(#5)
            const Chord minor6Sharp5 { 0, ChordQuality::Minor6, -1, 0, FifthAlt::Sharp, 0 };  // Cm6(#5)
            const Chord sixthSus4Flat5 { 0, ChordQuality::SixthSus4, -1, 0, FifthAlt::Flat, 0 }; // C6sus4(b5)

            expect (VoiceLeader::hasInherentSemitone (sus4Flat5));
            expect (ChordModel::effectiveIntervals (sus4Flat5) == std::vector<int> ({ 0, 5, 6 }));

            expect (VoiceLeader::hasInherentSemitone (sixthSharp5));
            expect (ChordModel::effectiveIntervals (sixthSharp5) == std::vector<int> ({ 0, 4, 8, 9 }));

            expect (VoiceLeader::hasInherentSemitone (minor6Sharp5));
            expect (VoiceLeader::hasInherentSemitone (sixthSus4Flat5));

            // Chords whose basic close voicing does not contain a semitone (inversions
            // may introduce one, but that is not what hasInherentSemitone answers).
            const Chord major { 0, ChordQuality::Major, -1 };
            const Chord major7 { 0, ChordQuality::Major7, -1 };
            const Chord dom7 { 0, ChordQuality::Dominant7, -1 };
            const Chord minor7 { 0, ChordQuality::Minor7, -1 };
            const Chord dim7 { 0, ChordQuality::Diminished7, -1 };
            const Chord aug7 { 0, ChordQuality::Augmented7, -1 };
            const Chord augMajor7 { 0, ChordQuality::AugmentedMajor7, -1 };
            const Chord blk { 0, ChordQuality::Blackadder, -1 };
            const Chord sus4 { 0, ChordQuality::Sus4, -1 };
            const Chord sixth { 0, ChordQuality::Sixth, -1 };

            expect (! VoiceLeader::hasInherentSemitone (major));
            expect (! VoiceLeader::hasInherentSemitone (major7));
            expect (! VoiceLeader::hasInherentSemitone (dom7));
            expect (! VoiceLeader::hasInherentSemitone (minor7));
            expect (! VoiceLeader::hasInherentSemitone (dim7));
            expect (! VoiceLeader::hasInherentSemitone (aug7));
            expect (! VoiceLeader::hasInherentSemitone (augMajor7));
            expect (! VoiceLeader::hasInherentSemitone (blk));
            expect (! VoiceLeader::hasInherentSemitone (sus4));
            expect (! VoiceLeader::hasInherentSemitone (sixth));
        }

        beginTest ("15. Semitone penalty also catches a clash at the top of the stack "
                   "(the bottom-cluster penalty alone would miss it)");
        {
            const std::vector<int> prev { 60, 64, 67 };
            // CM7 1st-inversion-like stack: diffs 3,4,1 -- the semitone is between the
            // top two notes (71,72), and the bottom pair (64,67) is a 3rd, so the
            // existing bottom-cluster penalty does not fire on it.
            const std::vector<int> candWithClash { 64, 67, 71, 72 };
            const std::vector<int> candNoClash { 60, 64, 67, 71 }; // diffs 4,3,4 -- no semitone anywhere

            const VoiceLeader::Options defaultOpts;
            VoiceLeader::Options noPenalty = defaultOpts;
            noPenalty.semitonePenalty = 0;

            const int withPenalty = VoiceLeader::cost (prev, candWithClash, defaultOpts);
            const int withoutPenalty = VoiceLeader::cost (prev, candWithClash, noPenalty);
            expectEquals (withPenalty - withoutPenalty, defaultOpts.semitonePenalty);

            const int noClashWithPenalty = VoiceLeader::cost (prev, candNoClash, defaultOpts);
            const int noClashWithoutPenalty = VoiceLeader::cost (prev, candNoClash, noPenalty);
            expectEquals (noClashWithPenalty - noClashWithoutPenalty, 0);
        }

        beginTest ("16. Semitone avoidance changes the outcome: C -> G -> CM7");
        {
            VoiceLeader leader;
            const Chord C { 0, ChordQuality::Major, -1 };
            const Chord G { 7, ChordQuality::Major, -1 };
            const Chord CM7 { 0, ChordQuality::Major7, -1 };

            const auto c1 = leader.next (C);
            const auto g1 = leader.next (G);
            const auto cm7 = leader.next (CM7);

            expect (c1 == std::vector<int> ({ 60, 64, 67 }));
            expect (g1 == std::vector<int> ({ 59, 62, 67 }));
            expect (cm7 == std::vector<int> ({ 60, 64, 67, 71 }));
            // Without the semitone penalty, {59,60,64,67} (cost 19) narrowly beats
            // {60,64,67,71} (cost 20); the penalty (8, applied once) flips that to 27
            // vs 20, so this specifically exercises the new rule rather than being
            // incidental.
            expect (cm7 != std::vector<int> ({ 59, 60, 64, 67 }));
        }

        beginTest ("17. Exempt chords keep their inherent semitone; non-exempt chords avoid it "
                   "even under an extreme penalty");
        {
            VoiceLeader::Options extreme;
            extreme.semitonePenalty = 1000;

            // Exempt: Csus4(b5)'s own close voicing already contains a semitone, so the
            // penalty is waived for it. If the wiring were wrong (penalty not waived),
            // an extreme penalty would push the result to a semitone-free rotation.
            {
                VoiceLeader leader (extreme);
                const Chord C { 0, ChordQuality::Major, -1 };
                const Chord sus4Flat5 { 0, ChordQuality::Sus4, -1, 0, FifthAlt::Flat, 0 };

                leader.next (C);
                const auto result = leader.next (sus4Flat5);
                expect (hasAdjacentSemitone (result));
            }

            // Non-exempt (contrast): CM7's own close voicing has no semitone, so the
            // extreme penalty applies in full and pins the result to the root-position
            // close voicing (the only rotation with no adjacent semitone).
            {
                VoiceLeader leader (extreme);
                const Chord B { 11, ChordQuality::Major, -1 };
                const Chord CM7 { 0, ChordQuality::Major7, -1 };

                leader.next (B);
                const auto result = leader.next (CM7);
                expect (! hasAdjacentSemitone (result));
                expect (result == std::vector<int> ({ 60, 64, 67, 71 }));
            }
        }

        beginTest ("18. Default semitonePenalty regression");
        {
            const VoiceLeader::Options defaultOpts;
            expectEquals (defaultOpts.semitonePenalty, 8);
        }
    }
};

static VoiceLeadingTest voiceLeadingTest;
