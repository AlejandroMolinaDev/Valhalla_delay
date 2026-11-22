#include "dsp/modes/BBDMode.h"

#include <algorithm>
#include <cmath>

namespace oe::dsp
{

namespace
{
    constexpr double kStages = 4096.0; // MN3005-class chip; clock = stages / (2 x delay)
} // namespace

BBDMode::BBDMode (Mode v) noexcept : variant (v), bbd (static_cast<std::uint32_t> (v) * 17u + 5u) {}

void BBDMode::prepare (double sampleRate)
{
    fs = sampleRate;
    bbd.prepare (fs);
    mod.prepare (fs);
    reset();
}

void BBDMode::reset() noexcept
{
    bbd.reset();
    mod.reset();
}

void BBDMode::update (const ModeContext& ctx) noexcept
{
    const auto delaySec = std::max (ctx.mainDelaySamples / fs, 0.001);
    const auto clockHz = kStages / (2.0 * delaySec);

    double cutoff = 3800.0;
    int order = 4;
    float noiseScale = 1.0f;
    LfoShape shape = LfoShape::SmoothTriangle;

    switch (variant)
    {
        case Mode::BBDBright:
        {
            // Brightness follows the clock: short delays bright, long ones dark.
            const auto eraScale = ctx.era == Era::Past ? 0.8 : (ctx.era == Era::Future ? 1.3 : 1.0);
            cutoff = std::clamp (2600.0 * std::pow (0.3 / delaySec, 0.6), 1600.0, 15000.0) * eraScale;
            order = ctx.era == Era::Future ? 2 : 4;
            noiseScale = 0.8f;
            bbd.setCurves (SatCurve::Bbd, ctx.era == Era::Future ? SatCurve::Soft : SatCurve::Thick);
            break;
        }
        case Mode::AnalogPlus:
            cutoff = ctx.era == Era::Past ? 5600.0 : (ctx.era == Era::Present ? 7600.0 : 12000.0);
            order = ctx.era == Era::Future ? 2 : 4;
            noiseScale = 0.34f; // ~ -6 dB (half) of the BBD mode's noise at the output
            bbd.setCurves (SatCurve::Thick, SatCurve::Thick);
            break;
        default: // BBD: Past / Present dark models, Future bright model
            cutoff = ctx.era == Era::Past ? 2800.0 : (ctx.era == Era::Present ? 3800.0 : 7000.0);
            order = ctx.era == Era::Future ? 2 : 4;
            noiseScale = ctx.era == Era::Past ? 1.3f : 1.0f;
            bbd.setCurves (SatCurve::Soft, SatCurve::Bbd);
            break;
    }

    switch (ctx.era)
    {
        case Era::Past:    shape = LfoShape::Triangle; break;
        case Era::Present: shape = LfoShape::SmoothTriangle; break;
        case Era::Future:  shape = LfoShape::Sine; break;
    }

    bbd.setBandwidth (std::min (cutoff, 0.45 * fs), order);
    bbd.setNoise (ctx.age * noiseScale, clockHz);
    mod.update (static_cast<double> (ctx.modRateHz), static_cast<double> (ctx.modDepth), shape);
}

} // namespace oe::dsp
