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

/**
 * A named preset progression, expressed relative to the tonic of a major key.
 *
 * Three levels, from the inside out:
 *   ChordSpec   - one chord: degree + quality (+ an optional bass for playback).
 *   Alternative - one accepted shape of a slot: a run of one or more ChordSpecs,
 *                 or a Wildcard predicate.
 *   Entry       - one bar's slot: the set of shapes accepted there. The first
 *                 alternative is the canonical form and is what applyToModel writes.
 *
 * Slash bases are for playback only: matching ignores the bass everywhere, so
 * any inversion of a slot's chord is accepted. Where a preset genuinely allows a
 * different chord over a given bass (Ochiba's bar 3), that is spelled out as a
 * Wildcard alternative rather than as a constraint on the canonical form.
 */
struct PresetProgression
{
    /** One chord of a preset. */
    struct ChordSpec
    {
        int degreeSemitone = 0;        // semitones above the tonic (0..11)
        ChordQuality quality = ChordQuality::Major;
        int bassDegree = -1;           // playback only; -1 = no slash bass
    };

    /** Predicate alternatives, for slots too loose to enumerate. */
    enum class Wildcard
    {
        None = 0,
        /** Any chord voiced over the given bass degree, except augmented ones. */
        AnyNonAugmentedOverBass
    };

    /** One accepted shape of a slot. */
    struct Alternative
    {
        std::vector<ChordSpec> chords;  // usually one; more for a subdivided bar
        Wildcard wildcard = Wildcard::None;
        int wildcardBassDegree = -1;    // semitones above the tonic, when wildcard is set

        Alternative() = default;

        /** A single chord, the common case. */
        Alternative (int degreeSemitone,
                     ChordQuality quality,
                     int bassDegree = -1)
            : chords { ChordSpec { degreeSemitone, quality, bassDegree } }
        {
        }

        Alternative (std::vector<ChordSpec> chordRun)
            : chords (std::move (chordRun))
        {
        }

        Alternative (Wildcard w, int bassDegree)
            : wildcard (w), wildcardBassDegree (bassDegree)
        {
        }

        /** How many chords this shape consumes when matching. */
        int length() const noexcept
        {
            return wildcard != Wildcard::None ? 1 : static_cast<int> (chords.size());
        }
    };

    /** One bar of a preset: every shape accepted there, canonical form first. */
    struct Entry
    {
        std::vector<Alternative> alternatives;

        Entry() = default;

        /** Lets a plain `{degree, quality}` stand for a single-chord bar. */
        Entry (int degreeSemitone, ChordQuality quality)
            : alternatives { Alternative { degreeSemitone, quality } }
        {
        }

        Entry (std::vector<Alternative> alts)
            : alternatives (std::move (alts))
        {
        }

        /** The canonical form: what applyToModel writes into the model. */
        const Alternative& canonical() const { return alternatives.front(); }
    };

    juce::String name;                 // e.g. Japanese name of the progression
    std::vector<Entry> entries;        // one entry per bar

    /** When true, pattern detection also accepts the progression started from any
        of its bars (its cyclic rotations), because the loop is recognised by the
        cycle rather than by where it happens to begin. The canonical rotation is
        still the only thing applyToModel writes and the picker shows. */
    bool detectRotations = false;
};

/** Built-in famous progressions selectable from the UI. */
class ProgressionPresets
{
public:
    /** All presets in display order:
        Royal Road (Ohdou), Canon, Pop-Punk, Komuro, Marusa, Autumn Leaves (Kareha),
        Fly Me to the Moon, Confirmation, Only One, Ochiba, Champs-Elysees. */
    static const std::vector<PresetProgression>& all();

    /** Replaces the model's content with the preset's canonical form: one bar per
        entry, subdivided to fit the bar's chords, numBars = entries.size(). Key is
        left unchanged; chord roots are transposed to the model's current key tonic. */
    static void applyToModel (const PresetProgression& preset, ProgressionModel& model);

    /** Every form pattern detection should accept for this preset, canonical
        first: just the preset itself, or all of its cyclic rotations when it sets
        detectRotations. Every form keeps the preset's name, so a match is
        reported as the same progression wherever the loop was entered. */
    static std::vector<PresetProgression> detectionForms (const PresetProgression& preset);
};

} // namespace reharm
