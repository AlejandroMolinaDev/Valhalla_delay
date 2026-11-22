#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

/** Temporary editor (Phase 1): JUCE's generic parameter editor. The original
    hardware-style interface replaces it in Phase 5. */
class OrbitEchoEditor final : public juce::GenericAudioProcessorEditor
{
public:
    explicit OrbitEchoEditor (OrbitEchoProcessor& p) : GenericAudioProcessorEditor (p) {}
};
