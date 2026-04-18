#include <JuceHeader.h>

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInitializer;

    juce::AudioDeviceManager deviceManager;
    deviceManager.initialise (0, 2, nullptr, true);

    juce::VST3PluginFormat format;
    juce::String pluginsToTest[] = {
        "/Library/Audio/Plug-Ins/VST3/VPS Avenger.vst3",
        "/Library/Audio/Plug-Ins/VST3/Kontakt 7.vst3"
    };

    for (const auto& path : pluginsToTest)
    {
        std::cout << "Testing plugin: " << path << std::endl;
        
        juce::OwnedArray<juce::PluginDescription> descriptions;
        format.findAllTypesForFile (descriptions, path.toStdString());

        if (descriptions.isEmpty())
        {
            std::cout << "  No plugin descriptions found for this file." << std::endl;
            continue;
        }

        juce::String error;
        auto setup = deviceManager.getAudioDeviceSetup();
        auto pluginInstance = format.createInstanceFromDescription (*descriptions[0], setup.sampleRate, setup.bufferSize, error);

        if (pluginInstance != nullptr)
        {
            std::cout << "  Successfully loaded: " << pluginInstance->getName() << std::endl;
            pluginInstance->prepareToPlay (setup.sampleRate, setup.bufferSize);
            std::cout << "  prepareToPlay completed." << std::endl;
            pluginInstance.reset();
            std::cout << "  Plugin instance destroyed." << std::endl;
        }
        else
        {
            std::cout << "  Failed to load plugin: " << error << std::endl;
        }
    }

    return 0;
}
