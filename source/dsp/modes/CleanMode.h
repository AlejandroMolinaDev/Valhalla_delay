#pragma once

#include <array>
#include <cmath>

#include "dsp/core/Filters.h"
#include "dsp/core/TransparentLimiter.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

/** Clean: modern clean digital delay with a program-dependent limiter in the
    loop instead of a saturation stage, so Drive (and Feedback above 100 %)
    can be pushed without distortion.

    Age = bandwidth: an in-loop low-pass from 20 kHz (0 %) down to 1.5 kHz
    (100 %), log-spaced. Era picks deep, unusual modulation waveforms
    (Past: smoothed square — alternating up/down pitch; Present: ramp —
    slow rise, quick return; Future: asymmetric "hump"), with twice the
    excursion of the other modes. */
class CleanMode final : public IModeProcessor
{
public:
    Mode id() const noexcept override { return Mode::Clean; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        TransparentLimiter::Settings s;
        s.thresholdDb = -1.5f;
        s.attackMs = 1.0f;
        s.releaseFastMs = 40.0f;
        s.releaseSlowMs = 600.0f;
        s.ceiling = 1.0f;
        limiter.prepare (fs, s);
        mod.prepare (fs);
        lastCutoff = -1.0;
        reset();
    }

    void reset() noexcept override
    {
        limiter.reset();
        for (auto& f : bandwidth)
            f.reset();
        mod.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        const auto cutoff = std::min (20000.0 * std::pow (1500.0 / 20000.0, static_cast<double> (ctx.age)), 0.45 * fs);
        if (std::abs (cutoff - lastCutoff) > 0.5)
        {
            for (auto& f : bandwidth)
                f.setup (cutoff, 2, fs);
            lastCutoff = cutoff;
        }

        LfoShape shape = LfoShape::Ramp;
        switch (ctx.era)
        {
            case Era::Past:    shape = LfoShape::SmoothSquare; break;
            case Era::Present: shape = LfoShape::Ramp; break;
            case Era::Future:  shape = LfoShape::Hump; break;
        }
        mod.update (static_cast<double> (ctx.modRateHz), 2.0 * static_cast<double> (ctx.modDepth), shape);
    }

    void advanceModulation() noexcept override { mod.advance(); }
    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override { return mod.at (t, tapDelay); }
    float readTap (const FractionalDelayLine& line, double d, int) noexcept override { return line.readLagrange (d); }

    void processRecord (float& l, float& r) noexcept override
    {
        l = static_cast<float> (bandwidth[0].process (static_cast<double> (l)));
        r = static_cast<float> (bandwidth[1].process (static_cast<double> (r)));
        limiter.process (l, r);
    }

    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }
    double recordGroupDelaySamples() const noexcept override { return bandwidth[0].groupDelaySamples(); }

private:
    double fs = 48000.0;
    double lastCutoff = -1.0;
    std::array<ButterworthLowpass, 2> bandwidth;
    TransparentLimiter limiter;
    PeriodicModulator mod;
};

} // namespace oe::dsp
