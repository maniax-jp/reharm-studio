#include "RecentPlugins.h"
#include <algorithm>

namespace reharm
{

RecentPluginList withRecentPlugin (RecentPluginList list,
                                   const juce::String& path,
                                   const juce::String& name)
{
    if (path.isEmpty())
        return list;

    list.erase (std::remove_if (list.begin(), list.end(),
                                [&path] (const RecentPlugin& r) { return r.path == path; }),
                list.end());

    list.insert (list.begin(), RecentPlugin { path, name });

    if ((int) list.size() > maxRecentPlugins)
        list.resize ((size_t) maxRecentPlugins);

    return list;
}

} // namespace reharm
