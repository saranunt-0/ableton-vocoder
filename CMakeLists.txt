cmake_minimum_required(VERSION 3.22)

project(VintageVocoder VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(VV_BUILD_PLUGIN "Build the VST3/AU/Standalone plugins (needs JUCE)" ON)
option(VV_BUILD_TESTS "Build the DSP unit tests and demo renderer" ON)
set(VV_JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout. Empty = download JUCE ${VV_JUCE_TAG}.")
set(VV_JUCE_TAG "8.0.9" CACHE STRING "JUCE git tag to download when VV_JUCE_DIR is empty")

# --- DSP core: plain C++, no JUCE, shared by plugins and tests -----------------
add_library(vv_dsp STATIC Source/dsp/VocoderEngine.cpp)
target_include_directories(vv_dsp PUBLIC Source)
set_target_properties(vv_dsp PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(vv_dsp PRIVATE /W4)
else()
    target_compile_options(vv_dsp PRIVATE -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion -Wfloat-equal)
endif()

if(VV_BUILD_TESTS)
    enable_testing()

    add_executable(vv_tests tests/VocoderTests.cpp)
    target_link_libraries(vv_tests PRIVATE vv_dsp)
    add_test(NAME vv_tests COMMAND vv_tests)

    add_executable(vv_render tools/RenderDemo.cpp)
    target_link_libraries(vv_render PRIVATE vv_dsp)
endif()

if(NOT VV_BUILD_PLUGIN)
    return()
endif()

# --- JUCE ----------------------------------------------------------------------
if(VV_JUCE_DIR)
    add_subdirectory(${VV_JUCE_DIR} ${CMAKE_BINARY_DIR}/JUCE EXCLUDE_FROM_ALL)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${VV_JUCE_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(VV_FORMATS VST3)
if(APPLE)
    list(APPEND VV_FORMATS AU)
endif()

# Two builds of the same code, because Ableton routes MIDI + voice differently
# to instruments and to effects (see docs/ableton-setup.md):
#   Vintage Vocoder    - instrument on a MIDI track, voice arrives via Sidechain
#   Vintage Vocoder FX - audio effect on the vocal track, MIDI arrives via "MIDI To"
juce_add_plugin(VintageVocoder
    PRODUCT_NAME "Vintage Vocoder"
    COMPANY_NAME "saranunt"
    BUNDLE_ID "com.saranunt.vintagevocoder"
    PLUGIN_MANUFACTURER_CODE Srnt
    PLUGIN_CODE VVoc
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS ${VV_FORMATS})

juce_add_plugin(VintageVocoderFX
    PRODUCT_NAME "Vintage Vocoder FX"
    COMPANY_NAME "saranunt"
    BUNDLE_ID "com.saranunt.vintagevocoderfx"
    PLUGIN_MANUFACTURER_CODE Srnt
    PLUGIN_CODE VVfx
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_MusicEffect
    FORMATS ${VV_FORMATS} Standalone)

target_compile_definitions(VintageVocoder PUBLIC VV_INSTRUMENT=1)
target_compile_definitions(VintageVocoderFX PUBLIC VV_INSTRUMENT=0)

foreach(plugin VintageVocoder VintageVocoderFX)
    target_sources(${plugin} PRIVATE
        Source/plugin/Parameters.cpp
        Source/plugin/PluginProcessor.cpp
        Source/plugin/PluginEditor.cpp)

    target_compile_definitions(${plugin} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(${plugin}
        PRIVATE
            vv_dsp
            juce::juce_audio_utils
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endforeach()
