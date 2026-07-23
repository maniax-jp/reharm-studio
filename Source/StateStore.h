#pragma once

#include <juce_core/juce_core.h>
#include "ProgressionModel.h"

namespace reharm
{

/**
 * Persists application state (progression, key, transport, plugin) as JSON.
 * Handles both the automatic session file and named user presets.
 * Not thread-safe; use from the message thread only.
 */
class StateStore
{
public:
    explicit StateStore (const juce::File& baseDirectory);

    /** Returns an instance pointing at ~/Library/Application Support/Reharm Studio.
        Note: on macOS, JUCE's userApplicationDataDirectory already resolves to
        "~/Library", so the "Application Support" segment must be appended here. */
    static StateStore createDefault();

    struct SessionData
    {
        int bpm = 120;
        float volume = 0.8f;
        bool voicingClose = true;          // true=Close, false=Open
        juce::String pluginPath;           // empty = no plugin
        juce::String pluginName;
        juce::String pluginStateB64;       // Base64; empty = no state
    };

    // ---- Pure serialization (static, unit-test target) ----
    static juce::var serialize (const ProgressionModel& model, const SessionData& s, bool musicalOnly = false);
    /** Applies to model and s when valid, returning true. If the top level is
        malformed, or the bars array is missing/empty, model is left untouched
        and false is returned. Model application is batched into a single
        onChanged notification via ScopedBulkEdit. */
    static bool deserialize (const juce::var& v, ProgressionModel& model, SessionData& s);

    // ---- Session (autosave) ----
    bool saveSession (const ProgressionModel& model, const SessionData& s) const;
    bool loadSession (ProgressionModel& model, SessionData& s) const;  // missing/corrupt -> false

    // ---- User presets ----
    juce::StringArray listUserPresetNames() const;  // names without ".json", sorted lexicographically
    bool userPresetExists (const juce::String& name) const;
    bool saveUserPreset (const juce::String& name, const ProgressionModel& model, const SessionData& s) const;
    bool loadUserPreset (const juce::String& name, ProgressionModel& model, SessionData& s) const;

    /** File::createLegalFileName plus leading/trailing whitespace trim.
        Returns an empty string when the result would be empty. */
    static juce::String sanitizePresetName (const juce::String& raw);

    juce::File baseDir() const;
    juce::File sessionFile() const;      // <base>/session.json
    juce::File presetsDir() const;       // <base>/Presets
private:
    juce::File base;
};

} // namespace reharm
