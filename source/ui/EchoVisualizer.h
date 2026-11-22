#pragma once

#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "ui/VisualizerData.h"

class OrbitEchoProcessor;

namespace oe::ui
{

/** Animated timeline of the echoes: position = delay times / heads,
    height = level after each repeat (Feedback), blur = Diffusion. Left channel
    above the centre line, right below. A sweeping read head lights the
    repeats as it passes. Data comes from the processor's lock-free FIFO;
    the repaint rate is capped at 60 fps. */
class EchoVisualizer final : public juce::Component, private juce::Timer
{
public:
    explicit EchoVisualizer (OrbitEchoProcessor& processor);
    ~EchoVisualizer() override;

    void paint (juce::Graphics&) override;

    struct Echo
    {
        float timeMs;
        float level; // 0..1
        int channel; // 0 = L, 1 = R
    };

    /** Computes the echo pattern for a frame (exposed for tests). */
    static void buildEchoes (const VisualizerFrame& frame, std::vector<Echo>& out, float windowMs);

private:
    void timerCallback() override;

    OrbitEchoProcessor& processor;
    VisualizerFrame frame;
    std::vector<Echo> echoes;
    float windowMs = 2000.0f;
    float sweepMs = 0.0f;
    float glow = 0.0f;
    double lastTime = 0.0;
};

} // namespace oe::ui
