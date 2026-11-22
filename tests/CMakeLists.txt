include(FetchContent)

FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.16.1
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(Catch2)

list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(Catch)

# ---------------------------------------------------------------------------
# DSP tests (pure C++, no JUCE)
# ---------------------------------------------------------------------------
file(GLOB OE_DSP_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/dsp/*.cpp")
add_executable(DspTests TestHelpers.h ${OE_DSP_TEST_SOURCES})
target_link_libraries(DspTests PRIVATE OrbitEchoDSP Catch2::Catch2WithMain)
oe_set_strict_warnings(DspTests)
catch_discover_tests(DspTests DISCOVERY_MODE PRE_TEST)

# ---------------------------------------------------------------------------
# Plugin-level and host-simulation tests (link the plugin's shared code)
# ---------------------------------------------------------------------------
file(GLOB OE_PLUGIN_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/*.cpp")
add_executable(PluginTests ${OE_PLUGIN_TEST_SOURCES})

# The plugin's shared-code static library already contains the compiled JUCE
# modules; the tests only need the module headers on the include path.
target_include_directories(PluginTests PRIVATE ${JUCE_SOURCE_DIR}/modules ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(PluginTests PRIVATE
    OrbitEcho
    Catch2::Catch2WithMain
    juce::juce_recommended_config_flags)
oe_set_plugin_warnings(PluginTests)
catch_discover_tests(PluginTests DISCOVERY_MODE PRE_TEST)
