#include <JuceHeader.h>
#include "../Source/PlayModeSelection.h"

using namespace reharm;

namespace
{
/** All 21 reachable selections: 3 block voicings + (3 patterns x 2 voicings x 3 rates). */
std::vector<PlayModeSelection> allReachableSelections()
{
    std::vector<PlayModeSelection> out;

    for (auto v : { Voicing::Style::Close, Voicing::Style::Open, Voicing::Style::Auto })
        out.push_back ({ ArpPattern::Off, v, ArpRate::Eighth });

    for (auto p : { ArpPattern::Up, ArpPattern::Down, ArpPattern::UpDown })
        for (auto v : { Voicing::Style::Close, Voicing::Style::Open })
            for (auto r : { ArpRate::Quarter, ArpRate::Eighth, ArpRate::Sixteenth })
                out.push_back ({ p, v, r });

    return out;
}

/** All 36 (pattern, voicing, rate) combinations, including unreachable ones
    (Arp x Auto), used to exercise normalize(). */
std::vector<PlayModeSelection> allCombinations()
{
    std::vector<PlayModeSelection> out;
    for (auto p : { ArpPattern::Off, ArpPattern::Up, ArpPattern::Down, ArpPattern::UpDown })
        for (auto v : { Voicing::Style::Close, Voicing::Style::Open, Voicing::Style::Auto })
            for (auto r : { ArpRate::Quarter, ArpRate::Eighth, ArpRate::Sixteenth })
                out.push_back ({ p, v, r });
    return out;
}
} // namespace

class PlayModeSelectionTest : public juce::UnitTest
{
public:
    PlayModeSelectionTest() : juce::UnitTest ("PlayModeSelection") {}

    void runTest() override
    {
        beginTest ("Round trip: every reachable id survives toMenuId -> fromMenuId");
        {
            const auto selections = allReachableSelections();
            expectEquals ((int) selections.size(), 21);

            for (const auto& s : selections)
            {
                const int id = toMenuId (s);
                PlayModeSelection back;
                const bool ok = fromMenuId (id, back);
                expect (ok);
                expect (back == s);
            }
        }

        beginTest ("Display/state agreement: buttonLabel never disagrees with the selection it renders");
        {
            for (const auto& s : allReachableSelections())
            {
                const auto label = buttonLabel (s);

                expect (label.contains ("Auto") == (s.voicing == Voicing::Style::Auto));
                expect (label.contains ("Open") == (s.voicing == Voicing::Style::Open));
                expect (label.contains ("Close") == (s.voicing == Voicing::Style::Close));

                expect (label.startsWith ("Block") == (s.pattern == ArpPattern::Off));
                expect (label.startsWith ("Arp") == (s.pattern != ArpPattern::Off));

                expect (label.contains (rateLabel (s.rate)) == (s.pattern != ArpPattern::Off));
            }
        }

        beginTest ("normalize invariant: pattern != Off implies voicing != Auto");
        {
            const auto combos = allCombinations();
            expectEquals ((int) combos.size(), 36);

            for (const auto& s : combos)
            {
                const auto n = normalize (s);
                expect (! (n.pattern != ArpPattern::Off && n.voicing == Voicing::Style::Auto));
            }
        }

        beginTest ("normalize is idempotent");
        {
            for (const auto& s : allCombinations())
            {
                const auto once = normalize (s);
                const auto twice = normalize (once);
                expect (twice == once);
            }
        }

        beginTest ("Unreachable input: {Up, Auto, Eighth} maps to the same id as {Up, Close, Eighth}");
        {
            const PlayModeSelection unreachable { ArpPattern::Up, Voicing::Style::Auto, ArpRate::Eighth };
            const PlayModeSelection collapsed   { ArpPattern::Up, Voicing::Style::Close, ArpRate::Eighth };
            expectEquals (toMenuId (unreachable), toMenuId (collapsed));
        }

        beginTest ("Invalid ids are rejected without touching the output");
        {
            // patternIdx = 3 (out of range: only Up/Down/UpDown = 0..2 exist).
            const int patternOutOfRange = 100 + 20 * 3;
            // rateIdx = 3 (out of range: only Quarter/Eighth/Sixteenth = 0..2 exist).
            const int rateOutOfRange = 100 + 3;

            for (const int badId : { 0, 4, 99, patternOutOfRange, rateOutOfRange })
            {
                PlayModeSelection out { ArpPattern::Down, Voicing::Style::Open, ArpRate::Sixteenth };
                const auto before = out;
                const bool ok = fromMenuId (badId, out);
                expect (! ok);
                expect (out == before);
            }
        }

        beginTest ("Existing id scheme is preserved (regression / session+menu compatibility)");
        {
            expectEquals (toMenuId ({ ArpPattern::Off, Voicing::Style::Close, ArpRate::Quarter }), 1);
            expectEquals (toMenuId ({ ArpPattern::Off, Voicing::Style::Close, ArpRate::Sixteenth }), 1);
            expectEquals (toMenuId ({ ArpPattern::Off, Voicing::Style::Open, ArpRate::Quarter }), 2);
            expectEquals (toMenuId ({ ArpPattern::Off, Voicing::Style::Open, ArpRate::Sixteenth }), 2);
            expectEquals (toMenuId ({ ArpPattern::Up, Voicing::Style::Close, ArpRate::Quarter }), 100);
        }

        beginTest ("applyMenuId keeps the remembered rate when switching to Block");
        {
            // Block ids carry no rate, so picking one must not reset the arpeggio
            // rate the user chose earlier.
            const PlayModeSelection arp16 { ArpPattern::Up, Voicing::Style::Close, ArpRate::Sixteenth };

            for (const int blockId : { 1, 2, 3 })
            {
                PlayModeSelection out;
                expect (applyMenuId (blockId, arp16, out));
                expectEquals ((int) out.pattern, (int) ArpPattern::Off);
                expectEquals ((int) out.rate, (int) ArpRate::Sixteenth);
            }

            // Going back to an arpeggio item still takes the rate from the id itself.
            PlayModeSelection back;
            expect (applyMenuId (toMenuId ({ ArpPattern::Down, Voicing::Style::Open, ArpRate::Quarter }),
                                 PlayModeSelection { ArpPattern::Off, Voicing::Style::Auto, ArpRate::Sixteenth },
                                 back));
            expectEquals ((int) back.rate, (int) ArpRate::Quarter);
            expectEquals ((int) back.pattern, (int) ArpPattern::Down);
            expectEquals ((int) back.voicing, (int) Voicing::Style::Open);

            // Unknown ids are still rejected and leave the output untouched.
            PlayModeSelection untouched { ArpPattern::UpDown, Voicing::Style::Open, ArpRate::Quarter };
            const auto before = untouched;
            expect (! applyMenuId (99, arp16, untouched));
            expect (untouched == before);
        }
    }
};

static PlayModeSelectionTest playModeSelectionTest;
