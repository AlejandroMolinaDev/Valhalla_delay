#pragma once

#include <cstdint>

#include "dsp/Types.h"
#include "dsp/core/FractionalDelayLine.h"

namespace oe::dsp
{

/** Control-rate snapshot handed to the active mode (every kControlInterval samples). */
struct ModeContext
{
    double sampleRate = 48000.0;
    Era era = Era::Present;
    float age = 0.2f;
    float pitchMix = 0.5f;
    float res = 0.3f;
    float wow = 0.25f;
    float flutter = 0.15f;
    float modRateHz = 0.5f;
    float modDepth = 0.1f;
    float freqShiftHz = 5.0f;
    float freqDetuneHz = 1.0f;
    float pitchShiftSt = 12.0f;
    float pitchDetuneCents = 10.0f;
    float driveDb = 0.0f;
    float diffAmount = 0.0f;
    double mainDelaySamples = 0.0;  // smoothed main delay (tape speed / BBD clock tracking)
};

/** How a tap relates to the modulation source (set by the Style). */
struct TapLayout
{
    double phaseOffset = 0.0;       // in LFO cycles (periodic) or random steps
    float polarity = 1.0f;          // -1 = anti-phase
    std::uint32_t decorrelation = 0; // >0: independent random stream (Quad taps)
};

/** Common interface of the 17 algorithms.

    Each mode contributes three things to the engine:
      - a read side: how a tap is read from the delay line (modulation,
        interpolation, grain pitch shifting, reverse playback) and an optional
        per-tap post-read stage (compander expanders, noise-reduction decode);
      - a record side, inside the feedback loop: saturation / drive, Age
        artefacts, era filters, frequency shifter, phaser, bit reduction...;
      - optional out-of-loop wet processing.

    prepare() may allocate; everything else is real-time safe. */
class IModeProcessor
{
public:
    virtual ~IModeProcessor() = default;

    virtual Mode id() const noexcept = 0;
    virtual void prepare (double sampleRate) = 0;
    virtual void reset() noexcept = 0;
    virtual void update (const ModeContext& ctx) noexcept = 0;

    /** Advances the modulation sources by one sample. */
    virtual void advanceModulation() noexcept {}

    /** Modulation offset (samples) for one tap whose nominal delay is tapDelay. */
    virtual double modulationSamples (const TapLayout&, double /*tapDelay*/) const noexcept { return 0.0; }

    virtual float readTap (const FractionalDelayLine& line, double delaySamples, int /*tap*/) noexcept
    {
        return line.readHermite (delaySamples);
    }

    virtual float postRead (int /*tap*/, float x) noexcept { return x; }

    /** Record path, inside the loop (input + feedback -> this -> delay line). */
    virtual void processRecord (float& l, float& r) noexcept = 0;

    /** Out-of-loop processing of the wet signal. */
    virtual void processWet (float&, float&) noexcept {}

    /** Inertia of the delay-time changes ("motor"), seconds. */
    virtual double glideSeconds() const noexcept { return 0.12; }

    /** Digital modes may crossfade instead of gliding (internal option). */
    virtual bool isDigital() const noexcept { return false; }

    /** Crystal moves Low/High Cut out of the loop. */
    virtual bool loopEqInLoop() const noexcept { return true; }

    /** Average extra delay introduced by a grain/reverse reader, subtracted
        from the echo time so the echoes stay on time (zero added latency). */
    virtual double readOffsetSamples (double /*tapDelay*/) const noexcept { return 0.0; }

    /** DC group delay (samples) of the mode's in-loop filters; the engine reads
        that much earlier so every repeat stays on the delay grid. */
    virtual double recordGroupDelaySamples() const noexcept { return 0.0; }

    /** Effective converter bits (Digital / Crystal / Pitch*), -1 if not applicable. */
    virtual int quantizerBits() const noexcept { return -1; }

    unsigned visibleControls() const noexcept { return modeControls (id()); }
};

} // namespace oe::dsp
