#include <JuceHeader.h>
#include <atomic>
#include <cmath>
#include <iostream>
#include <memory>
#include <string>

// ---------------------------------------------------------------------------
// Globals shared with the audio callback
// ---------------------------------------------------------------------------
static std::atomic<bool> gProcess{false};
static juce::AudioPluginInstance* gPlugin = nullptr;

// Optional playhead (set only when --playhead is passed)
class TestPlayHead : public juce::AudioPlayHead
{
public:
    juce::Optional<juce::AudioPlayHead::PositionInfo> getPosition() const override
    {
        juce::AudioPlayHead::PositionInfo info;
        const auto samples = sampleCount.load();
        const auto sr = sampleRate.load();
        const double secs = sr > 0 ? samples / sr : 0.0;
        const double bpm = 120.0;
        const double ppq = secs * (bpm / 60.0);
        info.setBpm (bpm);
        info.setTimeSignature (juce::AudioPlayHead::TimeSignature{4, 4});
        info.setTimeInSamples ((juce::int64) samples);
        info.setTimeInSeconds (secs);
        info.setPpqPosition (ppq);
        info.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        info.setIsPlaying (true);
        info.setIsRecording (false);
        return info;
    }

    std::atomic<double> sampleCount{0};
    std::atomic<double> sampleRate{44100.0};
};

static TestPlayHead* gPlayHead = nullptr; // non-owning; only set when --playhead

// ---------------------------------------------------------------------------
// Audio callback — mirrors AudioEngine flaws intentionally (alloc on RT thread)
// ---------------------------------------------------------------------------
class PresetSwitchAudioCallback : public juce::AudioIODeviceCallback
{
public:
    void audioDeviceAboutToStart (juce::AudioIODevice*) override {}
    void audioDeviceStopped() override {}

    void audioDeviceIOCallbackWithContext (const float* const* /*inputChannelData*/,
                                           int /*numInputChannels*/,
                                           float* const* outputChannelData,
                                           int numOutputChannels,
                                           int numSamples,
                                           const juce::AudioIODeviceCallbackContext&) override
    {
        // Clear device output first
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] != nullptr)
                juce::FloatVectorOperations::clear (outputChannelData[ch], numSamples);
        }

        if (! gProcess.load() || gPlugin == nullptr)
            return;

        // INTENTIONAL: allocate temp buffer on the audio thread (replicates app)
        const int numCh = juce::jmax (numOutputChannels, gPlugin->getTotalNumOutputChannels());
        juce::AudioBuffer<float> tempBuffer (numCh, numSamples);
        tempBuffer.clear();

        juce::MidiBuffer midi;

        // Every ~2 seconds, alternate note-on / note-off for C-E-G (vel 100)
        samplesUntilToggle -= numSamples;
        if (samplesUntilToggle <= 0)
        {
            const double sr = gPlugin->getSampleRate() > 0 ? gPlugin->getSampleRate() : 44100.0;
            samplesUntilToggle = (int) (2.0 * sr);

            notesOn = ! notesOn;
            static const int notes[] = {60, 64, 67};

            if (notesOn)
            {
                for (int n : notes)
                    midi.addEvent (juce::MidiMessage::noteOn (1, n, (juce::uint8) 100), 0);
            }
            else
            {
                for (int n : notes)
                    midi.addEvent (juce::MidiMessage::noteOff (1, n), 0);
            }
        }

        gPlugin->processBlock (tempBuffer, midi);

        // Copy to device with gain 0.2
        constexpr float gain = 0.2f;
        for (int ch = 0; ch < numOutputChannels; ++ch)
        {
            if (outputChannelData[ch] == nullptr)
                continue;

            const float* src = tempBuffer.getReadPointer (ch);
            float* dst = outputChannelData[ch];
            for (int i = 0; i < numSamples; ++i)
                dst[i] = src[i] * gain;
        }

        if (gPlayHead != nullptr)
            gPlayHead->sampleCount.store (gPlayHead->sampleCount.load() + (double) numSamples);
    }

private:
    int samplesUntilToggle = 0;
    bool notesOn = false;
};

// ---------------------------------------------------------------------------
// Message-loop pump — keeps plugin editor / timers alive
// ---------------------------------------------------------------------------
static void pump (double seconds)
{
    const auto start = juce::Time::getMillisecondCounterHiRes();
    const double targetMs = seconds * 1000.0;

    while ((juce::Time::getMillisecondCounterHiRes() - start) < targetMs)
    {
        if (auto* mm = juce::MessageManager::getInstance())
            mm->runDispatchLoopUntil (50);
        else
            juce::Thread::sleep (50);
    }
}

// ---------------------------------------------------------------------------
// CLI parsing
// ---------------------------------------------------------------------------
struct Options
{
    juce::String pluginPath = "/Library/Audio/Plug-Ins/VST3/VPS Avenger.vst3";
    bool playhead = false;
    int programs = 10;
    double hold = 3.0;
    bool noEditor = false;
};

static Options parseArgs (int argc, char* argv[])
{
    Options opts;

    for (int i = 1; i < argc; ++i)
    {
        const juce::String arg (argv[i]);

        if (arg == "--playhead")
        {
            opts.playhead = true;
        }
        else if (arg == "--no-editor")
        {
            opts.noEditor = true;
        }
        else if (arg == "--programs" && i + 1 < argc)
        {
            opts.programs = juce::jmax (0, juce::String (argv[++i]).getIntValue());
        }
        else if (arg == "--hold" && i + 1 < argc)
        {
            opts.hold = juce::jmax (0.0, juce::String (argv[++i]).getDoubleValue());
        }
        else if (! arg.startsWith ("--"))
        {
            opts.pluginPath = arg;
        }
        else
        {
            std::cout << "[PST] Unknown argument: " << arg << std::flush << std::endl;
        }
    }

    return opts;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main (int argc, char* argv[])
{
    const Options opts = parseArgs (argc, argv);

    std::cout << "[PST] start pluginPath=" << opts.pluginPath
              << " playhead=" << (opts.playhead ? "yes" : "no")
              << " programs=" << opts.programs
              << " hold=" << opts.hold
              << " noEditor=" << (opts.noEditor ? "yes" : "no")
              << std::flush << std::endl;

    std::cout << "[PST] ScopedJuceInitialiser_GUI" << std::flush << std::endl;
    juce::ScopedJuceInitialiser_GUI guiInitializer;

    std::cout << "[PST] AudioDeviceManager initialise(0, 2)" << std::flush << std::endl;
    juce::AudioDeviceManager deviceManager;
    deviceManager.initialise (0, 2, nullptr, true);

    auto setup = deviceManager.getAudioDeviceSetup();
    const double sampleRate = setup.sampleRate > 0 ? setup.sampleRate : 44100.0;
    const int bufferSize = setup.bufferSize > 0 ? setup.bufferSize : 512;
    std::cout << "[PST] device SR=" << sampleRate << " BS=" << bufferSize << std::flush << std::endl;

    // --- Scan plugin ---
    std::cout << "[PST] findAllTypesForFile" << std::flush << std::endl;
    juce::VST3PluginFormat format;
    juce::OwnedArray<juce::PluginDescription> descriptions;
    format.findAllTypesForFile (descriptions, opts.pluginPath);

    if (descriptions.isEmpty())
    {
        std::cout << "[PST] ERROR: no plugin descriptions found for: "
                  << opts.pluginPath << std::flush << std::endl;
        return 1;
    }

    const auto desc = *descriptions[0];
    std::cout << "[PST] found plugin name=" << desc.name
              << " manufacturer=" << desc.manufacturerName
              << std::flush << std::endl;

    // --- Create instance ---
    std::cout << "[PST] createInstanceFromDescription SR=" << sampleRate
              << " BS=" << bufferSize << std::flush << std::endl;
    juce::String error;
    auto plugin = format.createInstanceFromDescription (desc, sampleRate, bufferSize, error);

    if (plugin == nullptr)
    {
        std::cout << "[PST] ERROR: createInstanceFromDescription failed: "
                  << error << std::flush << std::endl;
        return 1;
    }

    std::cout << "[PST] createInstanceFromDescription OK"
              << " in=" << plugin->getTotalNumInputChannels()
              << " out=" << plugin->getTotalNumOutputChannels()
              << std::flush << std::endl;

    // --- Optional playhead (BEFORE prepareToPlay) ---
    TestPlayHead testPlayHead;
    testPlayHead.sampleRate.store (sampleRate);
    testPlayHead.sampleCount.store (0.0);

    if (opts.playhead)
    {
        std::cout << "[PST] setPlayHead" << std::flush << std::endl;
        plugin->setPlayHead (&testPlayHead);
        gPlayHead = &testPlayHead;
    }

    // --- prepareToPlay ---
    std::cout << "[PST] prepareToPlay" << std::flush << std::endl;
    plugin->prepareToPlay (sampleRate, bufferSize);
    std::cout << "[PST] prepareToPlay OK" << std::flush << std::endl;

    // --- Audio callback ---
    gPlugin = plugin.get();
    PresetSwitchAudioCallback audioCallback;

    std::cout << "[PST] addAudioCallback" << std::flush << std::endl;
    deviceManager.addAudioCallback (&audioCallback);
    gProcess.store (true);
    std::cout << "[PST] gProcess=true" << std::flush << std::endl;

    // --- Editor window ---
    std::unique_ptr<juce::DocumentWindow> editorWindow;

    if (! opts.noEditor)
    {
        std::cout << "[PST] createEditorIfNeeded" << std::flush << std::endl;
        auto* editor = plugin->createEditorIfNeeded();

        if (editor != nullptr)
        {
            std::cout << "[PST] show DocumentWindow" << std::flush << std::endl;
            editorWindow = std::make_unique<juce::DocumentWindow> (
                "PresetSwitchTest - " + plugin->getName(),
                juce::Colours::darkgrey,
                juce::DocumentWindow::closeButton);

            editorWindow->setUsingNativeTitleBar (true);
            editorWindow->setContentOwned (editor, true);
            editorWindow->centreWithSize (editor->getWidth(), editor->getHeight());
            editorWindow->setVisible (true);
            std::cout << "[PST] editor visible" << std::flush << std::endl;
        }
        else
        {
            std::cout << "[PST] createEditorIfNeeded returned null (no editor)" << std::flush << std::endl;
        }
    }
    else
    {
        std::cout << "[PST] --no-editor: skipping editor" << std::flush << std::endl;
    }

    // --- Initial hold ---
    std::cout << "[PST] initial pump hold=" << opts.hold << std::flush << std::endl;
    pump (opts.hold);

    const int numPrograms = plugin->getNumPrograms();
    const int currentProgram = plugin->getCurrentProgram();
    std::cout << "[PST] numPrograms=" << numPrograms
              << " currentProgram=" << currentProgram
              << std::flush << std::endl;

    // --- Program cycling ---
    const int n = juce::jmin (opts.programs, numPrograms);
    std::cout << "[PST] cycling programs 0.." << (n - 1) << std::flush << std::endl;

    for (int i = 0; i < n; ++i)
    {
        std::cout << "[PST] setCurrentProgram " << i << " BEGIN" << std::flush << std::endl;
        plugin->setCurrentProgram (i);
        pump (opts.hold);
        std::cout << "[PST] setCurrentProgram " << i << " OK name="
                  << plugin->getProgramName (i) << std::flush << std::endl;
    }

    // --- State round-trip ---
    std::cout << "[PST] getStateInformation BEGIN" << std::flush << std::endl;
    juce::MemoryBlock state;
    plugin->getStateInformation (state);
    std::cout << "[PST] getStateInformation OK size=" << state.getSize() << std::flush << std::endl;

    pump (1.0);

    std::cout << "[PST] setStateInformation BEGIN" << std::flush << std::endl;
    plugin->setStateInformation (state.getData(), (int) state.getSize());
    std::cout << "[PST] setStateInformation OK" << std::flush << std::endl;
    pump (opts.hold);

    // --- Shutdown ---
    std::cout << "[PST] shutdown: gProcess=false" << std::flush << std::endl;
    gProcess.store (false);

    std::cout << "[PST] removeAudioCallback" << std::flush << std::endl;
    deviceManager.removeAudioCallback (&audioCallback);
    gPlugin = nullptr;
    gPlayHead = nullptr;

    pump (0.5);

    std::cout << "[PST] destroy editor window" << std::flush << std::endl;
    editorWindow.reset();
    pump (0.5);

    std::cout << "[PST] plugin.reset()" << std::flush << std::endl;
    plugin.reset();

    std::cout << "[PST] closeAudioDevice" << std::flush << std::endl;
    deviceManager.closeAudioDevice();

    std::cout << "[PST] ALL DONE" << std::flush << std::endl;
    return 0;
}
