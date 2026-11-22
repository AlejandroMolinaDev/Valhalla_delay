#pragma once

#include <cmath>

#include "dsp/core/GrainPitchShifter.h"
#include "dsp/core/RandomLfo.h"
#include "dsp/modes/PitchMode.h"

namespace oe::dsp
{

/** Pitch Rich: a pitch-shifted path and a "lush" detuned path that share the
    same delay time, mixed inside the loop. Pitch Detune affects both paths
    (the lush path is only detuned, with a slow chorus drift). Pitch Mix
    (replaces Age) balances lush (0 %) <-> pitch-shifted (100 %) with an
    equal-power law. With high Diffusion this becomes a shimmer reverb. The
    converter runs at full resolution (no Age). */
class PitchRichMode final : public PitchMode
{
public:
    PitchRichMode() noexcept : PitchMode (Mode::PitchRich) {}

    void prepare (double sampleRate) override
    {
        PitchMode::prepare (sampleRate);
        lush.prepare (sampleRate);
        drift.setRate (0.37, sampleRate);
        driftDepth = 0.0015 * sampleRate; // ±1.5 ms chorus drift
    }

    void reset() noexcept override
    {
        PitchMode::reset();
        lush.reset();
        drift.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        color.update (ctx.era, 24);
        setPitch (shifter, ctx.pitchShiftSt, ctx.pitchDetuneCents);
        setPitch (lush, 0.0f, ctx.pitchDetuneCents);
        const auto w = windowFor (ctx.mainDelaySamples);
        shifter.setWindow (w);
        lush.setWindow (w);
        const auto theta = 0.5 * 3.14159265358979323846 * static_cast<double> (ctx.pitchMix);
        lushGain = static_cast<float> (std::cos (theta));
        shiftGain = static_cast<float> (std::sin (theta));
    }

    void advanceModulation() noexcept override
    {
        shifter.advance();
        lush.advance();
        drift.advance();
    }

    float readTap (const FractionalDelayLine& line, double d, int tap) noexcept override
    {
        const auto chorus = driftDepth * drift.value (static_cast<double> (tap) * 977.3);
        const auto lushOut = lush.read (line, std::max (0.0, d + chorus), tap);
        return lushGain * lushOut + shiftGain * shifter.read (line, d, tap);
    }

    int quantizerBits() const noexcept override { return -1; }

private:
    GrainPitchShifter lush;
    RandomLfo drift { 4242u };
    double driftDepth = 0.0;
    float lushGain = 0.7071f, shiftGain = 0.7071f;
};

} // namespace oe::dsp
