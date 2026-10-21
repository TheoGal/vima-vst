cmake_minimum_required(VERSION 3.22)
project(Vima VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(MSVC)
    # static runtime: the .vst3 works without installing the VC++ redistributable
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE: use a local checkout with -DJUCE_DIR=/path/to/JUCE, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} juce)
else()
    include(FetchContent)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.8
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(juce)
endif()

# VIMA is an instrument plugin that outputs MIDI only (notes and CC).
# The silent stereo output exists so hosts accept it as an instrument.
juce_add_plugin(Vima
    PRODUCT_NAME                "Vima"
    COMPANY_NAME                "TheoGal"
    BUNDLE_ID                   "gr.cnnbl.vima"
    PLUGIN_MANUFACTURER_CODE    Thgl
    PLUGIN_CODE                 Vima
    FORMATS                     VST3
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           TRUE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES             Instrument Generator
    COPY_PLUGIN_AFTER_BUILD     FALSE)

juce_add_binary_data(VimaData SOURCES
    Resources/fonts/IBMPlexSans-Regular.ttf
    Resources/fonts/IBMPlexSans-SemiBold.ttf
    Resources/fonts/IBMPlexSans-Bold.ttf
    Resources/fonts/IBMPlexMono-Regular.ttf
    Resources/fonts/IBMPlexMono-SemiBold.ttf)

set(VIMA_SOURCES
    Source/Model.cpp
    Source/Engine.cpp
    Source/Operations.cpp
    Source/Macros.cpp
    Source/MacroParams.cpp
    Source/MacrosView.cpp
    Source/Theme.cpp
    Source/Widgets.cpp
    Source/Popovers.cpp
    Source/Header.cpp
    Source/ParamRow.cpp
    Source/StepGrid.cpp
    Source/OpsPanel.cpp
    Source/PatternsView.cpp
    Source/SettingsDialog.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_sources(Vima PRIVATE ${VIMA_SOURCES})

target_compile_definitions(Vima PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Vima
    PRIVATE
        VimaData
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional test harness: offline engine checks and rendered screenshots of the UI
option(VIMA_BUILD_TESTS "Build the test harness" OFF)
if(VIMA_BUILD_TESTS)
    juce_add_console_app(VimaTests PRODUCT_NAME "VimaTests")
    target_sources(VimaTests PRIVATE tests/Tests.cpp ${VIMA_SOURCES})
    target_compile_definitions(VimaTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Vima" JUCE_STANDALONE_APPLICATION=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(VimaTests PRIVATE VimaData juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
