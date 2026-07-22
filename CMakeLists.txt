cmake_minimum_required(VERSION 3.15)

project(ReharmStudio VERSION 17.13.0)

# ============================================================================
# Build-location policy: `build/` is the ONLY canonical build directory.
# Release verification must run there (`make release-check`). Any other binary
# dir (.agent-builds/<name> via the agent-* presets, or ad-hoc dirs) is an
# agent sandbox: its test results never count as release verification, and the
# test binary brands its own output accordingly so logs cannot be mistaken.
# ============================================================================
if(CMAKE_BINARY_DIR STREQUAL "${CMAKE_SOURCE_DIR}/build")
    set(REHARM_SANDBOX_BUILD FALSE)
else()
    set(REHARM_SANDBOX_BUILD TRUE)
    message(STATUS "AGENT SANDBOX BUILD (${CMAKE_BINARY_DIR}) -- "
                   "test results here do NOT count as release verification; "
                   "final checks: `make release-check` (runs in build/).")
endif()

# Build type: default to Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 17)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.14
    GIT_SHALLOW TRUE
    GIT_PROGRESS TRUE
)
FetchContent_MakeAvailable(JUCE)

# Common compile definitions shared by all targets.
function(reharm_apply_common_definitions target)
    target_compile_definitions(${target} PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_PLUGINHOST_VST3=1
        $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:REHARM_DEBUG_LOG=1>
    )
endfunction()

# ============================================================================
# Main App (always built)
# ============================================================================
juce_add_gui_app(ReharmStudio
    PRODUCT_NAME "Reharm Studio"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/AppIcon_1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/AppIcon_64.png"
)

juce_generate_juce_header(ReharmStudio)

target_sources(ReharmStudio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/AudioEngine.cpp
    Source/ChordModel.cpp
    Source/ProgressionModel.cpp
    Source/HarmonyAnalyzer.cpp
    Source/Theme.cpp
    Source/HeaderBar.cpp
    Source/SequencerView.cpp
    Source/AnalysisStrip.cpp
    Source/ChordEditorPanel.cpp
    Source/TransportBar.cpp
)

reharm_apply_common_definitions(ReharmStudio)

target_link_libraries(ReharmStudio PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
    juce::juce_gui_extra
)

# ============================================================================
# Tests (enable with BUILD_TESTS=ON)
# ============================================================================
option(BUILD_TESTS "Build the test suite" OFF)

if(BUILD_TESTS)
juce_add_console_app(ReharmStudioTests
    PRODUCT_NAME "Reharm Studio Tests"
)

juce_generate_juce_header(ReharmStudioTests)

target_sources(ReharmStudioTests PRIVATE
    Tests/TestRunner.cpp
    Tests/TimingTest.cpp
    Tests/AudioEngineTest.cpp
    Tests/ChordModelTest.cpp
    Tests/ProgressionModelTest.cpp
    Tests/HarmonyAnalyzerTest.cpp
    Source/AudioEngine.cpp
    Source/ChordModel.cpp
    Source/ProgressionModel.cpp
    Source/HarmonyAnalyzer.cpp
)

reharm_apply_common_definitions(ReharmStudioTests)

if(REHARM_SANDBOX_BUILD)
    target_compile_definitions(ReharmStudioTests PRIVATE REHARM_AGENT_SANDBOX=1)
endif()

target_link_libraries(ReharmStudioTests PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_audio_devices
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)
endif()

# ============================================================================
# Experiments (enable with BUILD_EXPERIMENTS=ON)
# ============================================================================
option(BUILD_EXPERIMENTS "Build experiment/proof-of-concept programs" OFF)

if(BUILD_EXPERIMENTS)
juce_add_gui_app(PluginLoadTest
    PRODUCT_NAME "Plugin Load Test"
)

juce_generate_juce_header(PluginLoadTest)

target_sources(PluginLoadTest PRIVATE
    Experiments/PluginLoadTest.cpp
)

reharm_apply_common_definitions(PluginLoadTest)

target_link_libraries(PluginLoadTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

juce_add_gui_app(PresetSwitchTest
    PRODUCT_NAME "Preset Switch Test"
)

juce_generate_juce_header(PresetSwitchTest)

target_sources(PresetSwitchTest PRIVATE
    Experiments/PresetSwitchTest.cpp
)

reharm_apply_common_definitions(PresetSwitchTest)
target_compile_definitions(PresetSwitchTest PRIVATE
    JUCE_MODAL_LOOPS_PERMITTED=1
)

target_link_libraries(PresetSwitchTest PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_plugin_client
    juce::juce_audio_processors
    juce::juce_core
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)
endif()
