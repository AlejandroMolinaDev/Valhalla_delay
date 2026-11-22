#pragma once

#include <array>
#include <cmath>

#include "dsp/core/BitCrusher.h"
#include "dsp/core/Downsampler.h"
#include "dsp/core/Filters.h"
#include "dsp/core/Saturation.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/modes/Modulators.h"

namespace oe::dsp
{

/** LoFi: extreme degradation.

    Record chain: converter clipping -> (optional anti-alias filter) ->
    fractional sample-and-hold rate reduction -> linear bit crushing -> a
    band-limiting low-pass. Age = amount of rate reduction / aliasing:
    hold rate = host rate x (1/32)^Age (0 % = no reduction, 100 % = 1/32).

    Era = the anti-alias filter and the converter word length:
    Past = no anti-alias filter (raw aliasing), 8 bits; Present = gentle
    1-pole filter at the new Nyquist, 10 bits; Future = 4th-order filter at
    0.45 x the new rate, 12 bits. */
class LoFiMode final : public IModeProcessor
{
public:
    Mode id() const noexcept override { return Mode::LoFi; }

    void prepare (double sampleRate) override
    {
        fs = sampleRate;
        mod.prepare (fs);
        for (auto& f : bandLimit)
            f.setup (std::min (6500.0, 0.45 * fs), 2, fs);
        lastAa = -1.0;
        reset();
    }

    void reset() noexcept override
    {
        for (auto& d : hold)
            d.reset();
        for (auto& f : antiAliasSoft)
            f.reset();
        for (auto& f : antiAliasHard)
            f.reset();
        for (auto& f : bandLimit)
            f.reset();
        mod.reset();
    }

    void update (const ModeContext& ctx) noexcept override
    {
        const auto ratio = std::pow (1.0 / 32.0, static_cast<double> (ctx.age));
        for (auto& d : hold)
            d.setRatio (ratio);

        era = ctx.era;
        bits = era == Era::Past ? 8.0f : (era == Era::Present ? 10.0f : 12.0f);

        const auto newRate = ratio * fs;
        const auto aa = era == Era::Future ? 0.45 * newRate : 0.5 * newRate;
        if (std::abs (aa - lastAa) > 0.5)
        {
            for (auto& f : antiAliasSoft)
                f.setup (aa, fs);
            for (auto& f : antiAliasHard)
                f.setup (aa, 4, fs);
            lastAa = aa;
        }

        aaDelay = era == Era::Present ? onePoleGroupDelay (aa, fs)
                : era == Era::Future  ? antiAliasHard[0].groupDelaySamples()
                                      : 0.0;
        groupDelay = hold[0].averageDelay() + aaDelay + bandLimit[0].groupDelaySamples();

        mod.update (static_cast<double> (ctx.modRateHz), static_cast<double> (ctx.modDepth), LfoShape::Triangle);
    }

    void advanceModulation() noexcept override { mod.advance(); }
    double modulationSamples (const TapLayout& t, double tapDelay) const noexcept override { return mod.at (t, tapDelay); }

    void processRecord (float& l, float& r) noexcept override
    {
        l = processChannel (0, l);
        r = processChannel (1, r);
    }

    double glideSeconds() const noexcept override { return 0.12; }
    bool isDigital() const noexcept override { return true; }
    double recordGroupDelaySamples() const noexcept override { return groupDelay; }

private:
    float processChannel (int ch, float x) noexcept
    {
        const auto c = static_cast<std::size_t> (ch);
        auto y = saturate (SatCurve::Converter, x);
        if (era == Era::Present)
            y = static_cast<float> (antiAliasSoft[c].lowpass (static_cast<double> (y)));
        else if (era == Era::Future)
            y = static_cast<float> (antiAliasHard[c].process (static_cast<double> (y)));
        y = hold[c].process (y);
        y = quantizeLinear (y, bits);
        return static_cast<float> (bandLimit[c].process (static_cast<double> (y)));
    }

    double fs = 48000.0;
    Era era = Era::Present;
    float bits = 10.0f;
    double lastAa = -1.0, aaDelay = 0.0, groupDelay = 0.0;
    std::array<Downsampler, 2> hold;
    std::array<OnePole, 2> antiAliasSoft;
    std::array<ButterworthLowpass, 2> antiAliasHard;
    std::array<ButterworthLowpass, 2> bandLimit;
    PeriodicModulator mod;
};

} // namespace oe::dsp
