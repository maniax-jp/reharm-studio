#include "MidiInputRouter.h"

namespace reharm
{

MidiInputRouter::~MidiInputRouter()
{
    closeDevice();
}

juce::Array<juce::MidiDeviceInfo> MidiInputRouter::availableDevices()
{
    return juce::MidiInput::getAvailableDevices();
}

juce::String MidiInputRouter::resolveDeviceIdentifier (const juce::Array<juce::MidiDeviceInfo>& devices,
                                                       const juce::String& storedIdentifier,
                                                       const juce::String& storedName)
{
    if (storedIdentifier.isNotEmpty())
        for (const auto& d : devices)
            if (d.identifier == storedIdentifier)
                return d.identifier;

    if (storedName.isNotEmpty())
        for (const auto& d : devices)
            if (d.name == storedName)
                return d.identifier;

    return {};
}

void MidiInputRouter::prepare (double sampleRate)
{
    if (sampleRate <= 0.0)
        return;

    collector.reset (sampleRate);
    collectorReady.store (true);
}

void MidiInputRouter::fillNextBlock (juce::MidiBuffer& destination, int numSamples)
{
    if (numSamples <= 0 || ! collectorReady.load())
        return;

    collector.removeNextBlockOfMessages (destination, numSamples);
}

bool MidiInputRouter::setDevice (const juce::String& identifier)
{
    if (input != nullptr && openIdentifier == identifier)
        return true;

    closeDevice();

    if (identifier.isEmpty())
        return false;

    auto opened = juce::MidiInput::openDevice (identifier, this);
    if (opened == nullptr)
        return false;

    openIdentifier = opened->getIdentifier();
    openName = opened->getName();
    input = std::move (opened);
    input->start();
    return true;
}

bool MidiInputRouter::restoreDevice (const juce::String& storedIdentifier,
                                     const juce::String& storedName)
{
    const auto identifier = resolveDeviceIdentifier (availableDevices(), storedIdentifier, storedName);
    if (identifier.isEmpty())
        return false;

    return setDevice (identifier);
}

void MidiInputRouter::closeDevice()
{
    if (input != nullptr)
    {
        input->stop();
        input.reset();
    }

    openIdentifier.clear();
    openName.clear();
}

void MidiInputRouter::handleIncomingMidiMessage (juce::MidiInput*, const juce::MidiMessage& message)
{
    if (collectorReady.load())
        collector.addMessageToQueue (message);
}

} // namespace reharm
