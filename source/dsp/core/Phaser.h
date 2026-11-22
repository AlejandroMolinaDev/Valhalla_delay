#pragma once

#include <array>

#include "dsp/core/Lfo.h"

namespace oe::dsp
{

/** Analog-style phase shifter: 4, 6 or 12 first-order all-pass stages whose
    break frequency is swept exponentially by an LFO, mixed 50/50 with the
    input (notches), with a resonance feedback around the all-pass chain.

    The sweep depth shrinks as the rate rises (depth_eff = depth / (1 + rate /
    1.5 Hz)), as in classic pedals whose LFO amplitude falls with speed. The
    two channels run in quadrature (90°) for a wide stereo sweep. */
class Phaser
{
public:
    static constexpr int kMaxStages = 12;

    void prepare (double sampleRate) noexcept;
    void reset() noexcept;

    /** stages: 4, 6 or 12. rateHz 0.01..10, depth01 and resonance01 0..1. */
    void set (int stages, double rateHz, double depth01, double resonance01) noexcept;

    void process (float& l, float& r) noexcept;

private:
    double processChannel (int ch, double x, double coefficient) noexcept;
    double coefficientFor (double lfo) const noexcept;

    double fs = 48000.0;
    int numStages = 6;
    double depth = 0.5, feedback = 0.0;
    Lfo lfo;
    std::array<std::array<double, kMaxStages>, 2> state {};
    std::array<double, 2> lastOut {};
};

} // namespace oe::dsp
