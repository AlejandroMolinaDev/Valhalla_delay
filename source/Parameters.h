#pragma once

#include <atomic>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "dsp/Types.h"

namespace oe::params
{

/** Parameter version used in every juce::ParameterID (stable automation IDs). */
constexpr int kVersion = 1;

namespace id
{
    inline constexpr const char* mix = "mix";
    inline constexpr const char* style = "style";
    inline constexpr const char* mode = "mode";
    inline constexpr const char* era = "era";
    inline constexpr const char* delayL = "delayL";
    inline constexpr const char* delayR = "delayR";
    inline constexpr const char* syncL = "syncL";
    inline constexpr const char* syncR = "syncR";
    inline constexpr const char* spread = "spread";
    inline constexpr const char* ratio = "ratio";
    inline constexpr const char* spacing = "spacing";
    inline constexpr const char* tapA = "tapA";
    inline constexpr const char* tapB = "tapB";
    inline constexpr const char* tapC = "tapC";
    inline constexpr const char* tapD = "tapD";
    inline constexpr const char* quadMode = "quadMode";
    inline constexpr const char* feedback = "feedback";
    inline constexpr const char* width = "width";
    inline constexpr const char* drive = "drive";
    inline constexpr const char* age = "age";
    inline constexpr const char* pitchMix = "pitchMix";
    inline constexpr const char* res = "res";
    inline constexpr const char* diffAmount = "diffAmount";
    inline constexpr const char* diffSize = "diffSize";
    inline constexpr const char* lowCut = "lowCut";
    inline constexpr const char* highCut = "highCut";
    inline constexpr const char* wow = "wow";
    inline constexpr const char* flutter = "flutter";
    inline constexpr const char* modRate = "modRate";
    inline constexpr const char* modDepth = "modDepth";
    inline constexpr const char* freqShift = "freqShift";
    inline constexpr const char* freqDetune = "freqDetune";
    inline constexpr const char* pitchShift = "pitchShift";
    inline constexpr const char* pitchDetune = "pitchDetune";
    inline constexpr const char* duck = "duck";
} // namespace id

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Every parameter ID, in display order. */
const std::vector<juce::String>& allIds();

/** Whether a control is relevant (shown) for the given Mode x Style. All
    parameters always exist and keep their values; this only drives the UI. */
bool isVisible (const juce::String& paramId, dsp::Mode mode, dsp::Style style);

/** Duck sits in the main section for the modes that have it, in "Extra" otherwise. */
bool duckIsInMainSection (dsp::Mode mode);

/** Cached raw parameter pointers (lock-free reads on the audio thread). */
class Refs
{
public:
    explicit Refs (juce::AudioProcessorValueTreeState& apvts);

    /** Builds the engine snapshot (percentages -> 0..1 etc.). */
    dsp::EngineParams load (double bpm) const noexcept;

private:
    std::atomic<float>* get (juce::AudioProcessorValueTreeState&, const char*);

    std::atomic<float> *mix, *style, *mode, *era, *delayL, *delayR, *syncL, *syncR, *spread, *ratio, *spacing,
        *tapA, *tapB, *tapC, *tapD, *quadMode, *feedback, *width, *drive, *age, *pitchMix, *res, *diffAmount,
        *diffSize, *lowCut, *highCut, *wow, *flutter, *modRate, *modDepth, *freqShift, *freqDetune, *pitchShift,
        *pitchDetune, *duck;
};

// Value <-> text helpers (exposed for tests).
juce::String formatMs (float ms);
juce::String formatHz (float hz);
float parseNumberWithUnits (const juce::String& text);

} // namespace oe::params
