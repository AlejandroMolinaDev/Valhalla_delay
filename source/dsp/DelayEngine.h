#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>

#include "dsp/Types.h"
#include "dsp/core/DiffusionNetwork.h"
#include "dsp/core/Ducker.h"
#include "dsp/core/Filters.h"
#include "dsp/core/FractionalDelayLine.h"
#include "dsp/core/LoopEQ.h"
#include "dsp/core/Smoothers.h"
#include "dsp/core/TransparentLimiter.h"
#include "dsp/modes/IModeProcessor.h"
#include "dsp/styles/DualStyle.h"
#include "dsp/styles/PingPongStyle.h"
#include "dsp/styles/QuadStyle.h"
#include "dsp/styles/RatioStyle.h"
#include "dsp/styles/SingleStyle.h"

namespace oe::dsp
{

/** Orchestrates Style + Mode + the feedback loop.

    Per sample:
      1. Duck envelope from the dry input.
      2. Record path: Style routes (Drive x input) + Feedback x previous head
         outputs into the two delay lines, through the mode's in-loop chain
         (saturation, Age, era filters, ...), Diffusion, Loop EQ (not in
         Crystal), DC blocker and a last-resort clamp.
      3. Read heads: the Style's taps are read with the mode's reader
         (modulation, interpolation, pitch / reverse readers).
      4. Wet = Style mix of the heads -> mode wet stage -> Drive compensation ->
         duck -> safety limiter (-0.3 dBFS) -> Width -> equal-power Mix.

    Everything runs sample by sample, and every control-rate update happens on
    an absolute sample clock (every kControlInterval samples), so the output
    is bit-identical whatever the host block size is.

    Not thread-safe: prepare()/reset() from the message thread while audio is
    stopped (or from the audio thread itself), process() from the audio thread. */
class DelayEngine
{
public:
    DelayEngine();
    ~DelayEngine();

    DelayEngine (const DelayEngine&) = delete;
    DelayEngine& operator= (const DelayEngine&) = delete;

    /** Allocates the delay lines (20 s + modulation margin at this rate). */
    void prepare (double sampleRate);
    bool isPrepared() const noexcept { return fs > 0.0; }
    double getSampleRate() const noexcept { return fs; }

    /** Clears every buffer and filter state (no allocation). */
    void reset() noexcept;

    void setParameters (const EngineParams& p) noexcept { params = p; }
    const EngineParams& getParameters() const noexcept { return params; }

    /** Host bypass: the dry signal passes, the wet fades out smoothly and the
        delay lines are then cleared progressively. */
    void setBypassed (bool shouldBeBypassed) noexcept { bypassTarget = shouldBeBypassed; }

    /** In-place stereo processing (left/right may not alias). Any block size, including 0 and 1. */
    void process (float* left, float* right, int numSamples) noexcept;

    /** Conservative tail estimate for the host (60 s when Feedback >= 100 %). */
    static double estimateTailSeconds (const EngineParams& p) noexcept;

    // ---- Introspection (tests, visualiser) -------------------------------
    int getQuantizerBits() const noexcept;
    const TapPlan& getTapPlan() const noexcept { return plan; }
    double getTapDelaySamples (int tap) const noexcept;
    Mode getActiveMode() const noexcept;
    float getLastWetRms() const noexcept { return lastWetRms; }
    std::uint64_t getResetCount() const noexcept { return resetCount; }
    std::uint64_t getDiffusionProcessCount() const noexcept { return diffusion.getProcessCount(); }
    double getDiffusionCompensation() const noexcept { return diffusion.compensationSamples(); }

private:
    struct TimeCrossfade
    {
        double from = 0.0, to = 0.0;
        LinearRamp ramp;
    };

    void controlUpdate() noexcept;
    void updateModeContext() noexcept;
    void switchMode (Mode mode, Era era) noexcept;
    void applyStylePlan (bool snap) noexcept;
    IStyle& styleFor (Style s) noexcept;
    float readHead (IModeProcessor& m, int tap, double delay) noexcept;
    void processRecordChain (float& l, float& r) noexcept;
    void progressiveClear() noexcept;

    double fs = 0.0;
    EngineParams params;
    std::array<FractionalDelayLine, 2> lines;

    // Two banks of the 17 modes: a mode or era change crossfades from the
    // instance in one bank to a freshly reset instance in the other one.
    std::array<std::unique_ptr<IModeProcessor>, kNumModes> bankA, bankB;
    bool usingBankA = true;
    IModeProcessor* current = nullptr;
    IModeProcessor* previous = nullptr;
    Mode currentMode = Mode::Tape;
    Era currentEra = Era::Present;
    LinearRamp modeRamp;
    float modeMix = 1.0f;
    ModeContext modeContext;

    SingleStyle singleStyle;
    DualStyle dualStyle;
    RatioStyle ratioStyle;
    PingPongStyle pingPongStyle;
    QuadStyle quadStyle;
    IStyle* style = &singleStyle;
    Style currentStyle = Style::Single;
    Style pendingStyle = Style::Single;
    enum class StyleFade { None, Out, In } styleFade = StyleFade::None;
    LinearRamp styleRamp;
    float styleGain = 1.0f;

    TapPlan plan;
    std::array<MotorSmoother, kMaxTaps> tapTime;
    std::array<TimeCrossfade, kMaxTaps> timeXfade;
    MotorSmoother mainDelay;
    std::array<float, kMaxTaps> heads {};
    std::array<float, kMaxTaps> prevHeads {};
    double maxReadDelay = 0.0;
    double glideSeconds = -1.0;

    LoopEQ loopEq, wetEq;
    DiffusionNetwork diffusion;
    DcBlocker dc[2];
    SafetyLimiter limiter;
    Ducker ducker;

    OnePoleSmoother<float> feedbackS, dryS, wetS, widthS, driveS, driveCompS;
    float bypassAmount = 0.0f, bypassStep = 0.0f; // linear 30 ms bypass ramp

    int controlCountdown = 0;
    bool needsSnap = true;
    bool bypassTarget = false;
    bool fullyBypassed = false;
    std::size_t clearPosition = 0;
    bool clearPending = false;
    bool diffusionClearPending = false;
    bool nanDetected = false;
    std::uint64_t resetCount = 0;
    float lastWetRms = 0.0f;
};

} // namespace oe::dsp
