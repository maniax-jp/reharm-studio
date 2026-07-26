#include <JuceHeader.h>
#include "../Source/RecentPlugins.h"

using namespace reharm;

namespace
{
juce::StringArray pathsOf (const RecentPluginList& list)
{
    juce::StringArray paths;
    for (const auto& r : list)
        paths.add (r.path);
    return paths;
}
} // namespace

class RecentPluginsTest : public juce::UnitTest
{
public:
    RecentPluginsTest() : juce::UnitTest ("RecentPlugins") {}

    void runTest() override
    {
        beginTest ("newest entry goes to the front");
        {
            RecentPluginList list;
            list = withRecentPlugin (list, "/a.vst3", "A");
            list = withRecentPlugin (list, "/b.vst3", "B");

            expect (pathsOf (list) == juce::StringArray { "/b.vst3", "/a.vst3" });
        }

        beginTest ("re-loading a listed plugin moves it up instead of repeating it");
        {
            RecentPluginList list;
            list = withRecentPlugin (list, "/a.vst3", "A");
            list = withRecentPlugin (list, "/b.vst3", "B");
            list = withRecentPlugin (list, "/c.vst3", "C");
            list = withRecentPlugin (list, "/a.vst3", "A");

            expect (pathsOf (list) == juce::StringArray { "/a.vst3", "/c.vst3", "/b.vst3" });
            expectEquals ((int) list.size(), 3);
        }

        beginTest ("the stored name follows the path");
        {
            RecentPluginList list;
            list = withRecentPlugin (list, "/a.vst3", "Old Name");
            list = withRecentPlugin (list, "/a.vst3", "New Name");

            expectEquals ((int) list.size(), 1);
            expect (list.front().name == "New Name");
        }

        beginTest ("the list is capped at maxRecentPlugins, dropping the oldest");
        {
            RecentPluginList list;
            for (int i = 1; i <= maxRecentPlugins + 2; ++i)
                list = withRecentPlugin (list, "/p" + juce::String (i) + ".vst3", "P" + juce::String (i));

            expectEquals ((int) list.size(), maxRecentPlugins);
            expect (list.front().path == "/p7.vst3");
            expect (list.back().path == "/p3.vst3");   // p1 and p2 fell off the end
        }

        beginTest ("an empty path is ignored");
        {
            RecentPluginList list;
            list = withRecentPlugin (list, "/a.vst3", "A");
            list = withRecentPlugin (list, "", "Nameless");

            expectEquals ((int) list.size(), 1);
            expect (list.front().path == "/a.vst3");
        }
    }
};

static RecentPluginsTest recentPluginsTest;
