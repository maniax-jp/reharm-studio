cmake_minimum_required(VERSION 3.15)

project(ReharmStudio VERSION 1.0.0)

# Build type: default to Debug
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

set(CMAKE_CXX_STANDARD 17)

add_subdirectory(JUCE)

# ============================================================================
# Main App (always built)
# ============================================================================
juce_add_gui_app(ReharmStudio
    PRODUCT_NAME "Reharm Studio"
)

juce_generate_juce_header(ReharmStudio)

target_sources(ReharmStudio PRIVATE
    Source/Main.cpp
    Source/MainComponent.cpp
    Source/ChordProgressionGenerator.h
    Source/ChordProgressionGenerator.cpp
    Source/AudioEngine.cpp
    Source/ChordTheory.cpp
)

target_compile_definitions(ReharmStudio PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
)

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
    Tests/ChordProgressionTest.cpp
    Tests/ChordTheoryTest.cpp
    Tests/AudioEngineTest.cpp
    Source/MainComponent.cpp
    Source/ChordProgressionGenerator.cpp
    Source/ChordTheory.cpp
    Source/AudioEngine.cpp
)

target_compile_definitions(ReharmStudioTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
)

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

target_compile_definitions(PluginLoadTest PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST3=1
)

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
endif()
