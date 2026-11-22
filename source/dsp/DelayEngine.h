#pragma once

namespace oe::dsp
{

/** Orchestrates Style + Mode + feedback loop (filled in from Phase 1 on). */
class DelayEngine
{
public:
    void prepare (double newSampleRate);
    bool isPrepared() const noexcept { return sampleRate > 0.0; }

private:
    double sampleRate = 0.0;
};

} // namespace oe::dsp
