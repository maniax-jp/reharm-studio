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
        const auto& e = preset.entries [i];
        const int root = (tonic + e.degreeSemitone) % 12;

        model.setSubdivision (static_cast<int> (i), BarSubdivision::One);
        model.setChord (static_cast<int> (i), 0, Chord {root, e.quality, -1});
    }
}

} // namespace reharm
