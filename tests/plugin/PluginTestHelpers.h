#pragma once

#include <cmath>
#include <vector>

#include "PluginProcessor.h"
#include "dsp/TempoSync.h"

namespace oe::test
{

inline void setParam (OrbitEchoProcessor& p, const char* paramId, float value)
{
    auto* param = p.getState().getParameter (paramId);
    jassert (param != nullptr);
    param->setValueNotifyingHost (param->convertTo0to1 (value));
}

inline float getParam (OrbitEchoProcessor& p, const char* paramId)
{
    return p.getState().getRawParameterValue (paramId)->load();
}

inline void prepare (OrbitEchoProcessor& p, double sampleRate, int blockSize, int inputs = 2, int outputs = 2)
{
    juce::AudioProcessor::BusesLayout layout;
    layout.inputBuses.add (inputs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    layout.outputBuses.add (outputs == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo());
    p.setBusesLayout (layout);
    p.setRateAndBufferSizeDetails (sampleRate, blockSize);
    p.prepareToPlay (sampleRate, blockSize);
}

/** Processes `buffer` in consecutive chunks of the given sizes (cycled), like a host would. */
inline void processInChunks (OrbitEchoProcessor& p, juce::AudioBuffer<float>& buffer,
                             const std::vector<int>& sizes, bool bypassed = false)
{
    juce::MidiBuffer midi;
    int pos = 0;
    std::size_t k = 0;
    while (pos < buffer.getNumSamples())
    {
        const auto n = std::min (sizes[k++ % sizes.size()], buffer.getNumSamples() - pos);
        juce::AudioBuffer<float> view (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), pos, n);
        if (bypassed)
            p.processBlockBypassed (view, midi);
        else
            p.processBlock (view, midi);
        pos += n;
    }
}

inline bool isFinite (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (b.getSample (ch, i)))
                return false;
    return true;
}

inline int argmaxAbs (const juce::AudioBuffer<float>& b, int ch, int from = 0, int to = -1)
{
    if (to < 0)
        to = b.getNumSamples();
    int best = from;
    float bestV = -1.0f;
    for (int i = from; i < to; ++i)
        if (std::abs (b.getSample (ch, i)) > bestV)
        {
            bestV = std::abs (b.getSample (ch, i));
            best = i;
        }
    return best;
}

/** A clean, unmodulated digital setup for timing checks. */
inline void cleanDigital (OrbitEchoProcessor& p, float delayMs)
{
    setParam (p, params::id::mode, static_cast<float> (dsp::Mode::Digital));
    setParam (p, params::id::era, 2.0f);
    setParam (p, params::id::style, 0.0f);
    setParam (p, params::id::syncL, 0.0f);
    setParam (p, params::id::delayL, delayMs);
    setParam (p, params::id::feedback, 0.0f);
    setParam (p, params::id::age, 0.0f);
    setParam (p, params::id::modDepth, 0.0f);
    setParam (p, params::id::mix, 100.0f);
    setParam (p, params::id::lowCut, 10.0f);
    setParam (p, params::id::highCut, 20000.0f);
}

/** Minimal host play head reporting a tempo. */
class TestPlayHead final : public juce::AudioPlayHead
{
public:
    double bpm = 120.0;
    bool reportTempo = true;

    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        if (reportTempo)
            info.setBpm (bpm);
        info.setIsPlaying (true);
        return info;
    }
};

} // namespace oe::test
