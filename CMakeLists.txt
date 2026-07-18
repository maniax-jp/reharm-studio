cmake_minimum_required(VERSION 3.15)

project(ReharmStudio VERSION 17.5.0)

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
    )
    if(CMAKE_BUILD_TYPE STREQUAL "Debug" OR CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
        target_compile_definitions(${target} PRIVATE REHARM_DEBUG_LOG=1)
    endif()
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
    juce::juce_audio_plugin_client
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
juce_add_gui_app(ReharmStudioTests
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

reharm_apply_common_definitions(ReharmStudioTests)

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
