cmake_minimum_required(VERSION 3.28)
project(RytmFilter VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
# Windows: statische Laufzeit -> kein VC-Redistributable noetig
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(FetchContent)

# JUCE: lokaler Checkout per -DJUCE_DIR=<pfad>, sonst automatisch von GitHub
set(JUCE_DIR "" CACHE PATH "Optional: Pfad zu einem lokalen JUCE-Checkout")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.12
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# CLAP-Unterstuetzung fuer JUCE 8
FetchContent_Declare(clap-juce-extensions
    GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
    GIT_TAG        3837f4a6ed2ab0ba46aba69ae5b7041c7c64ce74
    GIT_SUBMODULES_RECURSE TRUE
    EXCLUDE_FROM_ALL)
FetchContent_MakeAvailable(clap-juce-extensions)

option(RYTMFILTER_COPY_AFTER_BUILD "Plugins nach dem Build in den User-Plugin-Ordner kopieren" ON)

set(RYTMFILTER_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RYTMFILTER_FORMATS AU)
endif()

option(RYTMFILTER_BUILD_TESTS "DSP-Tests bauen" ON)
option(RYTMFILTER_BUILD_SNAPSHOT "UI-Snapshot-Tool bauen (rendert den Editor als PNG)" OFF)

set(RYTMFILTER_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/MainPanel.cpp
    Source/DotDisplay.cpp)

juce_add_plugin(RytmFilter
    COMPANY_NAME                  "RytmFilter"
    PRODUCT_NAME                  "RytmFilter"
    PLUGIN_MANUFACTURER_CODE      Rytf
    PLUGIN_CODE                   Rtf1
    FORMATS                       ${RYTMFILTER_FORMATS}
    IS_SYNTH                      FALSE
    NEEDS_MIDI_INPUT              TRUE
    NEEDS_MIDI_OUTPUT             FALSE
    IS_MIDI_EFFECT                FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS   FALSE
    VST3_CATEGORIES               Fx Filter
    AU_MAIN_TYPE                  kAudioUnitType_MusicEffect
    COPY_PLUGIN_AFTER_BUILD       ${RYTMFILTER_COPY_AFTER_BUILD})

target_sources(RytmFilter PRIVATE ${RYTMFILTER_SOURCES})

clap_juce_extensions_plugin(TARGET RytmFilter
    CLAP_ID       "io.github.sscheppert-blip.rytmfilter"
    CLAP_FEATURES audio-effect filter stereo mono)

target_compile_definitions(RytmFilter PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RytmFilter
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(RYTMFILTER_BUILD_TESTS)
    juce_add_console_app(FilterTest PRODUCT_NAME "FilterTest")
    target_sources(FilterTest PRIVATE Tests/FilterTest.cpp)
    target_include_directories(FilterTest PRIVATE Source)
    target_compile_definitions(FilterTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(FilterTest
        PRIVATE juce::juce_core juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
endif()

if(RYTMFILTER_BUILD_SNAPSHOT)
    juce_add_console_app(UiSnapshot PRODUCT_NAME "UiSnapshot")
    target_sources(UiSnapshot PRIVATE Tests/UiSnapshot.cpp ${RYTMFILTER_SOURCES})
    target_include_directories(UiSnapshot PRIVATE Source)
    target_compile_definitions(UiSnapshot PRIVATE
        JucePlugin_Name="RytmFilter" JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(UiSnapshot
        PRIVATE juce::juce_audio_utils juce::juce_dsp
        PUBLIC juce::juce_recommended_config_flags)
endif()
