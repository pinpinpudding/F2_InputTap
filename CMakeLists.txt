cmake_minimum_required(VERSION 3.22)

# Windows: link the C++ runtime statically so the .vst3 has no extra DLL dependencies
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

project(InputTap VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Use a local JUCE checkout if JUCE_DIR is given, otherwise download JUCE 8.0.9
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.9
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(InputTap
    COMPANY_NAME             "F2"
    PRODUCT_NAME             "F2_InputTap"
    # Plugin codes are unchanged so existing FL Studio projects still find the plugin
    PLUGIN_MANUFACTURER_CODE Itap
    PLUGIN_CODE              Itp1
    FORMATS                  VST3
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         FALSE
    NEEDS_MIDI_OUTPUT        FALSE
    VST3_CATEGORIES          Fx Tools
    COPY_PLUGIN_AFTER_BUILD  FALSE)

target_sources(InputTap PRIVATE
    Source/InputHub.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(InputTap PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_ASIO=0          # ASIO is exclusive; FL Studio usually owns it
    JUCE_WASAPI=1
    JUCE_DIRECTSOUND=0
    JUCE_JACK=0)

target_link_libraries(InputTap
    PRIVATE
        juce::juce_audio_utils
        juce::juce_audio_devices
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
