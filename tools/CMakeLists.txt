# Offline renderer / CPU profiler (pure DSP, no JUCE).
add_executable(oe_render RenderTool.cpp)
target_link_libraries(oe_render PRIVATE OrbitEchoDSP)
oe_set_strict_warnings(oe_render)

# Host harness: loads the BUILT plugin binaries through JUCE's hosting code and
# drives them the way DAWs do (FL Studio patterns first). See HostHarness.cpp.
# PLUGINHOST_AU makes JUCE define JUCE_PLUGINHOST_AU and link CoreAudioKit on macOS.
juce_add_console_app(oe_host_harness PRODUCT_NAME "oe_host_harness" PLUGINHOST_AU TRUE)
target_sources(oe_host_harness PRIVATE HostHarness.cpp)
target_compile_definitions(oe_host_harness PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_PLUGINHOST_LV2=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(oe_host_harness PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)
