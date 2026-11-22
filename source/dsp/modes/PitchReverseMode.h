#pragma once

#include <cmath>

#include "dsp/core/BitCrusher.h"
#include "dsp/core/ReversePitchShifter.h"
#include "dsp/modes/DigitalColor.h"
#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** Pitch Reverse: segments of the delay line are played backwards — the
    splice length is the Delay parameter — and pitch-shifted at the same time
    (backwards playback at r x speed). Windowed crossfades between splices.
    In the loop, every repeat is reversed (and shifted) again. Record chain:
    DigitalColor (Age = converter bits). */
class PitchReverseMode final : public IModeProcessor
{
public:
    Mode id() const noexcept override { return Mode::PitchReverse; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        color.prepare (fs);
        reverse.prepare (fs, (kMaxDelaySeconds + kMaxModulationSeconds) * fs);
        reset();
    }

    void reset() noexcept override
    {
        color.reset();
        reverse.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        color.update (ctx.era, BitCrusher::bitsForAge (ctx.age));
        const auto st = static_cast<double> (ctx.pitchShiftSt);
        const auto half = 0.5 * static_cast<double> (ctx.pitchDetuneCents) / 100.0;
        reverse.setRatios (std::exp2 ((st + half) / 12.0), std::exp2 ((st - half) / 12.0));
    }

    void advanceModulation() noexcept override { reverse.advance(); }

    float readTap (const FractionalDelayLine& line, double d, int tap) noexcept override { return reverse.read (line, d, tap); }

    void processRecord (float& l, float& r) noexcept override { color.process (l, r); }

    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }
    int quantizerBits() const noexcept override { return color.bits(); }
    double recordGroupDelaySamples() const noexcept override { return color.groupDelaySamples(); }

private:
    double fs = 48000.0;
    DigitalColor color;
    ReversePitchShifter reverse;
};

} // namespace oe::dsp
