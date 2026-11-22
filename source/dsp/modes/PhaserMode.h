#pragma once

#include "dsp/core/Phaser.h"
#include "dsp/core/Saturation.h"
#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** Phaser DDL: digital delay with an analog-style phase shifter INSIDE the
    loop, on the record path. Every repeat is phased once more (psychedelic
    long delays); with Delay and Feedback at 0 the wet signal is simply the
    phaser — a stand-alone phaser.

    Mod Rate / Mod Depth drive the phaser LFO (not the read heads); the depth
    shrinks as the rate rises. Res (replaces Age) = phaser resonance.
    Era = number of all-pass stages (design decision): Past 4 (gentle, two
    notches), Present 6 (three notches), Future 12 (six notches, deep and
    vocal). A soft curve after the phaser bounds the loop. */
class PhaserMode final : public IModeProcessor
{
public:
    Mode id() const noexcept override { return Mode::PhaserDDL; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        phaser.prepare (fs);
        reset();
    }

    void reset() noexcept override { phaser.reset(); }

    void update (const ModeContext& ctx) noexcept override
    {
        const auto stages = ctx.era == Era::Past ? 4 : (ctx.era == Era::Present ? 6 : 12);
        phaser.set (stages, static_cast<double> (ctx.modRateHz), static_cast<double> (ctx.modDepth), static_cast<double> (ctx.res));
    }

    float readTap (const FractionalDelayLine& line, double d, int) noexcept override { return line.readLagrange (d); }

    void processRecord (float& l, float& r) noexcept override
    {
        phaser.process (l, r);
        l = saturate (SatCurve::Soft, l);
        r = saturate (SatCurve::Soft, r);
    }

    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }

private:
    double fs = 48000.0;
    Phaser phaser;
};

} // namespace oe::dsp
