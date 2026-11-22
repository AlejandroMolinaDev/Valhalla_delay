#pragma once

#include "dsp/core/BitCrusher.h"
#include "dsp/core/TransparentLimiter.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

/** Crystal: transparent delay for looping.

    Low Cut / High Cut are moved OUT of the loop (the engine applies them to
    the wet signal), and the loop only contains a nearly colourless, slow
    limiter: repeats stay bright, Feedback 100 % sustains a clean loop and
    higher settings build up without harshness. Full-band reads (Lagrange),
    sine modulation. Age = converter bit depth (like Digital). */
class CrystalMode final : public IModeProcessor
{
public:
    Mode id() const noexcept override { return Mode::Crystal; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        TransparentLimiter::Settings s;
        s.thresholdDb = -1.0f;
        s.attackMs = 5.0f;
        s.releaseFastMs = 300.0f;
        s.releaseSlowMs = 1500.0f;
        s.ceiling = 1.0f;
        limiter.prepare (fs, s);
        mod.prepare (fs);
        reset();
    }

    void reset() noexcept override
    {
        limiter.reset();
        mod.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        crusher.setBits (BitCrusher::bitsForAge (ctx.age));
        mod.update (static_cast<double> (ctx.modRateHz), static_cast<double> (ctx.modDepth), LfoShape::Sine);
    }

    void advanceModulation() noexcept override { mod.advance(); }
    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override { return mod.at (t, tapDelay); }
    float readTap (const FractionalDelayLine& line, double d, int) noexcept override { return line.readLagrange (d); }

    void processRecord (float& l, float& r) noexcept override
    {
        limiter.process (l, r);
        l = crusher.process (l);
        r = crusher.process (r);
    }

    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }
    bool loopEqInLoop() const noexcept override { return false; }
    int quantizerBits() const noexcept override { return crusher.getBits(); }

private:
    double fs = 48000.0;
    TransparentLimiter limiter;
    BitCrusher crusher;
    PeriodicModulator mod;
};

} // namespace oe::dsp
