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
using Weight = oe::ui::OrbitLookAndFeel::Weight;
namespace id = oe::params::id;
namespace colours = oe::ui::colours;

namespace
{
    // Geometry at the base size (1120 x 448).
    constexpr int kSectionTop = 104;
    constexpr int kSectionHeight = 268;
    constexpr int kSectionWidth = 114;
    constexpr int kSectionGap = 8;
    constexpr int kFirstSectionX = 376;
    constexpr int kKnobHeight = 112; // title + knob + value

    const juce::Rectangle<int> kLeftColumn { 20, 102, 112, 270 };
    const juce::Rectangle<int> kHero { 140, 98, 224, 266 };
    const juce::Rectangle<int> kVisualiser { 372, 16, 470, 72 };
    const juce::Rectangle<int> kBottomBar { 20, 392, 1080, 44 };

    enum Section
    {
        timeSection,
        feedbackSection,
        colorSection,
        modSection,
        diffSection,
        eqSection,
        numSections
    };

    juce::Rectangle<int> sectionBounds (int index)
    {
        return { kFirstSectionX + index * (kSectionWidth + kSectionGap), kSectionTop, kSectionWidth, kSectionHeight };
    }

    juce::Colour sectionColour (int index)
    {
        static const juce::Colour* const palette[] { &colours::amberLight, &colours::amber, &colours::orange,
                                                     &colours::orangeRed, &colours::red, &colours::crimson };
        return *palette[index];
    }

    const char* sectionTitle (int index)
    {
        static const char* const titles[] { "TIME", "FEEDBACK", "COLOR", "MOD", "DIFF", "EQ" };
        return titles[index];
    }

    /** Upper / lower knob slot of a stacked section. */
    juce::Rectangle<int> slot (int section, int row)
    {
        const auto s = sectionBounds (section);
        return { s.getX() + 2, s.getY() + 38 + row * (kKnobHeight + 4), s.getWidth() - 4, kKnobHeight };
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

    const char* styleDescription (dsp::Style s)
    {
        switch (s)
        {
            case dsp::Style::Single:   return "One delay for both sides. Spread pulls L and R apart.";
            case dsp::Style::Ratio:    return "R = Ratio x L. 61.8 % grows into a pseudo-reverb.";
            case dsp::Style::Dual:     return "Two independent delays, no cross feedback.";
            case dsp::Style::PingPong: return "L feeds R, R feeds L.";
            case dsp::Style::Quad:     return "Four tape heads A-D.";
        }
        return "";
    }

    /** Small padlock toggle next to the Mix knob (Mix Lock). */
    class LockButton final : public juce::Button
    {
    public:
        LockButton() : juce::Button ("Mix Lock") { setClickingTogglesState (false); }

        void paintButton (juce::Graphics& g, bool highlighted, bool) override
        {
            const auto r = getLocalBounds().toFloat().reduced (2.0f);
            const auto on = getToggleState();
            const auto colour = on ? colours::accent : (highlighted ? colours::textDim.brighter (0.3f) : colours::textDim);
            const auto body = r.withTrimmedTop (r.getHeight() * 0.45f);
            juce::Path shackle;
            const auto sw = body.getWidth() * 0.62f;
            const auto sx = body.getCentreX() - sw * 0.5f;
            shackle.addRoundedRectangle (sx, r.getY(), sw, r.getHeight() * 0.75f, sw * 0.5f);
            g.setColour (colour);
            g.strokePath (shackle, juce::PathStrokeType (2.2f));
            g.fillRoundedRectangle (body, 2.5f);
            if (! on)
            {
                // Open lock: gap in the shackle.
                g.setColour (colours::background);
                g.fillRect (juce::Rectangle<float> (sx + sw - 3.0f, r.getY() + r.getHeight() * 0.3f, 4.0f, r.getHeight() * 0.2f));
            }
        }
    };
} // namespace

// =============================================================================
class OrbitEchoEditor::Content final : public juce::Component
{
public:
    Content (OrbitEchoProcessor& p, juce::LookAndFeel& lnf)
        : processor (p), state (p.getState()), presetBar (p), compareBar (p, presetBar), modeMenu (state),
          styleMenu (state), eraSwitch (state), taps (state), visualizer (p),
          mix (state, id::mix, "Mix", "Dry / wet balance (equal power). The padlock (Mix Lock) keeps the Mix when loading presets."),
          duck (state, id::duck, "Duck", "Lowers the echoes (and part of the feedback) while you play; they swell back in the gaps."),
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
          modRate (state, id::modRate, "Rate", "Modulation rate (0.01 - 10 Hz)."),
          modDepth (state, id::modDepth, "Depth", "Modulation depth: chorus / vibrato on the echoes (phaser sweep in Phaser DDL)."),
          freqShift (state, id::freqShift, "Shift", "Linear frequency shift in Hz inside the loop: each repeat moves further (not a musical interval).", true),
          freqDetune (state, id::freqDetune, "Detune", "Shift offset between left and right: small values widen, large values pan quickly."),
          pitchShift (state, id::pitchShift, "Pitch", "Pitch shift inside the loop in semitones (+-24). Shift-drag or Ctrl/Cmd-drag for cents.", true),
          pitchDetune (state, id::pitchDetune, "Detune", "Left / right detune in cents (micro-shift doubling)."),
          diffAmount (state, id::diffAmount, "Amount", "Diffusion inside the loop: 68 % makes bell-shaped echoes, 91 % a dense reverb even with Feedback at 0. 0 % = off."),
          diffSize (state, id::diffSize, "Size", "Dispersion time relative to the delay: low values blur transients, high values become reverberant."),
          highCut (state, id::highCut, "HighCut", "Low-pass inside the loop (200 Hz - 20 kHz): each repeat gets darker."),
          lowCut (state, id::lowCut, "LowCut", "High-pass inside the loop (10 Hz - 2 kHz): each repeat gets thinner.")
    {
        for (auto* c : std::initializer_list<juce::Component*> { &presetBar, &compareBar, &modeMenu, &styleMenu, &eraSwitch,
                                                                 &taps, &visualizer, &styleInfo, &syncL, &syncR, &lock })
            addAndMakeVisible (c);
        for (auto* k : allKnobs())
            addAndMakeVisible (k);

        // Colours: left column and hero, then one colour per section.
        mix.setKnobColour (colours::orange);
        duck.setKnobColour (colours::orange);
        mix.setProminentTitle (true);
        duck.setProminentTitle (true);
        delayMs.setHero (true);
        delayNote.setHero (true);
        for (auto* k : { &spread, &ratio, &spacing, &delayRMs, &delayRNote })
            k->setKnobColour (sectionColour (timeSection));
        for (auto* k : { &feedback, &width })
            k->setKnobColour (sectionColour (feedbackSection));
        for (auto* k : { &drive, &age, &pitchMix, &res })
            k->setKnobColour (sectionColour (colorSection));
        for (auto* k : { &wow, &flutter, &modRate, &modDepth, &freqShift, &freqDetune, &pitchShift, &pitchDetune })
            k->setKnobColour (sectionColour (modSection));
        for (auto* k : { &diffAmount, &diffSize })
            k->setKnobColour (sectionColour (diffSection));
        for (auto* k : { &highCut, &lowCut })
            k->setKnobColour (sectionColour (eqSection));

        pitchShift.getSlider().setCoarseSnap (1.0); // semitone steps unless fine-dragging

        styleInfo.setJustificationType (juce::Justification::centred);
        styleInfo.setColour (juce::Label::textColourId, colours::textDim);
        styleInfo.setMinimumHorizontalScale (1.0f);

        setupSyncButton (syncL, syncLAttachment, id::syncL, rememberedNoteL);
        setupSyncButton (syncR, syncRAttachment, id::syncR, rememberedNoteR);
        syncR.getProperties().set ("accent", colours::amberLight.toString());

        lock.setTooltip ("Mix Lock: loading presets keeps the current Mix (saved with the session, not in presets).");
        lock.onClick = [this] { processor.mixLock = ! processor.mixLock.load(); refresh (true); };

        // Set explicitly once every child exists, so all of them (and the slider
        // value boxes they create) use the panel's look and feel from the start.
        setLookAndFeel (&lnf);
        refresh (false);
    }

    ~Content() override { setLookAndFeel (nullptr); }

    std::vector<Knob*> allKnobs()
    {
        return { &mix, &duck, &delayMs, &delayNote, &delayRMs, &delayRNote, &spread, &ratio, &spacing, &feedback,
                 &width, &drive, &age, &pitchMix, &res, &wow, &flutter, &modRate, &modDepth, &freqShift, &freqDetune,
                 &pitchShift, &pitchDetune, &diffAmount, &diffSize, &highCut, &lowCut };
    }

    void setupSyncButton (juce::TextButton& b, std::unique_ptr<juce::ParameterAttachment>& att, const char* paramId, int& remembered)
    {
        b.setButtonText ("SYNC");
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
        // Header: title (painted), echo visualiser, version + SAVE / A / B.
        visualizer.setBounds (kVisualiser);
        compareBar.setBounds (900, 52, 200, 30);

        // Left column: Mix (with its lock) and Duck.
        const auto knobH = kKnobHeight + 10;
        mix.setBounds (kLeftColumn.getX(), kLeftColumn.getY(), kLeftColumn.getWidth(), knobH);
        lock.setBounds (kLeftColumn.getRight() - 14, kLeftColumn.getY() + 2, 18, 20);
        duck.setBounds (kLeftColumn.getX(), kLeftColumn.getY() + knobH + 26, kLeftColumn.getWidth(), knobH);

        // Hero: Delay (ms or note value) and its Sync switch.
        delayMs.setBounds (kHero);
        delayNote.setBounds (kHero);
        syncL.setBounds (kHero.getCentreX() - 38, kHero.getBottom() + 2, 76, 24);

        // TIME: style knob on top; below, the style's extras.
        const auto timeTop = slot (timeSection, 0), timeBottom = slot (timeSection, 1);
        for (auto* k : { &spread, &ratio, &spacing, &delayRMs, &delayRNote })
            k->setBounds (timeTop);
        const auto t = sectionBounds (timeSection);
        taps.setBounds (t.getX() + 6, timeBottom.getY() + 8, t.getWidth() - 12, 84);
        syncR.setBounds (t.getCentreX() - 34, timeBottom.getY() + 8, 68, 24);
        styleInfo.setBounds (t.getX() + 8, timeBottom.getY() + (twoTimes ? 40 : 6), t.getWidth() - 16, twoTimes ? 70 : 100);

        feedback.setBounds (slot (feedbackSection, 0));
        width.setBounds (slot (feedbackSection, 1));
        drive.setBounds (slot (colorSection, 0));
        for (auto* k : { &age, &pitchMix, &res })
            k->setBounds (slot (colorSection, 1));
        for (auto* k : { &wow, &modRate, &freqShift, &pitchShift })
            k->setBounds (slot (modSection, 0));
        for (auto* k : { &flutter, &modDepth, &freqDetune, &pitchDetune })
            k->setBounds (slot (modSection, 1));
        diffAmount.setBounds (slot (diffSection, 0));
        diffSize.setBounds (slot (diffSection, 1));
        highCut.setBounds (slot (eqSection, 0));
        lowCut.setBounds (slot (eqSection, 1));

        // Bottom bar.
        auto bar = kBottomBar;
        modeMenu.setBounds (bar.removeFromLeft (300));
        styleMenu.setBounds (bar.removeFromLeft (240));
        eraSwitch.setBounds (bar.removeFromLeft (200));
        presetBar.setBounds (bar);
    }

    void paint (juce::Graphics& g) override
    {
        auto* lf = dynamic_cast<ui::OrbitLookAndFeel*> (&getLookAndFeel());
        auto font = [lf] (float h, Weight w) { return lf != nullptr ? lf->font (h, w) : juce::Font (juce::FontOptions (h)); };

        g.setGradientFill (juce::ColourGradient (colours::background, 560.0f, 0.0f, colours::backgroundDeep, 560.0f, 448.0f, false));
        g.fillAll();

        // Title: two-tone product name.
        const auto titleFont = font (60.0f, Weight::bold);
        const juce::String first ("Orbit"), second ("Echo");
        const auto firstWidth = juce::GlyphArrangement::getStringWidth (titleFont, first);
        g.setFont (titleFont);
        g.setColour (colours::accent);
        g.drawText (first, juce::Rectangle<float> (24.0f, 4.0f, firstWidth + 4.0f, 84.0f), juce::Justification::centredLeft);
        g.setColour (colours::accentRed);
        g.drawText (second, juce::Rectangle<float> (24.0f + firstWidth, 4.0f, 200.0f, 84.0f), juce::Justification::centredLeft);

        // Version line, top right.
        g.setFont (font (17.0f, Weight::semiBold));
        g.setColour (colours::accent);
        g.drawText ("Version " JucePlugin_VersionString, 860, 14, 240, 26, juce::Justification::centredRight);

        // Section frames.
        for (int i = 0; i < numSections; ++i)
        {
            const auto r = sectionBounds (i).toFloat();
            g.setColour (sectionColour (i));
            g.drawRoundedRectangle (r.reduced (1.5f), 14.0f, 3.0f);
            g.setFont (font (20.0f, Weight::bold));
            g.setColour (colours::text);
            g.drawText (sectionTitle (i), r.withHeight (36.0f).translated (0.0f, 4.0f), juce::Justification::centred);
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
        lock.setToggleState (processor.mixLock.load(), juce::dontSendNotification);
        if (! changed)
            return;

        mode = newMode;
        style = newStyle;
        lastSyncL = syncOnL;
        lastSyncR = syncOnR;
        initialised = true;
        twoTimes = style == dsp::Style::Dual || style == dsp::Style::PingPong;
        const auto quad = style == dsp::Style::Quad;

        delayMs.setTitle (twoTimes ? "Delay L" : "Delay");
        delayNote.setTitle (twoTimes ? "Delay L" : "Delay");
        modeMenu.setTooltip (juce::String ("Mode: ") + modeDescription (mode) + " Click to choose another algorithm.");
        styleInfo.setText (styleDescription (style), juce::dontSendNotification);
        styleInfo.setFont (juce::Font (juce::FontOptions (13.5f)));

        // Duck: main control for the modes that feature it, "extra" otherwise.
        const auto duckMain = params::duckIsInMainSection (mode);
        duck.setTitle (duckMain ? "Duck" : "Extra: Duck");
        duck.setTitleDimmed (! duckMain);
        duck.setKnobColour (duckMain ? colours::orange : colours::textDim.withMultipliedBrightness (0.7f));

        show (delayMs, ! syncOnL, animate);
        show (delayNote, syncOnL, animate);
        show (delayRMs, twoTimes && ! syncOnR, animate);
        show (delayRNote, twoTimes && syncOnR, animate);
        show (syncR, twoTimes, animate);
        show (spread, style == dsp::Style::Single, animate);
        show (ratio, style == dsp::Style::Ratio, animate);
        show (spacing, quad, animate);
        show (taps, quad, animate);
        show (styleInfo, ! quad, animate);

        for (auto* k : { &age, &pitchMix, &res, &wow, &flutter, &modRate, &modDepth, &freqShift, &freqDetune, &pitchShift, &pitchDetune })
            show (*k, params::isVisible (k->getParamId(), mode, style), animate);

        resized();
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
    ui::CompareBar compareBar;
    ui::ModeMenu modeMenu;
    ui::StyleMenu styleMenu;
    ui::EraSwitch eraSwitch;
    ui::TapButtons taps;
    ui::EchoVisualizer visualizer;
    juce::Label styleInfo;

    Knob mix, duck, delayMs, delayNote, delayRMs, delayRNote, spread, ratio, spacing, feedback, width, drive, age,
        pitchMix, res, wow, flutter, modRate, modDepth, freqShift, freqDetune, pitchShift, pitchDetune, diffAmount,
        diffSize, highCut, lowCut;

    juce::TextButton syncL, syncR;
    LockButton lock;
    std::unique_ptr<juce::ParameterAttachment> syncLAttachment, syncRAttachment;
    int rememberedNoteL = dsp::kSyncQuarter, rememberedNoteR = dsp::kSyncQuarter;

    dsp::Mode mode = dsp::Mode::Tape;
    dsp::Style style = dsp::Style::Single;
    bool lastSyncL = false, lastSyncR = false, initialised = false, twoTimes = false;
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
    g.fillAll (colours::backgroundDeep);
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
