#pragma once

#include "dsp/core/BitCrusher.h"
#include "dsp/modes/DigitalColor.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

/** Digital: 80s rack delay with a floating-point (gain-ranging) converter.

    Record path (in the loop): the DigitalColor chain (anti-alias low-pass ->
    converter clipping -> mantissa rounding to the Age-controlled number of
    bits, 50 % = 12 bits). Reads use 3rd-order Lagrange interpolation; the
    modulation is a smoothed triangle (Mod Rate / Mod Depth); Era also picks a
    sine LFO for Future. */
class DigitalMode final : public IModeProcessor
{
public:
    Mode id() const noexcept override { return Mode::Digital; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        color.prepare (fs);
        mod.prepare (fs);
        reset();
    }

    void reset() noexcept override
    {
        color.reset();
        mod.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        color.update (ctx.era, BitCrusher::bitsForAge (ctx.age));
        const auto shape = ctx.era == Era::Future ? LfoShape::Sine : LfoShape::SmoothTriangle;
        mod.update (static_cast<double> (ctx.modRateHz), static_cast<double> (ctx.modDepth), shape);
    }

    void advanceModulation() noexcept override { mod.advance(); }
    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override { return mod.at (t, tapDelay); }
    float readTap (const FractionalDelayLine& line, double d, int) noexcept override { return line.readLagrange (d); }
    void processRecord (float& l, float& r) noexcept override { color.process (l, r); }

    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }
    int quantizerBits() const noexcept override { return color.bits(); }
    double recordGroupDelaySamples() const noexcept override { return color.groupDelaySamples(); }

private:
    double fs = 48000.0;
    DigitalColor color;
    PeriodicModulator mod;
};

} // namespace oe::dsp
