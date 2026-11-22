#include "dsp/core/TapeTransport.h"

#include <algorithm>
#include <cmath>

#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

namespace
{
    // Rate factors: rate (Hz) = factor / delay (s). At 400 ms: wow 1 Hz, flutter 7.5 Hz.
    constexpr double kWowRateFactor = 0.4;
    constexpr double kFlutterRateFactor = 3.0;
    // Depth as a fraction of the delay time at 100 % (peak pitch deviation
    // 2*pi*factor*depth: wow ~1 %, flutter ~2 % at 100 %; the defaults give a few cents).
    constexpr double kWowDepth = 0.004;
    constexpr double kFlutterDepth = 0.0012;
    constexpr double kSpliceDipSeconds = 0.018;
} // namespace

TapeTransport::TapeTransport (std::uint32_t seed) noexcept
    : wowLfo (seed * 3u + 1u), flutterLfo (seed * 7u + 5u), clickRng (seed * 13u + 9u)
{
}

void TapeTransport::prepare (double sampleRate) noexcept
{
    fs = sampleRate;
    reset();
}

void TapeTransport::reset() noexcept
{
    wowLfo.reset();
    flutterLfo.reset();
    capstan.reset();
    wowCache = {};
    flutterCache = {};
    splicePhase = 0.37; // first splice not right at the start
    spliceGainNow = 1.0f;
    clickNow = 0.0f;
}

void TapeTransport::setDelayLinked (float wow, float flutter, double mainDelaySamples) noexcept
{
    delayLinked = true;
    const auto delaySec = std::max (mainDelaySamples / fs, 0.005);
    wowLfo.setRate (std::clamp (kWowRateFactor / delaySec, 0.1, 4.0), fs);
    flutterLfo.setRate (std::clamp (kFlutterRateFactor / delaySec, 2.0, 25.0), fs);
    capstan.setRate (std::clamp (0.6 * kWowRateFactor / delaySec, 0.08, 3.0), fs);

    const auto maxDepth = 0.010 * fs; // never more than 10 ms
    wowDepth = std::min (static_cast<double> (wow) * kWowDepth * mainDelaySamples, maxDepth);
    capstanDepth = 0.35 * wowDepth;
    flutterDepth = std::min (static_cast<double> (flutter) * kFlutterDepth * mainDelaySamples, 0.002 * fs);
}

void TapeTransport::setFree (double rateHz, float depth01) noexcept
{
    delayLinked = false;
    wowLfo.setRate (rateHz, fs);
    flutterLfo.setRate (std::clamp (rateHz * 9.0, 2.0, 40.0), fs);
    wowDepth = static_cast<double> (depth01) * kMaxModDepthMs * 0.001 * fs;
    flutterDepth = 0.04 * wowDepth;
    capstanDepth = 0.0;
}

void TapeTransport::setSplice (float amount01, double mainDelaySamples) noexcept
{
    spliceAmount = std::clamp (amount01, 0.0f, 1.0f);
    const auto period = std::clamp (kSpliceLoopFactor * mainDelaySamples / fs, 0.5, 20.0);
    spliceIncrement = 1.0 / (period * fs);
}

void TapeTransport::advance() noexcept
{
    wowLfo.advance();
    flutterLfo.advance();
    capstan.advance();

    // Splice envelope: raised-cosine dip of ~18 ms around phase 0 + a short click.
    splicePhase += spliceIncrement;
    if (splicePhase >= 1.0)
    {
        splicePhase -= 1.0;
        clickNow = spliceAmount;
    }

    const auto dipLength = kSpliceDipSeconds * fs * spliceIncrement; // in phase units
    if (spliceAmount > 0.0f && splicePhase < dipLength)
    {
        const auto u = splicePhase / dipLength;
        const auto dip = 0.5 - 0.5 * std::cos (kTwoPi * u);
        spliceGainNow = 1.0f - 0.5f * spliceAmount * static_cast<float> (dip);
    }
    else
    {
        spliceGainNow = 1.0f;
    }
    clickNow *= 0.92f; // ~0.3 ms decay at 48 kHz
}

double TapeTransport::modulation (const TapLayout& t, double tapDelay) const noexcept
{
    const auto offset = t.phaseOffset + 4099.37 * static_cast<double> (t.decorrelation);
    const auto pol = static_cast<double> (t.polarity);
    auto m = wowDepth * wowLfo.value (offset, wowCache.forOffset (offset))
           + flutterDepth * flutterLfo.value (offset * 1.7, flutterCache.forOffset (offset * 1.7));
    if (capstanDepth > 0.0)
        m += capstanDepth * capstan.value (LfoShape::Sine, t.phaseOffset);
    return pol * std::min (std::abs (m), 0.45 * tapDelay + 1.0) * (m < 0.0 ? -1.0 : 1.0);
}

float TapeTransport::applySplice (float x, double envelope, int channel) noexcept
{
    if (spliceAmount <= 0.0f)
        return x;
    auto y = x * spliceGainNow;
    if (clickNow > 1.0e-4f)
    {
        const auto n = clickRng.nextBipolar();
        y += (channel == 0 ? n : -n) * clickNow * 0.25f * static_cast<float> (envelope);
    }
    return y;
}

} // namespace oe::dsp
