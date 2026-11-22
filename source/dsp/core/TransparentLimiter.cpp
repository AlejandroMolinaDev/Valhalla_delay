#include "dsp/core/TransparentLimiter.h"

namespace oe::dsp
{

void TransparentLimiter::prepare (double sampleRate, const Settings& s) noexcept
{
    fs = sampleRate;
    threshold = std::pow (10.0, static_cast<double> (s.thresholdDb) / 20.0);
    ceiling = static_cast<double> (s.ceiling);
    auto coeff = [this] (float ms) { return 1.0 - std::exp (-1.0 / (static_cast<double> (ms) * 0.001 * fs)); };
    attackCoeff = coeff (s.attackMs);
    releaseFast = coeff (s.releaseFastMs);
    releaseSlow = coeff (s.releaseSlowMs);
    rmsCoeff = coeff (300.0f);
    reset();
}

void TransparentLimiter::reset() noexcept
{
    gain = 1.0;
    rms = 0.0;
}

void TransparentLimiter::process (float& l, float& r) noexcept
{
    const auto dl = static_cast<double> (l);
    const auto dr = static_cast<double> (r);
    const auto peak = std::max (std::abs (dl), std::abs (dr));
    rms += rmsCoeff * (0.5 * (dl * dl + dr * dr) - rms);

    const auto target = peak > threshold ? threshold / peak : 1.0;
    if (target < gain)
    {
        gain += attackCoeff * (target - gain);
    }
    else
    {
        // Program dependence: high crest factor (transient material) -> fast release,
        // dense sustained material -> slow release (less pumping, less distortion).
        const auto crest = peak / std::sqrt (rms + 1.0e-12);
        const auto mixAmt = std::clamp ((crest - 1.5) / 4.0, 0.0, 1.0);
        const auto rel = releaseSlow + mixAmt * (releaseFast - releaseSlow);
        gain += rel * (target - gain);
    }

    l = static_cast<float> (std::clamp (dl * gain, -ceiling, ceiling));
    r = static_cast<float> (std::clamp (dr * gain, -ceiling, ceiling));
}

} // namespace oe::dsp
