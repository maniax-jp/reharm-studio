#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace reharm
{

/** A VST3 the user has loaded before: enough to load it again and to label a menu entry. */
struct RecentPlugin
{
    juce::String path;
    juce::String name;

    bool operator== (const RecentPlugin& other) const
    { return path == other.path && name == other.name; }
};

/** Most-recent-first list of previously loaded plugins. */
using RecentPluginList = std::vector<RecentPlugin>;

/** How many entries the menu, and the session file, keep. */
inline constexpr int maxRecentPlugins = 5;

/** Returns list with this plugin at the front. A plugin is identified by its
    path, so re-loading one that is already listed moves it up instead of
    repeating it (and refreshes the stored name). Entries past
    maxRecentPlugins are dropped; an empty path leaves the list untouched. */
RecentPluginList withRecentPlugin (RecentPluginList list,
                                   const juce::String& path,
                                   const juce::String& name);

} // namespace reharm
