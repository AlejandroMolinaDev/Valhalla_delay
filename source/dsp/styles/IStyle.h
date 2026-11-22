#pragma once

#include <array>

#include "dsp/Types.h"
#include "dsp/modes/IModeProcessor.h"

namespace oe::dsp
{

/** Delay-time inputs resolved by the engine (sync already applied), in samples. */
struct StyleParams
{
    double delayL = 0.0;      // "Delay" / "Delay L"
    double delayR = 0.0;      // "Delay R" (Dual, PingPong)
    double spread = 0.0;      // Single
    double ratio = 0.618;     // Ratio
    double spacing = 0.0;     // Quad
    std::array<bool, 4> taps { true, true, true, true };
    QuadFeedback quadFeedback = QuadFeedback::Repeat;
};

/** Read heads requested by a Style for the current control period. */
struct TapPlan
{
    int numTaps = 2;
    std::array<double, kMaxTaps> delay {};       // target delay, samples
    std::array<int, kMaxTaps> line {};           // 0 = left delay line, 1 = right
    std::array<TapLayout, kMaxTaps> layout {};   // modulation relationship
    double mainDelay = 0.0;                      // drives tape speed / BBD clock tracking
};

/** L/R topology: which heads are read, how the input and the feedback are
    routed into the two delay lines, and how the heads form the wet output. */
class IStyle
{
public:
    virtual ~IStyle() = default;

    virtual Style id() const noexcept = 0;

    /** Control rate: computes the read heads for the given times. */
    virtual void plan (const StyleParams& p, TapPlan& out) noexcept = 0;

    /** Signal written into the left/right delay line before the mode chain,
        from the (driven) input and the previous sample's head outputs. */
    virtual void record (float inL, float inR, const float* prevTaps, float feedback,
                         float& recL, float& recR) const noexcept = 0;

    /** Wet output from the current head outputs. */
    virtual void wet (const float* taps, float& wetL, float& wetR) const noexcept = 0;
};

} // namespace oe::dsp
