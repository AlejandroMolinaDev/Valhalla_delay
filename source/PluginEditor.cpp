#include "PluginEditor.h"

#include "Parameters.h"
#include "dsp/TempoSync.h"
#include "ui/EchoVisualizer.h"
#include "ui/EraSwitch.h"
#include "ui/Knob.h"
#include "ui/ModeMenu.h"
#include "ui/PresetBar.h"
#include "ui/StyleMenu.h"
#include "ui/TapButtons.h"

using namespace oe;
using oe::ui::Knob;
namespace id = oe::params::id;
namespace colours = oe::ui::colours;

namespace
{
    // Panel geometry at the base size (900 x 480).
    const juce::Rectangle<int> kTopBar { 0, 0, 900, 46 };
    const juce::Rectangle<int> kLeft { 12, 54, 184, 262 };
    const juce::Rectangle<int> kExtra { 12, 324, 184, 144 };
    const juce::Rectangle<int> kScreen { 206, 54, 682, 112 };
    const juce::Rectangle<int> kDelay { 206, 172, 232, 146 };
    const juce::Rectangle<int> kFeedback { 444, 172, 140, 146 };
    const juce::Rectangle<int> kColor { 590, 172, 140, 146 };
    const juce::Rectangle<int> kMod { 736, 172, 152, 146 };
    const juce::Rectangle<int> kDiffusion { 206, 324, 150, 144 };
    const juce::Rectangle<int> kEq { 362, 324, 150, 144 };
    const juce::Rectangle<int> kDuck { 518, 324, 100, 144 };
    const juce::Rectangle<int> kMix { 624, 324, 264, 144 };
    const juce::Rectangle<int> kMixWide { 518, 324, 370, 144 }; // when Duck lives in "Extra"

    constexpr int kKnobW = 70;
    constexpr int kKnobH = 88;

    juce::Rectangle<int> slot (const juce::Rectangle<int>& section, int index, int count, int top = 22)
    {
        const auto inner = section.reduced (6, 0);
        const auto gap = (inner.getWidth() - count * kKnobW) / (count + 1);
        return { inner.getX() + gap + index * (kKnobW + gap), section.getY() + top, kKnobW, kKnobH };
    }

    const char* modeDescription (dsp::Mode m)
    {
        using dsp::Mode;
        switch (m)
        {
            case Mode::Tape:         return "Variable-speed tape echo. Wow, flutter and tone follow the delay time; Age adds asperity noise and splices.";
            case Mode::TapeClean:    return "Chrome tape: finer, brighter saturation, much less noise, no splices.";
            case Mode::ReelHiFi:     return "Studio reel-to-reel: full bandwidth, random wow and flutter independent of the delay time.";
            case Mode::TapeNR:       return "Tape with noise reduction: brighter and almost silent. Duck in the main panel.";
            case Mode::BBD:          return "Bucket-brigade chip: compander, dark filters, chip noise that follows the dynamics.";
            case Mode::BBDBright:    return "BBD whose brightness follows the delay time (clock rate). Duck in the main panel.";
            case Mode::AnalogPlus:   return "Modern BBD: twice the bandwidth, half the noise, thicker saturation.";
            case Mode::Digital:      return "80s floating-point rack delay. Age lowers the converter bits (50 % = 12 bits).";
            case Mode::Clean:        return "Clean digital with a program-dependent limiter in the loop: push Drive without distortion.";
            case Mode::Crystal:      return "Transparent looper: Low/High Cut act on the output only, repeats stay bright.";
            case Mode::LoFi:         return "Sample-rate reduction, aliasing and bit crushing. Age = amount of damage.";
            case Mode::GhostShift:   return "Hi-fi tape, frequency shifter and frequency-dependent diffusion (Era) inside the loop.";
            case Mode::Pitch:        return "Pitch shifter inside the loop: endless rising or falling spirals.";
            case Mode::PitchRich:    return "Pitch-shifted and detuned paths blended by Pitch Mix. With Diffusion: shimmer.";
            case Mode::PitchReverse: return "Reversed splices (splice length = Delay), pitch-shifted.";
            case Mode::PitchDuck:    return "Pitch spirals that duck under your playing.";
            case Mode::PhaserDDL:    return "Delay with a 4/6/12-stage phaser in the loop. Delay 0 + Feedback 0 = phaser alone.";
        }
        return "";
    }
} // namespace

// =============================================================================
class OrbitEchoEditor::Content final : public juce::Component
{
public:
    Content (OrbitEchoProcessor& p, juce::LookAndFeel& lnf)
        : processor (p), state (p.getState()), presetBar (p), modeMenu (state), styleMenu (state), eraSwitch (state),
          taps (state), visualizer (p),
          delayMs (state, id::delayL, "Delay", "Delay time in milliseconds (0 ms - 20 s). Type a value or drag; Ctrl/Cmd-drag for fine steps."),
          delayNote (state, id::syncL, "Delay", "Delay time as a note value, synced to the host tempo (T = triplet, D = dotted)."),
          delayRMs (state, id::delayR, "Delay R", "Right delay time in milliseconds."),
          delayRNote (state, id::syncR, "Delay R", "Right delay time as a note value, synced to the host tempo."),
          spread (state, id::spread, "Spread", "Adds this time to the left side and subtracts it from the right: widens the image."),
          ratio (state, id::ratio, "Ratio", "Right delay = Ratio x left delay. 61.8 % builds a pseudo-reverb whose density grows over time."),
          spacing (state, id::spacing, "Spacing", "Spacing between the four heads (and between L and R). 0 % = evenly spaced at 1/4, 1/2, 3/4, 4/4."),
          feedback (state, id::feedback, "Feedback", "Amount of each echo fed back. Above ~100 % the loop self-oscillates into its saturation (controlled)."),
          width (state, id::width, "Width", "Stereo width of the echoes: +100 % normal, 0 % mono, -100 % left/right swapped.", true),
          drive (state, id::drive, "Drive", "Input gain into the mode's saturation stage, with -Drive/2 dB output compensation."),
          age (state, id::age, "Age", "Physical artefacts of the current mode (noise, splices, bit depth, bandwidth, aliasing)."),
          pitchMix (state, id::pitchMix, "Pitch Mix", "Balance between the detuned (0 %) and the pitch-shifted (100 %) paths."),
          res (state, id::res, "Res", "Phaser resonance."),
          wow (state, id::wow, "Wow", "Slow random tape-speed variation; its rate follows the delay time."),
          flutter (state, id::flutter, "Flutter", "Fast random tape-speed variation; its rate follows the delay time."),
          modRate (state, id::modRate, "Mod Rate", "Modulation rate (0.01 - 10 Hz)."),
          modDepth (state, id::modDepth, "Mod Depth", "Modulation depth: chorus / vibrato on the echoes (phaser sweep in Phaser DDL)."),
          freqShift (state, id::freqShift, "Freq Shift", "Linear frequency shift in Hz inside the loop: each repeat moves further (not a musical interval).", true),
          freqDetune (state, id::freqDetune, "Freq Detune", "Shift offset between left and right: small values widen, large values pan quickly."),
          pitchShift (state, id::pitchShift, "Pitch", "Pitch shift inside the loop in semitones (+-24). Shift-drag or Ctrl/Cmd-drag for cents.", true),
          pitchDetune (state, id::pitchDetune, "Detune", "Left / right detune in cents (micro-shift doubling)."),
          diffAmount (state, id::diffAmount, "Amount", "Diffusion inside the loop: 68 % makes bell-shaped echoes, 91 % a dense reverb even with Feedback at 0. 0 % = off."),
          diffSize (state, id::diffSize, "Size", "Dispersion time relative to the delay: low values blur transients, high values become reverberant."),
          lowCut (state, id::lowCut, "Low Cut", "High-pass inside the loop (10 Hz - 2 kHz): each repeat gets thinner."),
          highCut (state, id::highCut, "High Cut", "Low-pass inside the loop (200 Hz - 20 kHz): each repeat gets darker."),
          duckMain (state, id::duck, "Duck", "Lowers the echoes (and part of the feedback) while you play; they swell back in the gaps."),
          duckExtra (state, id::duck, "Duck", "Lowers the echoes (and part of the feedback) while you play; they swell back in the gaps."),
          mix (state, id::mix, "Mix", "Dry / wet balance (equal power).")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &modeMenu, &styleMenu, &eraSwitch, &taps, &visualizer, &modeInfo,
                                                                 &syncL, &syncR, &mixLock })
            addAndMakeVisible (c);
        for (auto* k : allKnobs())
            addAndMakeVisible (k);

        pitchShift.getSlider().setCoarseSnap (1.0); // semitone steps unless fine-dragging

        modeInfo.setJustificationType (juce::Justification::topLeft);
        modeInfo.setColour (juce::Label::textColourId, colours::textDim);
        modeInfo.setMinimumHorizontalScale (1.0f);

        setupSyncButton (syncL, syncLAttachment, id::syncL, rememberedNoteL);
        setupSyncButton (syncR, syncRAttachment, id::syncR, rememberedNoteR);

        mixLock.setClickingTogglesState (false);
        mixLock.setTooltip ("Mix Lock: loading presets keeps the current Mix (saved with the session, not in presets).");
        mixLock.onClick = [this] { processor.mixLock = ! processor.mixLock.load(); refresh (true); };

        // Set explicitly once every child exists, so all of them (and the slider
        // value boxes they create) use the panel's look and feel from the start.
        setLookAndFeel (&lnf);
        refresh (false);
    }

    ~Content() override { setLookAndFeel (nullptr); }

    std::vector<Knob*> allKnobs()
    {
        return { &delayMs, &delayNote, &delayRMs, &delayRNote, &spread, &ratio, &spacing, &feedback, &width, &drive,
                 &age, &pitchMix, &res, &wow, &flutter, &modRate, &modDepth, &freqShift, &freqDetune, &pitchShift,
                 &pitchDetune, &diffAmount, &diffSize, &lowCut, &highCut, &duckMain, &duckExtra, &mix };
    }

    void setupSyncButton (juce::TextButton& b, std::unique_ptr<juce::ParameterAttachment>& att, const char* paramId, int& remembered)
    {
        b.setButtonText ("Sync");
        b.setTooltip ("Sync the delay to the host tempo (note values) or set it freely in milliseconds.");
        att = std::make_unique<juce::ParameterAttachment> (*state.getParameter (paramId), [&b] (float v)
                                                           { b.setToggleState (juce::roundToInt (v) != dsp::kSyncFree, juce::dontSendNotification); });
        att->sendInitialUpdate();
        b.onClick = [this, &att, paramId, &remembered]
        {
            const auto current = juce::roundToInt (state.getRawParameterValue (paramId)->load());
            if (current != dsp::kSyncFree)
            {
                remembered = current;
                att->setValueAsCompleteGesture (static_cast<float> (dsp::kSyncFree));
            }
            else
            {
                att->setValueAsCompleteGesture (static_cast<float> (remembered));
            }
            refresh (true);
        };
    }

    void resized() override
    {
        presetBar.setBounds (kTopBar.reduced (12, 0));

        modeMenu.setBounds (kLeft.getX(), kLeft.getY() + 18, kLeft.getWidth(), 34);
        styleMenu.setBounds (kLeft.getX(), kLeft.getY() + 76, kLeft.getWidth(), 28);
        eraSwitch.setBounds (kLeft.getX(), kLeft.getY() + 128, kLeft.getWidth(), 26);
        taps.setBounds (kLeft.getX(), kLeft.getY() + 178, kLeft.getWidth(), 54);
        modeInfo.setBounds (kLeft.getX() - 2, kLeft.getY() + 170, kLeft.getWidth() + 4, 88);

        visualizer.setBounds (kScreen);

        // Delay section: slot 0 = Delay (L), slot 1 = Spread / Ratio / Spacing / Delay R.
        const auto d0 = slot (kDelay, 0, 2), d1 = slot (kDelay, 1, 2);
        delayMs.setBounds (d0);
        delayNote.setBounds (d0);
        for (auto* k : { &spread, &ratio, &spacing, &delayRMs, &delayRNote })
            k->setBounds (d1);
        syncL.setBounds (d0.getX() + 7, d0.getBottom() + 2, kKnobW - 14, 20);
        syncR.setBounds (d1.getX() + 7, d1.getBottom() + 2, kKnobW - 14, 20);

        feedback.setBounds (slot (kFeedback, 0, 2));
        width.setBounds (slot (kFeedback, 1, 2));
        drive.setBounds (slot (kColor, 0, 2));
        for (auto* k : { &age, &pitchMix, &res })
            k->setBounds (slot (kColor, 1, 2));
        for (auto* k : { &wow, &modRate, &freqShift, &pitchShift })
            k->setBounds (slot (kMod, 0, 2));
        for (auto* k : { &flutter, &modDepth, &freqDetune, &pitchDetune })
            k->setBounds (slot (kMod, 1, 2));

        diffAmount.setBounds (slot (kDiffusion, 0, 2, 24));
        diffSize.setBounds (slot (kDiffusion, 1, 2, 24));
        lowCut.setBounds (slot (kEq, 0, 2, 24));
        highCut.setBounds (slot (kEq, 1, 2, 24));
        duckMain.setBounds (slot (kDuck, 0, 1, 24));
        duckExtra.setBounds (slot (kExtra, 0, 1, 24).translated (0, 0));

        const auto mixArea = duckInMain ? kMix : kMixWide;
        const auto mixKnob = juce::Rectangle<int> (mixArea.getCentreX() - 95, mixArea.getY() + 20, 110, 118);
        mix.setBounds (mixKnob);
        mixLock.setBounds (mixKnob.getRight() + 14, kMix.getCentreY() - 12, 84, 24);
    }

    void paint (juce::Graphics& g) override
    {
        auto* lf = dynamic_cast<ui::OrbitLookAndFeel*> (&getLookAndFeel());
        auto font = [lf] (float h, bool b) { return lf != nullptr ? lf->font (h, b) : juce::Font (juce::FontOptions (h)); };

        g.setGradientFill (juce::ColourGradient (colours::backgroundTop, 0.0f, 0.0f, colours::background, 0.0f, 480.0f, false));
        g.fillAll();
        // Brushed texture: very faint horizontal lines.
        g.setColour (juce::Colours::white.withAlpha (0.012f));
        for (int y = 0; y < 480; y += 3)
            g.drawHorizontalLine (y, 0.0f, 900.0f);

        // Top bar.
        g.setColour (juce::Colours::black.withAlpha (0.25f));
        g.fillRect (kTopBar);
        g.setColour (colours::copper.withAlpha (0.55f));
        g.fillRect (0, kTopBar.getBottom() - 2, 900, 1);

        // Left column labels.
        g.setFont (font (11.0f, true));
        g.setColour (colours::copper);
        g.drawText ("MODE", kLeft.getX(), kLeft.getY(), 120, 16, juce::Justification::centredLeft);
        g.drawText ("STYLE", kLeft.getX(), kLeft.getY() + 60, 120, 16, juce::Justification::centredLeft);
        g.drawText ("ERA", kLeft.getX(), kLeft.getY() + 112, 120, 16, juce::Justification::centredLeft);
        if (style == dsp::Style::Quad)
            g.drawText ("HEADS", kLeft.getX(), kLeft.getY() + 162, 120, 16, juce::Justification::centredLeft);

        auto section = [&] (const juce::Rectangle<int>& r, const juce::String& title, bool highlighted)
        {
            const auto rf = r.toFloat();
            g.setGradientFill (juce::ColourGradient (colours::panel.brighter (0.04f), 0.0f, rf.getY(), colours::panel.darker (0.12f), 0.0f, rf.getBottom(), false));
            g.fillRoundedRectangle (rf, 7.0f);
            g.setColour (colours::panelEdge);
            g.drawRoundedRectangle (rf.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.drawHorizontalLine (r.getY() + 1, rf.getX() + 6.0f, rf.getRight() - 6.0f);
            g.setFont (font (11.0f, true));
            g.setColour (highlighted ? colours::amber : colours::copper);
            g.drawText (title, r.getX() + 10, r.getY() + 4, r.getWidth() - 20, 15, juce::Justification::centredLeft);
        };

        section (kDelay, "DELAY", false);
        section (kFeedback, "FEEDBACK / WIDTH", false);
        section (kColor, "COLOR", false);
        section (kMod, modTitle, false);
        section (kDiffusion, "DIFFUSION", false);
        section (kEq, "EQ", false);
        if (duckInMain)
            section (kDuck, "DUCK", true);
        section (duckInMain ? kMix : kMixWide, "MIX", false);
        section (kExtra, "EXTRA", false);
        if (duckInMain)
        {
            g.setFont (font (11.5f, false));
            g.setColour (colours::textDim);
            g.drawFittedText ("Duck is in the main panel for this mode.", kExtra.reduced (14, 30), juce::Justification::centred, 3);
        }
    }

    /** Re-reads Mode / Style / sync / Mix Lock and updates what is shown. */
    void refresh (bool animate)
    {
        const auto newMode = static_cast<dsp::Mode> (juce::roundToInt (state.getRawParameterValue (id::mode)->load()));
        const auto newStyle = static_cast<dsp::Style> (juce::roundToInt (state.getRawParameterValue (id::style)->load()));
        const auto syncOnL = juce::roundToInt (state.getRawParameterValue (id::syncL)->load()) != dsp::kSyncFree;
        const auto syncOnR = juce::roundToInt (state.getRawParameterValue (id::syncR)->load()) != dsp::kSyncFree;
        const auto changed = newMode != mode || newStyle != style || syncOnL != lastSyncL || syncOnR != lastSyncR || ! initialised;
        mixLock.setToggleState (processor.mixLock.load(), juce::dontSendNotification);
        if (! changed)
            return;

        mode = newMode;
        style = newStyle;
        lastSyncL = syncOnL;
        lastSyncR = syncOnR;
        initialised = true;
        duckInMain = params::duckIsInMainSection (mode);

        const auto twoTimes = style == dsp::Style::Dual || style == dsp::Style::PingPong;
        const auto quad = style == dsp::Style::Quad;
        delayMs.setTitle (twoTimes ? "Delay L" : "Delay");
        delayNote.setTitle (twoTimes ? "Delay L" : "Delay");
        modeInfo.setText (modeDescription (mode), juce::dontSendNotification);
        modeInfo.setFont (juce::Font (juce::FontOptions (12.5f)));

        const auto controls = dsp::modeControls (mode);
        modTitle = (controls & dsp::ctl::Wow) != 0         ? "MOD / TRANSPORT"
                 : (controls & dsp::ctl::FreqShift) != 0   ? "MOD / FREQ SHIFT"
                 : (controls & dsp::ctl::PitchShift) != 0  ? "MOD / PITCH"
                 : mode == dsp::Mode::PhaserDDL            ? "MOD / PHASER LFO"
                                                           : "MOD";

        show (delayMs, ! syncOnL, animate);
        show (delayNote, syncOnL, animate);
        show (delayRMs, twoTimes && ! syncOnR, animate);
        show (delayRNote, twoTimes && syncOnR, animate);
        show (syncR, twoTimes, animate);
        show (spread, style == dsp::Style::Single, animate);
        show (ratio, style == dsp::Style::Ratio, animate);
        show (spacing, quad, animate);
        show (taps, quad, animate);
        show (modeInfo, ! quad, animate);

        for (auto* k : { &age, &pitchMix, &res, &wow, &flutter, &modRate, &modDepth, &freqShift, &freqDetune, &pitchShift, &pitchDetune })
            show (*k, params::isVisible (k->getParamId(), mode, style), animate);

        show (duckMain, duckInMain, animate);
        show (duckExtra, ! duckInMain, animate);
        resized(); // the Mix section widens when Duck is not in the main panel
        repaint();
    }

    bool isShown (const juce::String& paramId)
    {
        for (auto* k : allKnobs())
            if (k->getParamId() == paramId && k->isVisible())
                return true;
        if (paramId == id::tapA || paramId == id::tapB || paramId == id::tapC || paramId == id::tapD || paramId == id::quadMode)
            return taps.isVisible();
        if (paramId == id::mode)
            return modeMenu.isVisible();
        if (paramId == id::style)
            return styleMenu.isVisible();
        if (paramId == id::era)
            return eraSwitch.isVisible();
        return false;
    }

private:
    void show (juce::Component& c, bool visible, bool animate)
    {
        if (c.isVisible() == visible)
            return;
        auto& animator = juce::Desktop::getInstance().getAnimator();
        if (animate && isShowing())
        {
            if (visible)
                animator.fadeIn (&c, 180);
            else
                animator.fadeOut (&c, 140);
        }
        else
        {
            c.setAlpha (1.0f);
            c.setVisible (visible);
        }
    }

    OrbitEchoProcessor& processor;
    juce::AudioProcessorValueTreeState& state;

    ui::PresetBar presetBar;
    ui::ModeMenu modeMenu;
    ui::StyleMenu styleMenu;
    ui::EraSwitch eraSwitch;
    ui::TapButtons taps;
    ui::EchoVisualizer visualizer;
    juce::Label modeInfo;

    Knob delayMs, delayNote, delayRMs, delayRNote, spread, ratio, spacing, feedback, width, drive, age, pitchMix, res,
        wow, flutter, modRate, modDepth, freqShift, freqDetune, pitchShift, pitchDetune, diffAmount, diffSize, lowCut,
        highCut, duckMain, duckExtra, mix;

    juce::TextButton syncL, syncR, mixLock { "Mix Lock" };
    std::unique_ptr<juce::ParameterAttachment> syncLAttachment, syncRAttachment;
    int rememberedNoteL = dsp::kSyncQuarter, rememberedNoteR = dsp::kSyncQuarter;

    dsp::Mode mode = dsp::Mode::Tape;
    dsp::Style style = dsp::Style::Single;
    bool lastSyncL = false, lastSyncR = false, initialised = false, duckInMain = false;
    juce::String modTitle { "MOD" };
};

// =============================================================================
OrbitEchoEditor::OrbitEchoEditor (OrbitEchoProcessor& p) : AudioProcessorEditor (&p), orbitProcessor (p)
{
    // Read the saved size first: setting the limits below triggers resized().
    const auto savedWidth = juce::jlimit (kBaseWidth * 3 / 4, kBaseWidth * 2, orbitProcessor.editorWidth.load());

    setLookAndFeel (&lookAndFeel);
    tooltips.setLookAndFeel (&lookAndFeel);
    content = std::make_unique<Content> (p, lookAndFeel);
    addAndMakeVisible (*content);

    setResizable (true, true);
    setResizeLimits (kBaseWidth * 3 / 4, kBaseHeight * 3 / 4, kBaseWidth * 2, kBaseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio (static_cast<double> (kBaseWidth) / static_cast<double> (kBaseHeight));

    setSize (savedWidth, juce::roundToInt (static_cast<double> (savedWidth) * kBaseHeight / kBaseWidth));
    constructed = true;
    startTimerHz (15);
}

OrbitEchoEditor::~OrbitEchoEditor()
{
    tooltips.setLookAndFeel (nullptr);
    setLookAndFeel (nullptr);
}

void OrbitEchoEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);
}

void OrbitEchoEditor::resized()
{
    const auto scale = static_cast<float> (getWidth()) / static_cast<float> (kBaseWidth);
    content->setTransform (juce::AffineTransform::scale (scale));
    content->setBounds (0, 0, kBaseWidth, kBaseHeight);
    if (constructed)
        orbitProcessor.editorWidth = getWidth();
}

void OrbitEchoEditor::timerCallback()
{
    content->refresh (true);
}

void OrbitEchoEditor::updateVisibility (bool animate)
{
    content->refresh (animate);
}

bool OrbitEchoEditor::isControlShown (const juce::String& paramId) const
{
    return content->isShown (paramId);
}
