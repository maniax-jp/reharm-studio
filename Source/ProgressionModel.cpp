#include "ProgressionModel.h"
#include <algorithm>

namespace reharm
{

/* ========================================================================
   ProgressionModel
   ======================================================================== */

ProgressionModel::ProgressionModel()
    : bars {}, numBars (4), key {0, true}
{
}

int ProgressionModel::getNumBars() const noexcept
{
    return numBars;
}

void ProgressionModel::setNumBars (int newNumBars)
{
    newNumBars = std::clamp (newNumBars, 1, maxBars);

    if (numBars != newNumBars)
    {
        numBars = newNumBars;
        notifyChanged();
    }
}

const Bar& ProgressionModel::getBar (int barIndex) const
{
    // Release-safe: clamp rather than assert on out-of-range indices.
    const int i = std::clamp (barIndex, 0, maxBars - 1);
    return bars [static_cast<size_t> (i)];
}

BarSubdivision ProgressionModel::getSubdivision (int barIndex) const
{
    // Release-safe: clamp to active bar range (numBars is always >= 1).
    const int i = std::clamp (barIndex, 0, std::max (0, numBars - 1));
    return bars [static_cast<size_t> (i)].subdivision;
}

void ProgressionModel::setSubdivision (int barIndex, BarSubdivision newSubdivision)
{
    if (barIndex < 0 || barIndex >= numBars)
        return;

    bars [static_cast<size_t> (barIndex)].subdivision = newSubdivision;
    notifyChanged();
}

std::optional<Chord> ProgressionModel::getChord (int barIndex, int slotIndex) const
{
    if (barIndex < 0 || barIndex >= numBars)
        return std::nullopt;
    if (slotIndex < 0 || slotIndex >= bars [static_cast<size_t> (barIndex)].numSlots())
        return std::nullopt;
    return bars [static_cast<size_t> (barIndex)].slots [static_cast<size_t> (slotIndex)];
}

void ProgressionModel::setChord (int barIndex, int slotIndex, std::optional<Chord> chord)
{
    if (barIndex < 0 || barIndex >= numBars)
        return;
    if (slotIndex < 0 || slotIndex >= bars [static_cast<size_t> (barIndex)].numSlots())
        return;

    bars [static_cast<size_t> (barIndex)].slots [static_cast<size_t> (slotIndex)] = std::move (chord);
    notifyChanged();
}

void ProgressionModel::clear()
{
    for (int i = 0; i < numBars; ++i)
    {
        bars [static_cast<size_t> (i)].slots.fill (std::nullopt);
    }
    notifyChanged();
}

const KeyContext& ProgressionModel::getKey() const noexcept
{
    return key;
}

void ProgressionModel::setKey (KeyContext newKey)
{
    key = newKey;
    notifyChanged();
}

ProgressionState ProgressionModel::captureState() const
{
    ProgressionState state;
    state.bars = bars;
    state.numBars = numBars;
    state.key = key;
    return state;
}

void ProgressionModel::restoreState (const ProgressionState& state)
{
    bars = state.bars;
    numBars = std::clamp (state.numBars, 1, maxBars);
    key = state.key;
    notifyChanged();
}

bool operator== (const Bar& a, const Bar& b) noexcept
{
    return a.subdivision == b.subdivision && a.slots == b.slots;
}

bool operator!= (const Bar& a, const Bar& b) noexcept
{
    return ! (a == b);
}

bool operator== (const KeyContext& a, const KeyContext& b) noexcept
{
    return a.tonicPitchClass == b.tonicPitchClass && a.isMajor == b.isMajor;
}

bool operator!= (const KeyContext& a, const KeyContext& b) noexcept
{
    return ! (a == b);
}

bool operator== (const ProgressionState& a, const ProgressionState& b) noexcept
{
    return a.bars == b.bars && a.numBars == b.numBars && a.key == b.key;
}

bool operator!= (const ProgressionState& a, const ProgressionState& b) noexcept
{
    return ! (a == b);
}

std::vector<FlatChord> ProgressionModel::flatten() const
{
    std::vector<FlatChord> result;
    result.reserve (static_cast<size_t> (totalSlots()));

    for (int b = 0; b < numBars; ++b)
    {
        const int numSlots = bars [static_cast<size_t> (b)].numSlots();
        const double beats = bars [static_cast<size_t> (b)].beatsPerSlot();

        for (int s = 0; s < numSlots; ++s)
        {
            result.push_back ({bars [static_cast<size_t> (b)].slots [static_cast<size_t> (s)], beats, b, s});
        }
    }
    return result;
}

int ProgressionModel::totalSlots() const noexcept
{
    int total = 0;
    for (int i = 0; i < numBars; ++i)
        total += bars [static_cast<size_t> (i)].numSlots();
    return total;
}

void ProgressionModel::beginBulkEdit() noexcept
{
    ++bulkEditDepth;
}

void ProgressionModel::endBulkEdit()
{
    if (bulkEditDepth <= 0)
        return;

    --bulkEditDepth;

    if (bulkEditDepth == 0 && bulkEditDirty)
    {
        bulkEditDirty = false;
        if (onChanged)
            onChanged();
    }
}

void ProgressionModel::notifyChanged()
{
    if (bulkEditDepth > 0)
    {
        bulkEditDirty = true;
        return;
    }

    if (onChanged)
        onChanged();
}

/* ========================================================================
   ProgressionPresets
   ======================================================================== */

const std::vector<PresetProgression>& ProgressionPresets::all()
{
    // Short names to keep the preset table below readable. A bar written as a
    // plain {degree, quality} is a single chord with no alternatives.
    using Alt   = PresetProgression::Alternative;
    using Alts  = std::vector<PresetProgression::Alternative>;
    using Specs = std::vector<PresetProgression::ChordSpec>;

    static const std::vector<PresetProgression> presets =
    {
        { "Royal Road (IV-V-iii-vi)",
          {
              {5, ChordQuality::Major7},
              {7, ChordQuality::Dominant7},
              {4, ChordQuality::Minor7},
              {9, ChordQuality::Minor}
          }
        },
        { "Canon (I-V-vi-iii-IV-I-IV-V)",
          {
              {0, ChordQuality::Major},
              {7, ChordQuality::Major},
              {9, ChordQuality::Minor},
              {4, ChordQuality::Minor},
              {5, ChordQuality::Major},
              {0, ChordQuality::Major},
              {5, ChordQuality::Major},
              {7, ChordQuality::Major}
          }
        },
        { "Pop Punk (I-V-vi-IV)",
          {
              {0, ChordQuality::Major},
              {7, ChordQuality::Major},
              {9, ChordQuality::Minor},
              {5, ChordQuality::Major}
          }
        },
        { "Komuro (vi-IV-V-I)",
          {
              {9, ChordQuality::Minor},
              {5, ChordQuality::Major},
              {7, ChordQuality::Major},
              {0, ChordQuality::Major}
          }
        },
        { "Marunouchi (IVM7-III7-vim7-I7)",
          {
              {5, ChordQuality::Major7},
              {4, ChordQuality::Dominant7},
              {9, ChordQuality::Minor7},
              {0, ChordQuality::Dominant7}
          }
        },
        { "Autumn Leaves (ii-V-I-IV-vii-III7-vi)",
          {
              {2, ChordQuality::Minor7},
              {7, ChordQuality::Dominant7},
              {0, ChordQuality::Major7},
              {5, ChordQuality::Major7},
              {11, ChordQuality::Minor7Flat5},
              {4, ChordQuality::Dominant7},
              {9, ChordQuality::Minor7},
              {9, ChordQuality::Minor7}
          }
        },
        { "Fly Me to the Moon (vi-ii-V-I-IV-vii-III7-vi)",
          {
              {9, ChordQuality::Minor7},
              {2, ChordQuality::Minor7},
              {7, ChordQuality::Dominant7},
              {0, ChordQuality::Major7},
              {5, ChordQuality::Major7},
              {11, ChordQuality::Minor7Flat5},
              {4, ChordQuality::Dominant7},
              {9, ChordQuality::Minor7}
          }
        },
        // | C | Bm7b5 E7 | Am7 | Gm7 C7 | F |
        // Two bars carry a ii-V pair, so those entries hold a two-chord run.
        { "Confirmation (I-viim7b5-III7-vim7-iim7-V7-IV)",
          {
              {0, ChordQuality::Major},
              { Alts {
                  Alt { Specs {
                      {11, ChordQuality::Minor7Flat5},
                      {4,  ChordQuality::Dominant7}
                  } }
              } },
              {9, ChordQuality::Minor7},
              { Alts {
                  Alt { Specs {
                      {7, ChordQuality::Minor7},
                      {0, ChordQuality::Dominant7}
                  } }
              } },
              {5, ChordQuality::Major}
          }
        },
        // | C | F | G | E | Am7 |, where the III may be a plain triad, a dominant
        // 7th, or the bVIdim that substitutes for it.
        { "Only One (I-IV-V-III-vim7)",
          {
              {0, ChordQuality::Major},
              {5, ChordQuality::Major},
              {7, ChordQuality::Major},
              { Alts {
                  Alt {4, ChordQuality::Major},
                  Alt {4, ChordQuality::Dominant7},
                  Alt {8, ChordQuality::Diminished}
              } },
              {9, ChordQuality::Minor7}
          }
        },
        // | C | Em/B | Edim/Bb | A7 | Dm | C#aug | F/C | G |
        // The descending bass is the point, so bar 3 accepts any inversion of the
        // Edim as well as anything else sitting on that bb3 bass - except an
        // augmented chord, which breaks the line. The closing V may be a triad or
        // a dominant 7th.
        { "Ochiba (I-iii-biiidim-VI7-iim-Iaug-IV-V)",
          {
              {0, ChordQuality::Major},
              { Alts { Alt {4, ChordQuality::Minor, 11} } },
              { Alts {
                  Alt {4, ChordQuality::Diminished, 10},
                  Alt {PresetProgression::Wildcard::AnyNonAugmentedOverBass, 10}
              } },
              {9, ChordQuality::Dominant7},
              {2, ChordQuality::Minor},
              {1, ChordQuality::Augmented},
              { Alts { Alt {5, ChordQuality::Major, 0} } },
              { Alts {
                  Alt {7, ChordQuality::Major},
                  Alt {7, ChordQuality::Dominant7}
              } }
          }
        },
        // | C | E7/B | Am | C7/G |, where bar 2 may instead run the cliche line
        // through an augmented chord, and bar 4 may split into its own ii-V.
        { "Champs-Elysees (I-III7-vim-I7)",
          {
              {0, ChordQuality::Major},
              { Alts {
                  Alt {4, ChordQuality::Dominant7, 11},
                  Alt {0, ChordQuality::Augmented},
                  Alt {4, ChordQuality::Augmented}
              } },
              {9, ChordQuality::Minor},
              { Alts {
                  Alt {0, ChordQuality::Dominant7, 7},
                  Alt { Specs {
                      {7, ChordQuality::Minor7},
                      {0, ChordQuality::Dominant7}
                  } }
              } }
          }
        }
    };
    return presets;
}

void ProgressionPresets::applyToModel (const PresetProgression& preset, ProgressionModel& model)
{
    ProgressionModel::ScopedBulkEdit bulk (model);

    const auto& key = model.getKey();
    const int tonic = key.isMajor ? key.tonicPitchClass : (key.tonicPitchClass + 3) % 12;

    model.setNumBars (static_cast<int> (preset.entries.size()));
    model.clear();

    for (size_t i = 0; i < preset.entries.size(); ++i)
    {
        const auto& chords = preset.entries [i].canonical().chords;
        const int bar = static_cast<int> (i);

        // A bar carrying two chords is written as a half-bar split; presets never
        // use more than that, so anything else stays a single whole-bar chord.
        model.setSubdivision (bar, chords.size() == 2 ? BarSubdivision::Two
                                                     : BarSubdivision::One);

        for (size_t s = 0; s < chords.size() && s < 2; ++s)
        {
            const auto& spec = chords [s];
            const int root = (tonic + spec.degreeSemitone) % 12;
            const int bass = spec.bassDegree >= 0 ? (tonic + spec.bassDegree) % 12 : -1;

            model.setChord (bar, static_cast<int> (s), Chord {root, spec.quality, bass});
        }
    }
}

} // namespace reharm
