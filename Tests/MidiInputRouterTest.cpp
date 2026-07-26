#include <JuceHeader.h>
#include "../Source/MidiInputRouter.h"

using namespace reharm;

/**
 * Only the stored-device resolution is covered here: it is the pure part, and
 * the part that decides whether a saved MIDI input comes back on the next
 * launch. Opening real devices is out of scope for a unit test.
 */
class MidiInputRouterTest : public juce::UnitTest
{
public:
    MidiInputRouterTest() : juce::UnitTest ("MidiInputRouter") {}

    void runTest() override
    {
        const juce::Array<juce::MidiDeviceInfo> devices {
            juce::MidiDeviceInfo ("Keystation 49", "id-keystation"),
            juce::MidiDeviceInfo ("IAC Driver Bus 1", "id-iac")
        };

        beginTest ("identifier match wins");
        {
            expect (MidiInputRouter::resolveDeviceIdentifier (devices, "id-iac", "Keystation 49")
                        == "id-iac");
        }

        beginTest ("falls back to the name when the identifier is gone");
        {
            expect (MidiInputRouter::resolveDeviceIdentifier (devices, "id-stale", "Keystation 49")
                        == "id-keystation");
        }

        beginTest ("name-only entries still resolve");
        {
            expect (MidiInputRouter::resolveDeviceIdentifier (devices, {}, "IAC Driver Bus 1")
                        == "id-iac");
        }

        beginTest ("unknown device resolves to nothing");
        {
            expect (MidiInputRouter::resolveDeviceIdentifier (devices, "id-stale", "Unplugged Keyboard")
                        .isEmpty());
            expect (MidiInputRouter::resolveDeviceIdentifier (devices, {}, {}).isEmpty());
            expect (MidiInputRouter::resolveDeviceIdentifier ({}, "id-keystation", "Keystation 49")
                        .isEmpty());
        }
    }
};

static MidiInputRouterTest midiInputRouterTest;
