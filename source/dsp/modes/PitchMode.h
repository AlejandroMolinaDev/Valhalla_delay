#pragma once

#include <algorithm>
#include <cmath>

#include "dsp/core/BitCrusher.h"
#include "dsp/core/GrainPitchShifter.h"
#include "dsp/modes/DigitalColor.h"
#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** Pitch (and Pitch Duck): digital delay with a chromatic granular pitch
    shifter INSIDE the loop. The heads read the delay line through the grain
    shifter, so the first echo is shifted and every repeat is shifted again:
    with high Feedback the echoes spiral up (or down) endlessly.

    Pitch Shift: ±24 semitones. Pitch Detune: L = +detune/2, R = -detune/2
    cents (micro-shift / doubling). Grain window = clamp(2 x delay, 20, 60 ms).
    Record chain: DigitalColor (Era filters and curves, Age = converter bits).
    Pitch Duck is identical, with the Duck knob in the main section. */
class PitchMode : public IModeProcessor
{
public:
    explicit PitchMode (Mode variant = Mode::Pitch) noexcept : reported (variant) {}

    Mode id() const noexcept override { return reported; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        color.prepare (fs);
        shifter.prepare (fs);
        reset();
    }

    void reset() noexcept override
    {
        color.reset();
        shifter.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        color.update (ctx.era, BitCrusher::bitsForAge (ctx.age));
        setPitch (shifter, ctx.pitchShiftSt, ctx.pitchDetuneCents);
        shifter.setWindow (windowFor (ctx.mainDelaySamples));
    }

    void advanceModulation() noexcept override { shifter.advance(); }

    float readTap (const FractionalDelayLine& line, double d, int tap) noexcept override { return shifter.read (line, d, tap); }

    void processRecord (float& l, float& r) noexcept override { color.process (l, r); }

    double readOffsetSamples (double tapDelay) const noexcept override { return std::min (0.5 * shifter.getWindow(), tapDelay); }
    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }
    int quantizerBits() const noexcept override { return color.bits(); }
    double recordGroupDelaySamples() const noexcept override { return color.groupDelaySamples(); }

    static void setPitch (GrainPitchShifter& s, float semitones, float detuneCents) noexcept
    {
        const auto st = static_cast<double> (semitones);
        const auto half = 0.5 * static_cast<double> (detuneCents) / 100.0;
        s.setRatios (std::exp2 ((st + half) / 12.0), std::exp2 ((st - half) / 12.0));
    }

    double windowFor (double mainDelay) const noexcept { return std::clamp (2.0 * mainDelay, 0.020 * fs, 0.060 * fs); }

protected:
    Mode reported;
    double fs = 48000.0;
    DigitalColor color;
    GrainPitchShifter shifter;
};

} // namespace oe::dsp
