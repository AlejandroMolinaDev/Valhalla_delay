#include "Parameters.h"

#include <cmath>

#include "dsp/TempoSync.h"

namespace oe::params
{

namespace
{
    using Range = juce::NormalisableRange<float>;
    using Float = juce::AudioParameterFloat;
    using Choice = juce::AudioParameterChoice;
    using Bool = juce::AudioParameterBool;
    using Attr = juce::AudioParameterFloatAttributes;

    juce::ParameterID pid (const char* name) { return { name, kVersion }; }

    // ---- Ranges ------------------------------------------------------------
    /** 0..20000 ms on an exponential curve that still reaches 0 (centre ~= 400 ms). */
    Range delayRange()
    {
        constexpr float maxMs = 20000.0f;
        constexpr float k = 7.78f;
        const auto denom = std::exp (k) - 1.0f;
        return Range (
            0.0f, maxMs,
            [=] (float, float, float x) { return maxMs * (std::exp (k * x) - 1.0f) / denom; },
            [=] (float, float, float v) { return std::log1p (juce::jlimit (0.0f, maxMs, v) * denom / maxMs) / k; },
            [] (float, float, float v) { return v < 100.0f ? std::round (v * 100.0f) / 100.0f : std::round (v * 10.0f) / 10.0f; });
    }

    /** Logarithmic range between lo and hi (both > 0). */
    Range logRange (float lo, float hi, float snapStep = 0.0f)
    {
        const auto ratio = hi / lo;
        return Range (
            lo, hi,
            [=] (float, float, float x) { return lo * std::pow (ratio, x); },
            [=] (float, float, float v) { return std::log (juce::jlimit (lo, hi, v) / lo) / std::log (ratio); },
            [=] (float, float, float v)
            {
                v = juce::jlimit (lo, hi, v);
                return snapStep > 0.0f ? std::round (v / snapStep) * snapStep : v;
            });
    }

    /** -1000..+1000 Hz, cubic so that the region around 0 Hz gets most of the travel. */
    Range freqShiftRange()
    {
        constexpr float maxHz = 1000.0f;
        return Range (
            -maxHz, maxHz,
            [=] (float, float, float x)
            {
                const auto u = 2.0f * x - 1.0f;
                return maxHz * u * u * u;
            },
            [=] (float, float, float v)
            {
                const auto u = std::cbrt (juce::jlimit (-maxHz, maxHz, v) / maxHz);
                return 0.5f * (u + 1.0f);
            },
            [] (float, float, float v) { return std::round (v * 100.0f) / 100.0f; });
    }

    // ---- Text --------------------------------------------------------------
    auto percentText (int decimals = 0)
    {
        return [decimals] (float v, int) { return juce::String (v, decimals) + " %"; };
    }

    auto plainParse()
    {
        return [] (const juce::String& t) { return parseNumberWithUnits (t); };
    }

    juce::StringArray syncChoices()
    {
        juce::StringArray s;
        for (const auto& n : dsp::kSyncNotes)
            s.add (n.name);
        return s;
    }

    juce::StringArray toStringArray (const auto& names)
    {
        juce::StringArray s;
        for (const auto* n : names)
            s.add (n);
        return s;
    }

    std::unique_ptr<Float> percent (const char* name, const char* label, float lo, float hi, float def)
    {
        return std::make_unique<Float> (pid (name), label, Range (lo, hi, 0.1f), def,
                                        Attr().withLabel ("%").withStringFromValueFunction (percentText())
                                              .withValueFromStringFunction (plainParse()));
    }
} // namespace

// ---------------------------------------------------------------------------
juce::String formatMs (float ms)
{
    if (ms < 100.0f)
        return juce::String (ms, 2) + " ms";
    if (ms < 1000.0f)
        return juce::String (ms, 1) + " ms";
    return juce::String (juce::roundToInt (ms)) + " ms";
}

juce::String formatHz (float hz)
{
    if (hz < 1.0f)
        return juce::String (hz, 3) + " Hz";
    if (hz < 10.0f)
        return juce::String (hz, 2) + " Hz";
    if (hz < 1000.0f)
        return juce::String (juce::roundToInt (hz)) + " Hz";
    return juce::String (hz / 1000.0f, 2) + " kHz";
}

float parseNumberWithUnits (const juce::String& text)
{
    const auto t = text.trim().toLowerCase();
    auto value = t.getFloatValue();
    if (t.endsWith ("khz") || t.endsWith ("k"))
        value *= 1000.0f;
    else if (t.endsWith ("s") && ! t.endsWith ("ms"))
        value *= 1000.0f; // seconds typed into a time field
    return value;
}

// ---------------------------------------------------------------------------
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    const auto msAttr = Attr().withLabel ("ms")
                            .withStringFromValueFunction ([] (float v, int) { return formatMs (v); })
                            .withValueFromStringFunction (plainParse());
    const auto hzAttr = Attr().withLabel ("Hz")
                            .withStringFromValueFunction ([] (float v, int) { return formatHz (v); })
                            .withValueFromStringFunction (plainParse());

    layout.add (percent (id::mix, "Mix", 0.0f, 100.0f, 30.0f));
    layout.add (std::make_unique<Choice> (pid (id::style), "Style", toStringArray (dsp::kStyleNames), 0));
    layout.add (std::make_unique<Choice> (pid (id::mode), "Mode", toStringArray (dsp::kModeNames), 0));
    layout.add (std::make_unique<Choice> (pid (id::era), "Era", toStringArray (dsp::kEraNames), 1));

    layout.add (std::make_unique<Float> (pid (id::delayL), "Delay", delayRange(), 350.0f, msAttr));
    layout.add (std::make_unique<Float> (pid (id::delayR), "Delay R", delayRange(), 500.0f, msAttr));
    layout.add (std::make_unique<Choice> (pid (id::syncL), "Delay Sync", syncChoices(), dsp::kSyncQuarter));
    layout.add (std::make_unique<Choice> (pid (id::syncR), "Delay Sync R", syncChoices(), dsp::kSyncQuarter));
    layout.add (std::make_unique<Float> (pid (id::spread), "Spread", Range (0.0f, 100.0f, 0.01f), 0.0f, msAttr));
    layout.add (percent (id::ratio, "Ratio", 1.0f, 100.0f, 61.8f));
    layout.add (percent (id::spacing, "Spacing", 0.0f, 100.0f, 0.0f));
    layout.add (std::make_unique<Bool> (pid (id::tapA), "Tap A", true));
    layout.add (std::make_unique<Bool> (pid (id::tapB), "Tap B", true));
    layout.add (std::make_unique<Bool> (pid (id::tapC), "Tap C", true));
    layout.add (std::make_unique<Bool> (pid (id::tapD), "Tap D", true));
    layout.add (std::make_unique<Choice> (pid (id::quadMode), "Repeat/Swell", juce::StringArray { "Repeat", "Swell" }, 0));

    layout.add (percent (id::feedback, "Feedback", 0.0f, 200.0f, 35.0f));
    layout.add (percent (id::width, "Width", -100.0f, 100.0f, 100.0f));
    layout.add (std::make_unique<Float> (pid (id::drive), "Drive", Range (0.0f, 24.0f, 0.01f), 0.0f,
                                         Attr().withLabel ("dB")
                                             .withStringFromValueFunction ([] (float v, int) { return (v > 0.0f ? "+" : "") + juce::String (v, 1) + " dB"; })
                                             .withValueFromStringFunction (plainParse())));
    layout.add (percent (id::age, "Age", 0.0f, 100.0f, 20.0f));
    layout.add (percent (id::pitchMix, "Pitch Mix", 0.0f, 100.0f, 50.0f));
    layout.add (percent (id::res, "Res", 0.0f, 100.0f, 30.0f));
    layout.add (percent (id::diffAmount, "Diffusion", 0.0f, 100.0f, 0.0f));
    layout.add (percent (id::diffSize, "Diffusion Size", 0.0f, 100.0f, 50.0f));
    layout.add (std::make_unique<Float> (pid (id::lowCut), "Low Cut", logRange (10.0f, 2000.0f), 40.0f, hzAttr));
    layout.add (std::make_unique<Float> (pid (id::highCut), "High Cut", logRange (200.0f, 20000.0f), 8000.0f, hzAttr));
    layout.add (percent (id::wow, "Wow", 0.0f, 100.0f, 25.0f));
    layout.add (percent (id::flutter, "Flutter", 0.0f, 100.0f, 15.0f));
    layout.add (std::make_unique<Float> (pid (id::modRate), "Mod Rate", logRange (0.01f, 10.0f), 0.5f, hzAttr));
    layout.add (percent (id::modDepth, "Mod Depth", 0.0f, 100.0f, 10.0f));
    layout.add (std::make_unique<Float> (pid (id::freqShift), "Freq Shift", freqShiftRange(), 5.0f,
                                         Attr().withLabel ("Hz")
                                             .withStringFromValueFunction ([] (float v, int)
                                                                           { return (v > 0.0f ? "+" : "") + juce::String (v, std::abs (v) < 10.0f ? 2 : 1) + " Hz"; })
                                             .withValueFromStringFunction (plainParse())));
    layout.add (std::make_unique<Float> (pid (id::freqDetune), "Freq Detune", Range (0.0f, 50.0f, 0.01f), 1.0f,
                                         Attr().withLabel ("Hz")
                                             .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 2) + " Hz"; })
                                             .withValueFromStringFunction (plainParse())));
    layout.add (std::make_unique<Float> (pid (id::pitchShift), "Pitch Shift", Range (-24.0f, 24.0f, 0.01f), 12.0f,
                                         Attr().withLabel ("st")
                                             .withStringFromValueFunction ([] (float v, int)
                                                                           {
                                                                               const auto whole = std::abs (v - std::round (v)) < 0.005f;
                                                                               return (v > 0.0f ? "+" : "") + juce::String (v, whole ? 0 : 2) + " st";
                                                                           })
                                             .withValueFromStringFunction (plainParse())));
    layout.add (std::make_unique<Float> (pid (id::pitchDetune), "Pitch Detune", Range (0.0f, 100.0f, 0.1f), 10.0f,
                                         Attr().withLabel ("ct")
                                             .withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1) + " ct"; })
                                             .withValueFromStringFunction (plainParse())));
    layout.add (percent (id::duck, "Duck", 0.0f, 100.0f, 0.0f));

    return layout;
}

const std::vector<juce::String>& allIds()
{
    static const std::vector<juce::String> ids {
        id::mix, id::style, id::mode, id::era, id::delayL, id::delayR, id::syncL, id::syncR, id::spread,
        id::ratio, id::spacing, id::tapA, id::tapB, id::tapC, id::tapD, id::quadMode, id::feedback, id::width,
        id::drive, id::age, id::pitchMix, id::res, id::diffAmount, id::diffSize, id::lowCut, id::highCut,
        id::wow, id::flutter, id::modRate, id::modDepth, id::freqShift, id::freqDetune, id::pitchShift,
        id::pitchDetune, id::duck
    };
    return ids;
}

bool duckIsInMainSection (dsp::Mode mode)
{
    return (dsp::modeControls (mode) & dsp::ctl::DuckMain) != 0;
}

bool isVisible (const juce::String& p, dsp::Mode mode, dsp::Style style)
{
    using dsp::Style;
    const auto c = dsp::modeControls (mode);
    const auto has = [c] (unsigned flag) { return (c & flag) != 0; };
    const auto twoTimes = style == Style::Dual || style == Style::PingPong;

    if (p == id::delayR || p == id::syncR)
        return twoTimes;
    if (p == id::spread)
        return style == Style::Single;
    if (p == id::ratio)
        return style == Style::Ratio;
    if (p == id::spacing || p == id::tapA || p == id::tapB || p == id::tapC || p == id::tapD || p == id::quadMode)
        return style == Style::Quad;

    if (p == id::age)
        return has (dsp::ctl::Age);
    if (p == id::pitchMix)
        return has (dsp::ctl::PitchMix);
    if (p == id::res)
        return has (dsp::ctl::Res);
    if (p == id::wow)
        return has (dsp::ctl::Wow);
    if (p == id::flutter)
        return has (dsp::ctl::Flutter);
    if (p == id::modRate)
        return has (dsp::ctl::ModRate);
    if (p == id::modDepth)
        return has (dsp::ctl::ModDepth);
    if (p == id::freqShift)
        return has (dsp::ctl::FreqShift);
    if (p == id::freqDetune)
        return has (dsp::ctl::FreqDetune);
    if (p == id::pitchShift)
        return has (dsp::ctl::PitchShift);
    if (p == id::pitchDetune)
        return has (dsp::ctl::PitchDetune);

    return true; // mix, style, mode, era, delayL, syncL, feedback, width, drive, diffusion, EQ, duck
}

// ---------------------------------------------------------------------------
std::atomic<float>* Refs::get (juce::AudioProcessorValueTreeState& apvts, const char* name)
{
    auto* p = apvts.getRawParameterValue (name);
    jassert (p != nullptr);
    return p;
}

Refs::Refs (juce::AudioProcessorValueTreeState& s)
    : mix (get (s, id::mix)), style (get (s, id::style)), mode (get (s, id::mode)), era (get (s, id::era)),
      delayL (get (s, id::delayL)), delayR (get (s, id::delayR)), syncL (get (s, id::syncL)), syncR (get (s, id::syncR)),
      spread (get (s, id::spread)), ratio (get (s, id::ratio)), spacing (get (s, id::spacing)), tapA (get (s, id::tapA)),
      tapB (get (s, id::tapB)), tapC (get (s, id::tapC)), tapD (get (s, id::tapD)), quadMode (get (s, id::quadMode)),
      feedback (get (s, id::feedback)), width (get (s, id::width)), drive (get (s, id::drive)), age (get (s, id::age)),
      pitchMix (get (s, id::pitchMix)), res (get (s, id::res)), diffAmount (get (s, id::diffAmount)),
      diffSize (get (s, id::diffSize)), lowCut (get (s, id::lowCut)), highCut (get (s, id::highCut)), wow (get (s, id::wow)),
      flutter (get (s, id::flutter)), modRate (get (s, id::modRate)), modDepth (get (s, id::modDepth)),
      freqShift (get (s, id::freqShift)), freqDetune (get (s, id::freqDetune)), pitchShift (get (s, id::pitchShift)),
      pitchDetune (get (s, id::pitchDetune)), duck (get (s, id::duck))
{
}

dsp::EngineParams Refs::load (double bpm) const noexcept
{
    const auto pct = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed) * 0.01f; };
    const auto val = [] (const std::atomic<float>* a) { return a->load (std::memory_order_relaxed); };
    const auto idx = [] (const std::atomic<float>* a, int count)
    { return juce::jlimit (0, count - 1, juce::roundToInt (a->load (std::memory_order_relaxed))); };

    dsp::EngineParams p;
    p.mix = pct (mix);
    p.style = static_cast<dsp::Style> (idx (style, dsp::kNumStyles));
    p.mode = static_cast<dsp::Mode> (idx (mode, dsp::kNumModes));
    p.era = static_cast<dsp::Era> (idx (era, dsp::kNumEras));
    p.delayLMs = val (delayL);
    p.delayRMs = val (delayR);
    p.syncL = idx (syncL, static_cast<int> (dsp::kSyncNotes.size()));
    p.syncR = idx (syncR, static_cast<int> (dsp::kSyncNotes.size()));
    p.bpm = dsp::sanitiseBpm (bpm);
    p.spreadMs = val (spread);
    p.ratio = pct (ratio);
    p.spacing = pct (spacing);
    p.taps = { val (tapA) > 0.5f, val (tapB) > 0.5f, val (tapC) > 0.5f, val (tapD) > 0.5f };
    p.quadFeedback = idx (quadMode, 2) == 1 ? dsp::QuadFeedback::Swell : dsp::QuadFeedback::Repeat;
    p.feedback = pct (feedback);
    p.width = pct (width);
    p.driveDb = val (drive);
    p.age = pct (age);
    p.pitchMix = pct (pitchMix);
    p.res = pct (res);
    p.diffAmount = pct (diffAmount);
    p.diffSize = pct (diffSize);
    p.lowCutHz = val (lowCut);
    p.highCutHz = val (highCut);
    p.wow = pct (wow);
    p.flutter = pct (flutter);
    p.modRateHz = val (modRate);
    p.modDepth = pct (modDepth);
    p.freqShiftHz = val (freqShift);
    p.freqDetuneHz = val (freqDetune);
    p.pitchShiftSt = val (pitchShift);
    p.pitchDetuneCents = val (pitchDetune);
    p.duck = pct (duck);
    return p;
}

} // namespace oe::params
