#pragma once

#include <algorithm>
#include <array>
#include <cmath>

#include "dsp/styles/IStyle.h"

namespace oe::dsp
{

/** Quad: 60s/70s multi-head tape echo. Four head pairs A-D (one L and one R
    read per pair) on the two delay lines, each with an on/off switch.

    Spacing 0 %: uniform heads at D/4, D/2, 3D/4, D (Delay = head D) and L = R.
    Spacing > 0 %: the heads move towards an irregular, incommensurate layout
    and the R heads separate from the L heads (stereo spread).

    Repeat: only head D feeds back (rhythmic repeating patterns; head D keeps
    recirculating even if its output switch is off, like the playback head of
    a tape echo whose output is muted). Swell: every active head feeds back
    (normalised by the number of heads) — multi-head resonance, pseudo-reverb
    with high Spacing. Taps layout: tap index = 2 * head + (0 = L, 1 = R). */
class QuadStyle final : public IStyle
{
public:
    static constexpr std::array<double, 4> kUniform { 0.25, 0.5, 0.75, 1.0 };
    static constexpr std::array<double, 4> kIrregular { 0.237, 0.441, 0.719, 1.0 };
    static constexpr std::array<double, 4> kRightOffset { 0.09, 0.07, 0.05, 0.035 };

    Style id() const noexcept override { return Style::Quad; }

    void plan (const StyleParams& p, TapPlan& out) noexcept override
    {
        const auto s = std::clamp (p.spacing, 0.0, 1.0);
        out.numTaps = 8;
        active = 0;
        for (int h = 0; h < 4; ++h)
        {
            const auto hi = static_cast<std::size_t> (h);
            const auto frac = kUniform[hi] + s * (kIrregular[hi] - kUniform[hi]);
            const auto tL = p.delayL * frac;
            const auto tR = tL * (1.0 - s * kRightOffset[hi]);
            const auto l = static_cast<std::size_t> (2 * h);
            out.delay[l] = tL;
            out.delay[l + 1] = tR;
            out.line[l] = 0;
            out.line[l + 1] = 1;
            out.layout[l] = { 0.25 * h, 1.0f, static_cast<std::uint32_t> (2 * h + 1) };
            out.layout[l + 1] = { 0.25 * h + 0.125, -1.0f, static_cast<std::uint32_t> (2 * h + 2) };
            enabled[hi] = p.taps[hi];
            if (enabled[hi])
                ++active;
        }
        swell = p.quadFeedback == QuadFeedback::Swell;
        wetNorm = active > 0 ? 1.0f / std::sqrt (static_cast<float> (active)) : 0.0f;
        swellNorm = active > 0 ? 1.0f / static_cast<float> (active) : 1.0f;
        out.mainDelay = p.delayL;
    }

    void record (float inL, float inR, const float* prev, float fb, float& recL, float& recR) const noexcept override
    {
        float fbL = 0.0f, fbR = 0.0f;
        if (swell && active > 0)
        {
            for (std::size_t h = 0; h < 4; ++h)
                if (enabled[h])
                {
                    fbL += prev[2 * h];
                    fbR += prev[2 * h + 1];
                }
            fbL *= swellNorm;
            fbR *= swellNorm;
        }
        else
        {
            fbL = prev[6];
            fbR = prev[7];
        }
        recL = inL + fb * fbL;
        recR = inR + fb * fbR;
    }

    void wet (const float* taps, float& wetL, float& wetR) const noexcept override
    {
        float l = 0.0f, r = 0.0f;
        for (std::size_t h = 0; h < 4; ++h)
            if (enabled[h])
            {
                l += taps[2 * h];
                r += taps[2 * h + 1];
            }
        wetL = l * wetNorm;
        wetR = r * wetNorm;
    }

private:
    std::array<bool, 4> enabled { true, true, true, true };
    int active = 4;
    bool swell = false;
    float wetNorm = 0.5f, swellNorm = 0.25f;
};

} // namespace oe::dsp
