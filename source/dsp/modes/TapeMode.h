#pragma once

#include <array>

#include "dsp/core/Compander.h"
#include "dsp/core/Filters.h"
#include "dsp/core/Random.h"
#include "dsp/core/Saturation.h"
#include "dsp/core/TapeTransport.h"
#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** The four tape algorithms share one implementation with a variant table:

    | Variant    | Transport         | EQ                        | Noise (Age)          | Extras            |
    |------------|-------------------|---------------------------|----------------------|-------------------|
    | Tape       | delay-linked w/f  | follows delay time        | asperity + splices   |                   |
    | Tape Clean | delay-linked w/f  | follows delay, brighter   | light asperity       | finer saturation  |
    | Reel HiFi  | free (Mod R/D)    | full band, fixed          | asperity             |                   |
    | Tape NR    | delay-linked w/f  | follows delay, brightest  | very light           | 2:1 / 1:2 NR loop |

    Record chain: (NR encode) -> pre-emphasis -> tape saturation with soft
    hysteresis -> de-emphasis -> gap-loss low-pass (cutoff tracks the tape
    speed, i.e. 1/delay) -> head bump -> signal-modulated asperity noise ->
    splice. Playback: modulated read (wow / flutter) -> (NR decode). */
class TapeMode final : public IModeProcessor
{
public:
    explicit TapeMode (Mode variant) noexcept;

    Mode id() const noexcept override { return variant; }
    void prepare (double sampleRate) override;
    void reset() noexcept override;
    void update (const ModeContext& ctx) noexcept override;

    void advanceModulation() noexcept override { transport.advance(); }
    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override
    {
        return transport.modulation (t, tapDelay);
    }

    float postRead (int tap, float x) noexcept override;
    void processRecord (float& l, float& r) noexcept override;

    double glideSeconds() const noexcept override { return variant == Mode::ReelHiFi ? 0.20 : 0.35; }
    double recordGroupDelaySamples() const noexcept override { return groupDelay; }

private:
    float processChannel (int ch, float x) noexcept;

    Mode variant;
    double fs = 48000.0;
    TapeTransport transport;

    // Per-variant constants
    double hfBase = 7500.0;    // gap-loss cutoff at the reference speed (150 ms)
    bool tracksSpeed = true;
    float noiseScale = 1.0f;
    bool splices = false;
    bool noiseReduction = false;
    float bias = 0.12f;

    // Current state
    SatCurve curve = SatCurve::Tape;
    float inputScale = 1.0f;
    float noiseAmount = 0.0f;
    float hystUp = 1.0f, hystDown = 1.0f;
    double groupDelay = 0.0;
    double lastHf = -1.0, lastQ = -1.0, lastBump = -1.0;
    float bumpGain = 0.0f;
    float chainGain = 1.0f;

    std::array<OnePole, 2> preEmph, deEmph, noiseHp;
    std::array<Svf, 2> gapLoss, headBump;
    std::array<EnvelopeFollower, 2> env;
    std::array<FastRandom, 2> rng;
    std::array<float, 2> magnet {};
    std::array<Compressor2to1, 2> nrEncode;
    std::array<Expander1to2, kMaxTaps> nrDecode;
};

} // namespace oe::dsp
