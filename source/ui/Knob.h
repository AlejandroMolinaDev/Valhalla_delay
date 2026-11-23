#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace oe::ui
{

/** Rotary slider used for every knob of the panel.

    - vertical drag; Ctrl / Cmd + drag = fine (6x slower); Shift + drag is
      also fine on knobs that snap (Pitch Shift snaps to semitones unless
      Shift / Ctrl / Cmd is held);
    - double click = default value, mouse wheel, click on the value to type a
      number (units accepted: "350 ms", "1.2 s", "8k", "-6 dB"...);
    - tooltip explaining what the control does. */
class KnobSlider final : public juce::Slider
{
public:
    KnobSlider();

    /** Coarse snapping interval (0 = none); fine drag bypasses it. */
    void setCoarseSnap (double interval) noexcept { coarseSnap = interval; }

    void mouseDown (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    double snapValue (double attemptedValue, DragMode) override;

private:
    double coarseSnap = 0.0;
    bool fineDrag = false;
};

/** A titled knob: label above, flat knob, value below. */
class Knob final : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState& state, const juce::String& paramId, const juce::String& title,
          const juce::String& tooltip, bool bipolar = false);

    void resized() override;
    void paint (juce::Graphics&) override;

    KnobSlider& getSlider() noexcept { return slider; }
    const juce::String& getParamId() const noexcept { return paramId; }
    void setTitle (const juce::String& t);

    /** Flat colour of the knob (usually its section colour). */
    void setKnobColour (juce::Colour c);

    /** The large ringed main knob (Delay). */
    void setHero (bool shouldBeHero);

    /** Dimmed title (e.g. Duck when it lives in the "extra" slot). */
    void setTitleDimmed (bool dimmed);

    /** Larger upper-case title (left column knobs). */
    void setProminentTitle (bool prominent);

private:
    juce::String paramId, title;
    KnobSlider slider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    bool hero = false, titleDimmed = false, prominent = false;
};

} // namespace oe::ui
