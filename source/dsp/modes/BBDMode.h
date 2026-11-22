#pragma once

#include "dsp/core/BBDModel.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

/** BBD, BBD Bright and Analog+ share the bucket-brigade model:

    | Variant    | Bandwidth                                   | Noise | Saturation        |
    |------------|---------------------------------------------|-------|-------------------|
    | BBD        | dark: Past 2.8 k / Present 3.8 k (4th order), Future 7 k "bright model" | 1.0 | op-amp + chip |
    | BBD Bright | follows the delay time (clock rate): short = bright, long = dark | 0.8 | two-stage, asymmetric |
    | Analog+    | ~2x BBD (5.6 k / 7.6 k / 12 k)              | ~1/2 (-6 dB) | thick      |

    Modulation: smoothed triangle LFO (Mod Rate / Mod Depth) -> vibrato and
    "slews". Age = chip noise level (it follows the signal dynamics through
    the compander). Era also changes the LFO shape. */
class BBDMode : public IModeProcessor
{
public:
    explicit BBDMode (Mode variant) noexcept;

    Mode id() const noexcept override { return variant; }
    void prepare (double sampleRate) override;
    void reset() noexcept override;
    void update (const ModeContext& ctx) noexcept override;

    void advanceModulation() noexcept override { mod.advance(); }
    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override { return mod.at (t, tapDelay); }

    float postRead (int tap, float x) noexcept override { return bbd.playback (tap, x); }
    void processRecord (float& l, float& r) noexcept override
    {
        l = bbd.record (0, l);
        r = bbd.record (1, r);
    }

    double glideSeconds() const noexcept override { return 0.25; }
    double recordGroupDelaySamples() const noexcept override { return bbd.groupDelaySamples(); }

private:
    Mode variant;
    double fs = 48000.0;
    BBDModel bbd;
    PeriodicModulator mod;
};

} // namespace oe::dsp
