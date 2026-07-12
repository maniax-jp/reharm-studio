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
    jassert (barIndex >= 0 && barIndex < maxBars);
    return bars [barIndex];
}

BarSubdivision ProgressionModel::getSubdivision (int barIndex) const
{
    jassert (barIndex >= 0 && barIndex < numBars);
    return bars [barIndex].subdivision;
}

void ProgressionModel::setSubdivision (int barIndex, BarSubdivision newSubdivision)
{
    if (barIndex < 0 || barIndex >= numBars)
        return;

    bars [barIndex].subdivision = newSubdivision;
    notifyChanged();
}

std::optional<Chord> ProgressionModel::getChord (int barIndex, int slotIndex) const
{
    if (barIndex < 0 || barIndex >= numBars)
        return std::nullopt;
    if (slotIndex < 0 || slotIndex >= bars [barIndex].numSlots())
        return std::nullopt;
    return bars [barIndex].slots [slotIndex];
}

void ProgressionModel::setChord (int barIndex, int slotIndex, std::optional<Chord> chord)
{
    if (barIndex < 0 || barIndex >= numBars)
        return;
    if (slotIndex < 0 || slotIndex >= bars [barIndex].numSlots())
        return;

    bars [barIndex].slots [slotIndex] = std::move (chord);
    notifyChanged();
}

void ProgressionModel::clear()
{
    for (int i = 0; i < numBars; ++i)
    {
        bars [i].slots.fill (std::nullopt);
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

std::vector<FlatChord> ProgressionModel::flatten() const
{
    std::vector<FlatChord> result;
    result.reserve (static_cast<size_t> (totalSlots()));

    for (int b = 0; b < numBars; ++b)
    {
        const int numSlots = bars [b].numSlots();
        const double beats = bars [b].beatsPerSlot();

        for (int s = 0; s < numSlots; ++s)
        {
            result.push_back ({bars [b].slots [s], beats, b, s});
        }
    }
    return result;
}

int ProgressionModel::totalSlots() const noexcept
{
    int total = 0;
    for (int i = 0; i < numBars; ++i)
        total += bars [i].numSlots();
    return total;
}

void ProgressionModel::notifyChanged()
{
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
