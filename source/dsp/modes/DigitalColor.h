#pragma once

#include <array>

#include "dsp/Types.h"
#include "dsp/core/BitCrusher.h"
#include "dsp/core/Filters.h"
#include "dsp/core/Saturation.h"

namespace oe::dsp
{

/** The 80s floating-point converter colouring shared by Digital and the
    Pitch modes: anti-alias low-pass -> converter curve -> mantissa rounding.

    Era: Past = 9 kHz 4th-order + hard converter knee; Present = 14 kHz
    2nd-order + soft tanh curve; Future = 19.5 kHz 2nd-order + very clean curve. */
class DigitalColor
{
public:
    void prepare (double sampleRate) noexcept
    {
        fs = sampleRate;
        lastEra = -1;
        reset();
    }

    void reset() noexcept
    {
        for (auto& f : antiAlias)
            f.reset();
    }

    /** bits: converter bits (24 = transparent). */
    void update (Era era, int bits) noexcept
    {
        crusher.setBits (bits);
        const auto e = static_cast<int> (era);
        if (e == lastEra)
            return;
        lastEra = e;
        double cutoff = 14000.0;
        int order = 2;
        switch (era)
        {
            case Era::Past:    cutoff = 9000.0;  order = 4; curve = SatCurve::Converter; break;
            case Era::Present: cutoff = 14000.0; order = 2; curve = SatCurve::Soft;      break;
            case Era::Future:  cutoff = 19500.0; order = 2; curve = SatCurve::Clean;     break;
        }
        for (auto& f : antiAlias)
            f.setup (cutoff, order, fs);
    }

    void process (float& l, float& r) noexcept
    {
        l = crusher.process (saturate (curve, static_cast<float> (antiAlias[0].process (static_cast<double> (l)))));
        r = crusher.process (saturate (curve, static_cast<float> (antiAlias[1].process (static_cast<double> (r)))));
    }

    double groupDelaySamples() const noexcept { return antiAlias[0].groupDelaySamples(); }
    int bits() const noexcept { return crusher.getBits(); }

private:
    double fs = 48000.0;
    int lastEra = -1;
    std::array<ButterworthLowpass, 2> antiAlias;
    BitCrusher crusher;
    SatCurve curve = SatCurve::Soft;
};

} // namespace oe::dsp
