#pragma once

#include "dsp/core/BitCrusher.h"
#include "dsp/core/Filters.h"
#include "dsp/core/Saturation.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

/** Digital: 80s rack delay with a floating-point (gain-ranging) converter.

    Record path (in the loop): anti-alias low-pass -> converter clipping
    (Era-dependent curve) -> mantissa rounding to the Age-controlled number of
    bits (50 % = 12 bits). Reads use 3rd-order Lagrange interpolation; the
    modulation is a smoothed triangle (Mod Rate / Mod Depth).

    Era: Past = 9 kHz 4th-order + hard converter knee; Present = 14 kHz
    2nd-order + soft tanh curve; Future = 19.5 kHz 2nd-order + very clean curve. */
class DigitalMode : public IModeProcessor
{
public:
    explicit DigitalMode (Mode reportedId = Mode::Digital) noexcept : reported (reportedId) {}

    Mode id() const noexcept override { return reported; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        mod.prepare (fs);
        lastEra = -1;
        reset();
    }

    void reset() noexcept override
    {
        for (auto& f : antiAlias)
            f.reset();
        mod.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        crusher.setBits (BitCrusher::bitsForAge (ctx.age));

        const auto era = static_cast<int> (ctx.era);
        if (era != lastEra)
        {
            lastEra = era;
            double cutoff = 14000.0;
            int order = 2;
            switch (ctx.era)
            {
                case Era::Past:    cutoff = 9000.0;  order = 4; curve = SatCurve::Converter; shape = LfoShape::SmoothTriangle; break;
                case Era::Present: cutoff = 14000.0; order = 2; curve = SatCurve::Soft;      shape = LfoShape::SmoothTriangle; break;
                case Era::Future:  cutoff = 19500.0; order = 2; curve = SatCurve::Clean;     shape = LfoShape::Sine;           break;
            }
            for (auto& f : antiAlias)
                f.setup (cutoff, order, fs);
        }

        mod.update (static_cast<double> (ctx.modRateHz), static_cast<double> (ctx.modDepth), shape);
    }

    void advanceModulation() noexcept override { mod.advance(); }

    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override { return mod.at (t, tapDelay); }

    float readTap (const FractionalDelayLine& line, double d, int) noexcept override { return line.readLagrange (d); }

    void processRecord (float& l, float& r) noexcept override
    {
        l = crusher.process (saturate (curve, static_cast<float> (antiAlias[0].process (l))));
        r = crusher.process (saturate (curve, static_cast<float> (antiAlias[1].process (r))));
    }

    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }
    int quantizerBits() const noexcept override { return crusher.getBits(); }
    double recordGroupDelaySamples() const noexcept override { return antiAlias[0].groupDelaySamples(); }

private:
    Mode reported;
    double fs = 48000.0;
    int lastEra = -1;
    ButterworthLowpass antiAlias[2];
    BitCrusher crusher;
    SatCurve curve = SatCurve::Soft;
    LfoShape shape = LfoShape::SmoothTriangle;
    PeriodicModulator mod;
};

} // namespace oe::dsp
