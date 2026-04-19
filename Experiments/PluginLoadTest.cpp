#include <JuceHeader.h>

static std::atomic<bool> gLoadingPlugin{false};
static juce::AudioPluginInstance* gTestPlugin = nullptr;
static std::atomic<bool> gShouldRun{false};

class PluginAudioCallback : public juce::AudioIODeviceCallback
{
public:
    PluginAudioCallback (juce::AudioDeviceManager& mgr) : deviceManager (mgr) {}

    void audioDeviceAboutToStart (juce::AudioIODevice* device) override {}
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const* inputChannelData,
                                           int numInputChannels,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext& context) override
    {
        // Clear all output
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                memset (outputChannelData[ch], 0, numSamples * sizeof(float));
        }

        // Block during plugin creation
        if (std::atomic_load(&gLoadingPlugin))
            return;

        // Only process when signaled
        if (!std::atomic_load(&gShouldRun))
            return;

        if (gTestPlugin != nullptr)
        {
            // Route plugin channels to device channels
            int numPluginOut = gTestPlugin->getTotalNumOutputChannels();
            int numCh = jmax (numOutputChannels, numPluginOut);
            juce::AudioBuffer<float> tempBuffer (numCh, numSamples);
            tempBuffer.clear();
            juce::MidiBuffer midi;

            gTestPlugin->processBlock (tempBuffer, midi);

            // Copy first N channels to device output
            for (int ch = 0; ch < numOutputChannels; ++ch)
            {
                if (outputChannelData[ch] != nullptr)
                    memcpy (outputChannelData[ch], tempBuffer.getReadPointer (ch), numSamples * sizeof(float));
            }
        }
    }

private:
    juce::AudioDeviceManager& deviceManager;
};

static void runTest (juce::AudioDeviceManager& deviceManager, int numInputCh, int numOutputCh, const char* label)
{
    std::atomic_store(&gLoadingPlugin, false);
    std::atomic_store(&gShouldRun, false);
    gTestPlugin = nullptr;

    deviceManager.closeAudioDevice();
    deviceManager.initialise (numInputCh, numOutputCh, nullptr, true);

    if (deviceManager.getCurrentAudioDevice() == nullptr)
    {
        std::cout << "  [" << label << "] Failed to open audio device with " << numInputCh << "in/" << numOutputCh << "out" << std::endl;
        return;
    }

    // Check actual device configuration
    auto* device = deviceManager.getCurrentAudioDevice();
    int actualIn = 0, actualOut = 0;
    juce::String deviceName = "none";
    if (device != nullptr)
    {
        deviceName = device->getName();
        actualIn = device->getInputChannelNames().size();
        actualOut = device->getOutputChannelNames().size();
    }
    std::cout << "  [" << label << "] Actual device: " << deviceName << " — "
              << actualIn << " in / " << actualOut << " out" << std::endl;

    juce::VST3PluginFormat format;
    auto path = "/Library/Audio/Plug-Ins/VST3/VPS Avenger.vst3";

    PluginAudioCallback audioCallback (deviceManager);
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, juce::String (path));

    if (descriptions.isEmpty())
    {
        std::cout << "  [" << label << "] No plugin descriptions found." << std::endl;
        return;
    }

    auto desc = *descriptions[0];

    // Print plugin I/O configuration
    std::cout << "  [" << label << "] Plugin: " << desc.name << std::endl;
    std::cout << "  [" << label << "] Requested: " << numInputCh << " in / " << numOutputCh << " out" << std::endl;

    // Query actual plugin I/O setup
    juce::String error;
    std::atomic_store(&gLoadingPlugin, true);
    auto testSetup = deviceManager.getAudioDeviceSetup();
    auto probePlugin = format.createInstanceFromDescription (desc, testSetup.sampleRate, testSetup.bufferSize, error);

    if (probePlugin != nullptr)
    {
        // Print plugin's actual channel configuration
        int numIn = probePlugin->getTotalNumInputChannels();
        int numOut = probePlugin->getTotalNumOutputChannels();
        std::cout << "  [" << label << "] Plugin inputs: " << numIn << std::endl;
        std::cout << "  [" << label << "] Plugin outputs: " << numOut << std::endl;

        probePlugin.reset();
    }
    else
    {
        std::cout << "  [" << label << "] Probe failed: " << error << std::endl;
    }

    std::atomic_store(&gLoadingPlugin, false);

    // Now do the real load with callback running
    std::cout << "  [" << label << "] Starting with callback running..." << std::endl;
    deviceManager.addAudioCallback (&audioCallback);
    juce::Thread::sleep (100);

    std::atomic_store(&gLoadingPlugin, true);
    error = "";
    auto realPluginInstance = format.createInstanceFromDescription (desc, testSetup.sampleRate, testSetup.bufferSize, error);
    std::atomic_store(&gLoadingPlugin, false);

    if (realPluginInstance != nullptr)
    {
        std::cout << "  [" << label << "] createInstanceFromDescription succeeded." << std::endl;
        gTestPlugin = realPluginInstance.get();
        realPluginInstance->prepareToPlay (testSetup.sampleRate, testSetup.bufferSize);
        std::cout << "  [" << label << "] prepareToPlay succeeded." << std::endl;

        // Now enable callback processing
        std::atomic_store(&gShouldRun, true);
        std::cout << "  [" << label << "] Processing audio for 3 seconds..." << std::endl;
        juce::Thread::sleep (3000);

        std::atomic_store(&gShouldRun, false);
        deviceManager.removeAudioCallback (&audioCallback);
        gTestPlugin = nullptr;
        realPluginInstance.reset();
        std::cout << "  [" << label << "] PASSED" << std::endl;
    }
    else
    {
        std::cout << "  [" << label << "] FAILED: " << error << std::endl;
    }
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI guiInitializer;

    juce::AudioDeviceManager deviceManager;
    deviceManager.initialise (0, 2, nullptr, true);

    std::cout << "========================================" << std::endl;
    std::cout << "VPS Avenger: Mono/Stereo Channel Test" << std::endl;
    std::cout << "========================================" << std::endl;

    // Test 1: Device 0in/1out (mono output)
    std::cout << "\n--- Test 1: Device 0in/1out (mono output) ---" << std::endl;
    runTest (deviceManager, 0, 1, "T1-MonoOut");

    // Test 2: Device 0in/2out, plugin stereo (MainComponent pattern)
    std::cout << "\n--- Test 2: Device 0in/2out (stereo output) ---" << std::endl;
    runTest (deviceManager, 0, 2, "T2-StereoOut");

    // Test 3: Device 1in/1out (mono)
    std::cout << "\n--- Test 3: Device 1in/1out (mono) ---" << std::endl;
    runTest (deviceManager, 1, 1, "T3-MonoFull");

    // Test 4: Device 2in/2out (full stereo)
    std::cout << "\n--- Test 4: Device 2in/2out (full stereo) ---" << std::endl;
    runTest (deviceManager, 2, 2, "T4-StereoFull");

    // Test 5: Device 0in/34out (matching plugin output count)
    std::cout << "\n--- Test 5: Device 0in/34out (matching plugin outputs) ---" << std::endl;
    runTest (deviceManager, 0, 34, "T5-Match34");

    deviceManager.closeAudioDevice();

    std::cout << "\n========================================" << std::endl;
    std::cout << "All tests completed." << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
