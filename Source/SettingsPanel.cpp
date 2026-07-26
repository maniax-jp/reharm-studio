#include "SettingsPanel.h"
#include "Localization.h"

namespace
{
constexpr int kNoneItemId = 1;
constexpr int kFirstDeviceItemId = 2;
// Placeholder row for a picked device that is no longer connected. Kept out of
// the device id range so it can never collide with a real entry.
constexpr int kDisconnectedItemId = 9000;
} // namespace

SettingsPanel::SettingsPanel()
{
    midiCaption.setText (reharm::loc::tr ("MIDI Input"), juce::dontSendNotification);
    midiCaption.setFont (theme::font (13.0f, true));
    midiCaption.setColour (juce::Label::textColourId, theme::text);
    addAndMakeVisible (midiCaption);

    midiInputBox.onChange = [this] { handleSelectionChanged(); };
    addAndMakeVisible (midiInputBox);

    startTimer (2000);
}

void SettingsPanel::timerCallback()
{
    // Never rebuild under an open popup: it lists a snapshot of the items, so
    // swapping them out from under the user could apply the wrong device.
    if (midiInputBox.isPopupActive())
        return;

    if (onRefreshRequested)
        onRefreshRequested();
}

int SettingsPanel::indexOfIdentifier (const juce::String& identifier) const
{
    if (identifier.isEmpty())
        return -1;

    for (int i = 0; i < midiDevices.size(); ++i)
        if (midiDevices.getReference (i).identifier == identifier)
            return i;

    return -1;
}

void SettingsPanel::setMidiInputs (const juce::Array<juce::MidiDeviceInfo>& devices,
                                   const juce::String& newSelectedIdentifier,
                                   const juce::String& newSelectedName)
{
    // The poll repeats the same list most of the time; rebuilding then would
    // only make the box flicker.
    if (listPopulated
        && devices == midiDevices
        && newSelectedIdentifier == selectedIdentifier
        && newSelectedName == selectedName)
        return;

    midiDevices = devices;
    selectedIdentifier = newSelectedIdentifier;
    selectedName = newSelectedName;
    listPopulated = true;

    midiInputBox.clear (juce::dontSendNotification);
    midiInputBox.addItem (reharm::loc::tr ("No MIDI input"), kNoneItemId);

    for (int i = 0; i < midiDevices.size(); ++i)
        midiInputBox.addItem (midiDevices.getReference (i).name, kFirstDeviceItemId + i);

    const int index = indexOfIdentifier (selectedIdentifier);
    const bool disconnected = (index < 0 && selectedName.isNotEmpty());

    if (disconnected)
    {
        // Not selectable, but still shown: the user picked this device and it is
        // simply unplugged right now.
        midiInputBox.addItem (selectedName, kDisconnectedItemId);
        midiInputBox.setItemEnabled (kDisconnectedItemId, false);
    }

    midiInputBox.setColour (juce::ComboBox::textColourId,
                            disconnected ? theme::textDim : theme::text);

    const int itemId = index >= 0 ? kFirstDeviceItemId + index
                                  : (disconnected ? kDisconnectedItemId : kNoneItemId);
    midiInputBox.setSelectedId (itemId, juce::dontSendNotification);
}

void SettingsPanel::handleSelectionChanged()
{
    const int id = midiInputBox.getSelectedId();
    if (id == kDisconnectedItemId)
        return;

    const int index = id - kFirstDeviceItemId;
    const bool isDevice = (index >= 0 && index < midiDevices.size());

    const juce::String identifier = isDevice ? midiDevices.getReference (index).identifier : juce::String();
    const juce::String name       = isDevice ? midiDevices.getReference (index).name       : juce::String();

    if (identifier == selectedIdentifier)
        return;

    if (onMidiInputSelected)
        onMidiInputSelected (identifier, name);
}

void SettingsPanel::paint (juce::Graphics& g)
{
    g.fillAll (theme::surface);
}

void SettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (20);

    // The dialog ends up a little taller than the requested content size (native
    // title bar), so centre the block instead of letting the slack pile up below.
    const int blockHeight = 22 + 6 + 28;
    area = area.withSizeKeepingCentre (area.getWidth(), juce::jmin (area.getHeight(), blockHeight));

    midiCaption.setBounds (area.removeFromTop (22));
    area.removeFromTop (6);
    midiInputBox.setBounds (area.removeFromTop (28));
}
