#pragma once

#include <array>
#include <cstdint>

#include "dsp/core/Compander.h"
#include "dsp/core/Filters.h"
#include "dsp/core/FrequencyShifter.h"
#include "dsp/core/Random.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

/** Ghost Shift: hi-fi tape + frequency shifter + frequency-dependent
    diffusion, all inside the loop.

    Record chain: hi-fi tape curve -> fixed gap loss -> asperity noise (Age)
    -> single-sideband frequency shifter (Freq Shift ± Freq Detune / 2 per
    side: linear shift in Hz, so harmonics become inharmonic and each repeat
    moves further) -> Era dispersion chain.

    Era changes the diffusion: Past = standard (no extra dispersion, only the
    global Diffusion section); Present = frequency-dependent diffusion: a chain
    of 12 first-order all-passes with negative coefficients delays the low
    frequencies more than the highs (highs arrive first); Future = strongly
    frequency dependent: 32 stages — chirping, "ghostly" voices. The
    dispersion strength grows with the Diffusion amount. */
class GhostShiftMode final : public IModeProcessor
{
public:
    static constexpr int kMaxDispersion = 32;

    Mode id() const noexcept override { return Mode::GhostShift; }

    void prepare (double sampleRate) override;
    void reset() noexcept override;
    void update (const ModeContext& ctx) noexcept override;

    void advanceModulation() noexcept override { wow.advance(); }
    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override { return wow.at (t, tapDelay); }

    void processRecord (float& l, float& r) noexcept override;

    double glideSeconds() const noexcept override { return 0.2; }
    double recordGroupDelaySamples() const noexcept override { return groupDelay; }

private:
    float processTape (int ch, float x) noexcept;
    double dispersionGroupDelay (double w) const noexcept;

    double fs = 48000.0;
    FrequencyShifter shifter;
    RandomModulator wow { 909u };
    std::array<Svf, 2> gapLoss;
    std::array<OnePole, 2> noiseHp;
    std::array<EnvelopeFollower, 2> env;
    std::array<FastRandom, 2> rng { FastRandom (0x51ED270Bu), FastRandom (0x9B3F1E2Du) };
    std::array<std::array<double, kMaxDispersion>, 2> apState {};
    int dispersionStages = 0;
    double dispersionCoeff = 0.0;
    float noiseAmount = 0.0f;
    double groupDelay = 0.0;
    double lastHf = -1.0;
};

} // namespace oe::dsp
