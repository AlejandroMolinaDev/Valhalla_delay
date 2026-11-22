#pragma once

#include <array>
#include <cstdint>

#include "dsp/Types.h"
#include "dsp/core/Compander.h"
#include "dsp/core/Filters.h"
#include "dsp/core/Lfo.h"
#include "dsp/core/Random.h"
#include "dsp/core/Saturation.h"

namespace oe::dsp
{

/** Bucket-brigade delay chip model wrapped around the engine's delay line.

    Record side (in the loop): input op-amp saturation -> anti-alias low-pass
    -> 2:1 compressor -> chip clipping -> chip / clock noise.
    Playback side (per read head): reconstruction low-pass -> 1:2 expander.

    The noise is injected between compressor and expander, so the expander
    makes it follow the signal dynamics (loud: noise audible under the echo,
    quiet: noise pushed down); it is also gated by the input envelope so the
    unit is silent in silence. The compressor sits on input + feedback: when
    the input is hot (high Drive) it pulls the recirculating echoes down — the
    BBD "ducking". */
class BBDModel
{
public:
    explicit BBDModel (std::uint32_t seed = 21u) noexcept;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** Anti-alias + reconstruction cutoff (Hz) and order (2 or 4). */
    void setBandwidth (double cutoffHz, int order) noexcept;

    /** Noise amount (Age x mode scale), clock frequency for the clock leakage tone. */
    void setNoise (float amount, double clockHz) noexcept;

    void setCurves (SatCurve inputCurve, SatCurve chipCurve) noexcept;

    float record (int channel, float x) noexcept;
    float playback (int head, float x) noexcept;

    /** DC group delay of anti-alias + reconstruction filters (samples). */
    double groupDelaySamples() const noexcept { return 2.0 * filterGroupDelay; }

private:
    double fs = 48000.0;
    std::array<ButterworthLowpass, 2> antiAlias;
    std::array<ButterworthLowpass, kMaxTaps> reconstruction;
    std::array<Compressor2to1, 2> compressor;
    std::array<Expander1to2, kMaxTaps> expander;
    std::array<EnvelopeFollower, 2> gate;
    std::array<FastRandom, 2> rng;
    std::array<OnePole, 2> noiseTilt;
    Lfo clock;
    float noiseAmount = 0.0f;
    SatCurve inCurve = SatCurve::Soft, chipCurve = SatCurve::Bbd;
    double filterGroupDelay = 0.0;
    double lastCutoff = -1.0;
    int lastOrder = 0;
};

} // namespace oe::dsp
