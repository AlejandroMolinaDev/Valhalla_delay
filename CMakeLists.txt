cmake_minimum_required(VERSION 3.25)

# ---------------------------------------------------------------------------
# Platform settings that must be defined before project()
# ---------------------------------------------------------------------------

# Windows: static MSVC runtime (/MT, /MTd) so the plugin never depends on the
# Visual C++ redistributable being installed on the user's machine.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# macOS: universal binary (Apple Silicon + Intel), minimum macOS 10.13.
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")

include(cmake/Version.cmake)
include(cmake/CompilerWarnings.cmake)

project(OrbitEcho VERSION ${OE_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(OE_BUILD_TESTS "Build the Catch2 test suites" ON)
option(OE_BUILD_TOOLS "Build the offline render tool and the host harness" ON)
option(OE_COPY_AFTER_BUILD "Copy the plugin to the user's plugin folders after building" ON)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

add_subdirectory(JUCE EXCLUDE_FROM_ALL)
add_subdirectory(clap-juce-extensions EXCLUDE_FROM_ALL)

# ---------------------------------------------------------------------------
# DSP core: plain C++20, no JUCE dependency (fast, deterministic unit tests)
# ---------------------------------------------------------------------------
file(GLOB_RECURSE OE_DSP_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/dsp/*.h")

add_library(OrbitEchoDSP STATIC ${OE_DSP_SOURCES})
target_include_directories(OrbitEchoDSP PUBLIC source)
target_compile_features(OrbitEchoDSP PUBLIC cxx_std_20)
oe_set_strict_warnings(OrbitEchoDSP)

# ---------------------------------------------------------------------------
# Embedded resources: fonts (OFL) and factory presets
# ---------------------------------------------------------------------------
file(GLOB OE_FACTORY_PRESETS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/source/presets/*.xml")
file(GLOB OE_FONTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/*.ttf")
set(OE_BINARY_SOURCES ${OE_FONTS} ${OE_FACTORY_PRESETS})

if(OE_BINARY_SOURCES)
    juce_add_binary_data(OrbitEchoData
        HEADER_NAME BinaryData.h
        NAMESPACE BinaryData
        SOURCES ${OE_BINARY_SOURCES})
    set_target_properties(OrbitEchoData PROPERTIES POSITION_INDEPENDENT_CODE ON)
    set(OE_HAS_BINARY_DATA 1)
else()
    set(OE_HAS_BINARY_DATA 0)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
juce_add_plugin(OrbitEcho
    PRODUCT_NAME "Orbit Echo"
    COMPANY_NAME "Alejandro Audio"
    BUNDLE_ID com.alejandroaudio.orbitecho
    VERSION ${OE_VERSION}
    DESCRIPTION "Multimode stereo delay"
    PLUGIN_MANUFACTURER_CODE Alej
    PLUGIN_CODE Orbe
    FORMATS VST3 AU Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay
    AU_MAIN_TYPE kAudioUnitType_Effect
    AU_SANDBOX_SAFE TRUE
    COPY_PLUGIN_AFTER_BUILD ${OE_COPY_AFTER_BUILD})

file(GLOB OE_PLUGIN_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ui/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/ui/*.h")
target_sources(OrbitEcho PRIVATE ${OE_PLUGIN_SOURCES})
target_include_directories(OrbitEcho PUBLIC source)

target_compile_definitions(OrbitEcho PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    OE_HAS_BINARY_DATA=${OE_HAS_BINARY_DATA})

target_link_libraries(OrbitEcho
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_recommended_warning_flags
    PUBLIC
        OrbitEchoDSP
        juce::juce_recommended_config_flags)

if(OE_HAS_BINARY_DATA)
    target_link_libraries(OrbitEcho PUBLIC OrbitEchoData)
endif()

# CLAP (FL Studio 2024+, Bitwig, Reaper) through free-audio/clap-juce-extensions,
# because JUCE 8 has no native CLAP wrapper.
clap_juce_extensions_plugin(TARGET OrbitEcho
    CLAP_ID "com.alejandroaudio.orbitecho"
    CLAP_FEATURES audio-effect delay stereo
    CLAP_USE_JUCE_PARAMETER_RANGES DISCRETE)

# ---------------------------------------------------------------------------
# Tests and tools
# ---------------------------------------------------------------------------
if(OE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(OE_BUILD_TOOLS)
    add_subdirectory(tools)
endif()
