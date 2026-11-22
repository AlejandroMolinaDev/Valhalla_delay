#pragma once

#include <array>
#include <cstdint>
#include <vector>

#include "dsp/core/RandomLfo.h"

namespace oe::dsp
{

/** Diffusion: eight series Schroeder all-passes per channel inside the loop,
    each lightly modulated so the result never sounds metallic.

    - Amount sets the all-pass gain g (0 % disables the network completely:
      it is skipped, saving CPU). ~68 % gives equal attack and decay (a bell
      shaped echo), ~91 % a dense reverb even with Feedback at 0 %.
    - Size scales the total dispersion time relative to the delay length
      (Σd = (0.01 + 0.99·size²) × reference delay, at most 20 s).
    - The energy centroid of a Schroeder all-pass is exactly its delay d,
      whatever g, so the centroid of the chain is Σd. The engine reads the
      heads Σd earlier: diffused echoes stay centred on the delay grid, and the
      transition from "off" to "barely on" (a pure delay of Σd) is seamless.
    - Switching on/off crossfades between the dry path and the network
      (30 ms); the reported compensation follows the crossfade. */
class DiffusionNetwork
{
public:
    static constexpr int kStages = 8;
    static constexpr int kModDecimation = 16;

    /** Allocates up to `maxSeconds` of total dispersion per channel. */
    void prepare (double sampleRate, double maxSeconds = 20.2);
    void reset() noexcept;

    /** Clears one stage buffer per call (spreads the work over audio blocks).
        Returns true once every stage is clear. */
    bool clearStep() noexcept;

    /** Control rate. referenceDelay in samples (shortest head delay). */
    void setParameters (float amount01, float size01, double referenceDelay, bool snap) noexcept;

    /** True while the network contributes (amount > 0 or still fading out). */
    bool isActive() const noexcept { return active; }

    void process (float& l, float& r) noexcept;

    /** Current centroid compensation (samples) for the read heads. */
    double compensationSamples() const noexcept { return compensation; }

    /** Allpass gain for a given Amount (exposed for tests / docs). */
    static double gainForAmount (double amount01) noexcept;

    /** Number of processed samples (test hook: proves the network is skipped at 0 %). */
    std::uint64_t getProcessCount() const noexcept { return processCount; }

private:
    struct Stage
    {
        std::vector<float> buffer;
        std::uint32_t write = 0;
        double delay = 1.0, target = 1.0, maxDelay = 1.0;
        RandomLfo lfo { 1u };
        double modDepth = 0.0;
        double modNow = 0.0, modStep = 0.0; // modulation evaluated every kModDecimation samples
    };

    float processStage (Stage& s, float x, double g) noexcept;

    double fs = 48000.0;
    std::array<std::array<Stage, kStages>, 2> stages;
    double g = 0.0, gTarget = 0.0, gCoeff = 0.0, delayCoeff = 0.0;
    double mix = 0.0, mixTarget = 0.0, mixStep = 0.0;
    double totalDelay = 0.0;
    double compensation = 0.0;
    bool active = false;
    std::uint64_t processCount = 0;
    int clearIndex = 0;
    int modCountdown = 0;
};

} // namespace oe::dsp
