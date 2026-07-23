#pragma once

#include "ChordModel.h"
#include <array>
#include <functional>
#include <optional>

namespace reharm
{

struct ProgressionState;

/** How many chord slots a bar is divided into. */
enum class BarSubdivision { One = 1, Two = 2, Four = 4 };

/** One bar of the sequencer: 1, 2 or 4 chord slots. Empty slot = rest. */
struct Bar
{
    BarSubdivision subdivision = BarSubdivision::One;
    std::array<std::optional<Chord>, 4> slots;   // only the first numSlots() entries are active

    int numSlots() const noexcept { return static_cast<int> (subdivision); }
    double beatsPerSlot() const noexcept { return 4.0 / numSlots(); }
};

/** A flattened playable entry: chord (or rest) with a duration in beats. */
struct FlatChord
{
    std::optional<Chord> chord;   // nullopt = rest
    double beats = 4.0;
    int barIndex = 0;
    int slotIndex = 0;
};

/**
 * The step-sequencer data model: up to 8 bars in 4/4, each bar split into
 * 1, 2 or 4 chord slots. Not thread-safe; use from the message thread only.
 */
class ProgressionModel
{
public:
    static constexpr int maxBars = 8;

    ProgressionModel();

    int getNumBars() const noexcept;
    void setNumBars (int newNumBars);            // clamped to [1, maxBars]

    const Bar& getBar (int barIndex) const;
    BarSubdivision getSubdivision (int barIndex) const;

    /** Changes a bar's subdivision. Existing chords are preserved positionally:
        growing keeps slot contents (new slots empty); shrinking keeps the
        first numSlots() slots and discards the rest. */
    void setSubdivision (int barIndex, BarSubdivision newSubdivision);

    std::optional<Chord> getChord (int barIndex, int slotIndex) const;
    void setChord (int barIndex, int slotIndex, std::optional<Chord> chord);
    void clear();                                // empties all slots, keeps bar count/subdivisions

    const KeyContext& getKey() const noexcept;
    void setKey (KeyContext newKey);

    /** Value snapshot of the whole progression (all bars + numBars + key). */
    ProgressionState captureState() const;
    /** Apply a snapshot and fire onChanged once. */
    void restoreState (const ProgressionState& state);

    /** Flattens all bars into playable entries (including rests). */
    std::vector<FlatChord> flatten() const;

    /** Total number of active slots across all bars. */
    int totalSlots() const noexcept;

    /** Suppress onChanged notifications until matching endBulkEdit().
        Nested calls are supported; notification fires once when the outermost
        scope ends if any mutation occurred. */
    void beginBulkEdit() noexcept;
    void endBulkEdit();

    /** RAII helper for beginBulkEdit/endBulkEdit. */
    class ScopedBulkEdit
    {
    public:
        explicit ScopedBulkEdit (ProgressionModel& model) noexcept
            : m (model)
        {
            m.beginBulkEdit();
        }

        ~ScopedBulkEdit()
        {
            m.endBulkEdit();
        }

        ScopedBulkEdit (const ScopedBulkEdit&) = delete;
        ScopedBulkEdit& operator= (const ScopedBulkEdit&) = delete;

    private:
        ProgressionModel& m;
    };

    /** Called after any mutation (setChord/setSubdivision/setNumBars/setKey/clear). */
    std::function<void()> onChanged;

private:
    void notifyChanged();

    std::array<Bar, maxBars> bars;
    int numBars = 4;
    KeyContext key;
    int bulkEditDepth = 0;
    bool bulkEditDirty = false;
};

/** Value snapshot of the whole progression (bars + numBars + key). */
struct ProgressionState
{
    std::array<Bar, ProgressionModel::maxBars> bars;
    int numBars = 4;
    KeyContext key;
};

bool operator== (const Bar& a, const Bar& b) noexcept;
bool operator!= (const Bar& a, const Bar& b) noexcept;
bool operator== (const ProgressionState& a, const ProgressionState& b) noexcept;
bool operator!= (const ProgressionState& a, const ProgressionState& b) noexcept;
bool operator== (const KeyContext& a, const KeyContext& b) noexcept;
bool operator!= (const KeyContext& a, const KeyContext& b) noexcept;

/** A named preset progression, expressed relative to the tonic of a major key. */
struct PresetProgression
{
    struct Entry
    {
        int degreeSemitone = 0;        // semitones above the tonic (0..11)
        ChordQuality quality = ChordQuality::Major;
    };

    juce::String name;                 // e.g. Japanese name of the progression
    std::vector<Entry> entries;        // one entry per bar (subdivision One)
};

/** Built-in famous progressions selectable from the UI. */
class ProgressionPresets
{
public:
    /** All presets in display order:
        Royal Road (Ohdou), Canon, Pop-Punk, Komuro, Marusa, Autumn Leaves (Kareha). */
    static const std::vector<PresetProgression>& all();

    /** Replaces the model's content with the preset: one chord per bar,
        subdivision One, numBars = entries.size(). Key is left unchanged;
        chord roots are transposed to the model's current key tonic. */
    static void applyToModel (const PresetProgression& preset, ProgressionModel& model);
};

} // namespace reharm
