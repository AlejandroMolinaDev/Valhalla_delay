#pragma once

#include <cstdint>

#include "dsp/core/Random.h"
#include "dsp/core/RandomLfo.h"
#include "dsp/core/Lfo.h"
#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** Tape transport: motor speed, wow, flutter and the loop splice.

    Delay-linked mode (Tape, Tape Clean, Tape NR — variable-speed motor echoes):
    the delay time is set by the motor speed, so the speed variations scale
    with it. Wow and flutter rates are a factor of 1/delay (a slower capstan
    turns more slowly) and their depth is a fraction of the delay time; the
    resulting pitch wobble therefore keeps the same character at any delay
    time, as on the hardware, while its rate follows the delay.

    Free mode (Reel HiFi — fixed-speed studio machine): random modulation with
    Mod Rate / Mod Depth, independent of the delay time.

    Splice: a tape loop has a splice that passes the heads once per loop
    revolution (period = kSpliceLoopFactor x delay). It causes a short level
    dip and a soft click, both proportional to the signal (no output in
    silence) and to Age. */
class TapeTransport
{
public:
    static constexpr double kSpliceLoopFactor = 8.0;

    explicit TapeTransport (std::uint32_t seed = 11u) noexcept;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Delay-linked wow / flutter (0..1 each), from the smoothed main delay. */
    void setDelayLinked (float wow, float flutter, double mainDelaySamples) noexcept;

    /** Free-running random modulation (Reel HiFi). */
    void setFree (double rateHz, float depth01) noexcept;

    /** Splice artefacts: depth 0..1 (0 = off), loop period from the delay. */
    void setSplice (float amount01, double mainDelaySamples) noexcept;

    void advance() noexcept;

    /** Read-position offset (samples) for one head. */
    double modulation (const TapLayout& t, double tapDelay) const noexcept;

    /** Applies the splice gain dip / click to a record-path sample.
        `envelope` is the signal envelope (clicks scale with it). */
    float applySplice (float x, double envelope, int channel) noexcept;

private:
    double fs = 48000.0;
    bool delayLinked = true;

    RandomLfo wowLfo, flutterLfo;
    mutable RandomLfo::CacheSet<kMaxTaps> wowCache, flutterCache;
    Lfo capstan;                 // periodic component of wow (capstan eccentricity)
    double wowDepth = 0.0;       // samples
    double flutterDepth = 0.0;   // samples
    double capstanDepth = 0.0;   // samples

    double splicePhase = 0.0, spliceIncrement = 0.0;
    float spliceAmount = 0.0f;
    float spliceGainNow = 1.0f;
    float clickNow = 0.0f;
    FastRandom clickRng;
};

} // namespace oe::dsp
