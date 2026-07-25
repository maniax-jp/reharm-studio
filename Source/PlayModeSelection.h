#pragma once

#include "PlaybackSettings.h"
#include <juce_core/juce_core.h>

namespace reharm
{

/** The user's play-mode choice as one value: what the play-mode button shows and
    what the builder receives must always be derived from the same struct. */
struct PlayModeSelection
{
    ArpPattern pattern = ArpPattern::Off;
    Voicing::Style voicing = Voicing::Style::Auto;
    ArpRate rate = ArpRate::Eighth;

    bool operator== (const PlayModeSelection& o) const noexcept
    {
        return pattern == o.pattern && voicing == o.voicing && rate == o.rate;
    }
    bool operator!= (const PlayModeSelection& o) const noexcept { return ! (*this == o); }
};

/** Collapses unreachable combinations. Auto is a block-chord feature and is never
    offered for arpeggios, so (pattern != Off && voicing == Auto) becomes Close.
    This is the single gate that keeps the UI and the engine from disagreeing. */
inline PlayModeSelection normalize (PlayModeSelection s) noexcept
{
    if (s.pattern != ArpPattern::Off && s.voicing == Voicing::Style::Auto)
        s.voicing = Voicing::Style::Close;
    return s;
}

namespace detail
{
    /** Play-mode popup item IDs.
        Block: 1 = Close, 2 = Open, 3 = Auto.
        Arp:   100 + patternIdx*20 + voicingIdx*10 + rateIdx
               patternIdx Up=0 Down=1 UpDown=2 ; voicingIdx Close=0 Open=1 ; rateIdx 0..2. */
    constexpr int kBlockClose = 1;
    constexpr int kBlockOpen  = 2;
    constexpr int kBlockAuto  = 3;
    constexpr int kArpBase    = 100;

    inline int patternIdxOf (ArpPattern p) noexcept
    {
        switch (p)
        {
            case ArpPattern::Up:     return 0;
            case ArpPattern::Down:   return 1;
            case ArpPattern::UpDown: return 2;
            default:                 return -1;
        }
    }

    /** Auto is never passed in here: normalize() has already collapsed it to Close
        whenever pattern != Off, which is the only case that reaches this helper. */
    inline int voicingIdxOf (Voicing::Style v) noexcept
    {
        return v == Voicing::Style::Open ? 1 : 0;
    }

    inline int arpItemId (int patternIdx, int voicingIdx, ArpRate r) noexcept
    {
        return kArpBase + patternIdx * 20 + voicingIdx * 10 + (int) r;
    }
} // namespace detail

/** Menu item id. Block: 1=Close, 2=Open, 3=Auto.
    Arp: 100 + patternIdx*20 + voicingIdx*10 + rateIdx
         (patternIdx Up=0 Down=1 UpDown=2; voicingIdx Close=0 Open=1; rateIdx 0..2).
    Unreachable input is normalized first, so this never returns an invalid id. */
inline int toMenuId (const PlayModeSelection& sIn) noexcept
{
    const auto s = normalize (sIn);

    if (s.pattern == ArpPattern::Off)
    {
        if (s.voicing == Voicing::Style::Close) return detail::kBlockClose;
        if (s.voicing == Voicing::Style::Open)  return detail::kBlockOpen;
        return detail::kBlockAuto;
    }

    return detail::arpItemId (detail::patternIdxOf (s.pattern), detail::voicingIdxOf (s.voicing), s.rate);
}

/** Inverse of toMenuId. Returns false for an unknown id (out is untouched). */
inline bool fromMenuId (int id, PlayModeSelection& out) noexcept
{
    if (id == detail::kBlockClose) { out = normalize ({ ArpPattern::Off, Voicing::Style::Close, ArpRate::Eighth }); return true; }
    if (id == detail::kBlockOpen)  { out = normalize ({ ArpPattern::Off, Voicing::Style::Open,  ArpRate::Eighth }); return true; }
    if (id == detail::kBlockAuto)  { out = normalize ({ ArpPattern::Off, Voicing::Style::Auto,  ArpRate::Eighth }); return true; }

    if (id < detail::kArpBase)
        return false;

    const int enc = id - detail::kArpBase;
    const int patternIdx = enc / 20;
    const int voicingIdx = (enc % 20) / 10;
    const int rateIdx    = enc % 10;

    if (patternIdx < 0 || patternIdx > 2 || voicingIdx < 0 || voicingIdx > 1 || rateIdx < 0 || rateIdx > 2)
        return false;

    static const ArpPattern patterns[3] = { ArpPattern::Up, ArpPattern::Down, ArpPattern::UpDown };
    static const Voicing::Style voicings[2] = { Voicing::Style::Close, Voicing::Style::Open };

    out = normalize ({ patterns[patternIdx], voicings[voicingIdx], static_cast<ArpRate> (rateIdx) });
    return true;
}

/** Applies a menu id on top of the current selection. Block ids carry no rate
    (the button does not show one), so the remembered arpeggio rate is preserved
    instead of being reset -- switching Arp -> Block -> Arp must not silently
    change the rate the user picked. Returns false for an unknown id. */
inline bool applyMenuId (int id, const PlayModeSelection& prev, PlayModeSelection& out) noexcept
{
    PlayModeSelection parsed;
    if (! fromMenuId (id, parsed))
        return false;

    if (parsed.pattern == ArpPattern::Off)
        parsed.rate = prev.rate;

    out = normalize (parsed);
    return true;
}

/** "1/4" / "1/8" / "1/16". */
inline juce::String rateLabel (ArpRate r)
{
    switch (r)
    {
        case ArpRate::Quarter:   return "1/4";
        case ArpRate::Sixteenth: return "1/16";
        case ArpRate::Eighth:
        default:                 return "1/8";
    }
}

/** Play-mode button text, e.g. "Block Auto" / "Arp Up Close 1/8". */
inline juce::String buttonLabel (const PlayModeSelection& sIn)
{
    const auto s = normalize (sIn);

    juce::String voicingName;
    switch (s.voicing)
    {
        case Voicing::Style::Close: voicingName = "Close"; break;
        case Voicing::Style::Open:  voicingName = "Open"; break;
        case Voicing::Style::Auto:
        default:                    voicingName = "Auto"; break;
    }

    if (s.pattern == ArpPattern::Off)
        return "Block " + voicingName;

    juce::String styleName;
    switch (s.pattern)
    {
        case ArpPattern::Up:     styleName = "Arp Up"; break;
        case ArpPattern::Down:   styleName = "Arp Down"; break;
        case ArpPattern::UpDown: styleName = "Arp Up-Down"; break;
        case ArpPattern::Off:
        default:                 styleName = "Block"; break;
    }

    return styleName + " " + voicingName + " " + rateLabel (s.rate);
}

} // namespace reharm
