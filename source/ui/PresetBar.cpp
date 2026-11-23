#include "ui/PresetBar.h"

#include "PluginProcessor.h"
#include "PresetManager.h"
#include "ui/LookAndFeel.h"

namespace oe::ui
{

namespace
{
    constexpr int kArrowSize = 18;

    juce::Font barFont (juce::Component& c, OrbitLookAndFeel::Weight w)
    {
        if (auto* lf = dynamic_cast<OrbitLookAndFeel*> (&c.getLookAndFeel()))
            return lf->font (21.0f, w);
        return juce::Font (juce::FontOptions (21.0f));
    }
} // namespace

// =============================================================================
PresetBar::PresetBar (OrbitEchoProcessor& p) : presets (p.getPresetManager())
{
    prev.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    prev.onClick = [this] { presets.loadPrevious(); timerCallback(); };
    next.onClick = [this] { presets.loadNext(); timerCallback(); };
    addAndMakeVisible (prev);
    addAndMakeVisible (next);
    setTooltip ("Presets by category; Save stores your own.");
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
    timerCallback();
    startTimerHz (8);
}

PresetBar::~PresetBar() = default;

void PresetBar::timerCallback()
{
    auto text = presets.getCurrentName();
    if (presets.isModified())
        text << " *";
    if (text != shownName)
    {
        shownName = text;
        repaint();
    }
}

juce::Rectangle<int> PresetBar::nameArea() const
{
    return getLocalBounds().withTrimmedRight (2 * kArrowSize + 20);
}

void PresetBar::paint (juce::Graphics& g)
{
    const auto labelFont = barFont (*this, OrbitLookAndFeel::Weight::bold);
    const juce::String label ("PRESETS: ");
    const auto labelWidth = juce::GlyphArrangement::getStringWidthInt (labelFont, label);
    auto area = nameArea();

    g.setFont (labelFont);
    g.setColour (colours::text);
    g.drawText (label, area.removeFromLeft (labelWidth), juce::Justification::centredLeft);
    g.setFont (barFont (*this, OrbitLookAndFeel::Weight::semiBold));
    g.setColour (isMouseOver() ? colours::accent.brighter (0.25f) : colours::accent);
    g.drawText (shownName, area, juce::Justification::centredLeft, true);
}

void PresetBar::resized()
{
    auto r = getLocalBounds();
    const auto y = (r.getHeight() - kArrowSize) / 2;
    next.setBounds (r.getRight() - kArrowSize, y, kArrowSize, kArrowSize);
    prev.setBounds (r.getRight() - 2 * kArrowSize - 8, y, kArrowSize, kArrowSize);
}

void PresetBar::mouseUp (const juce::MouseEvent& e)
{
    if (nameArea().contains (e.getPosition()))
        showPresetMenu();
}

void PresetBar::showPresetMenu()
{
    juce::PopupMenu menu;
    for (const auto& category : presets.getCategories())
    {
        juce::PopupMenu sub;
        for (int i = 0; i < presets.getNumPresets(); ++i)
            if (presets.getPreset (i).category == category)
                sub.addItem (i + 1, presets.getPreset (i).name, true, i == presets.getCurrentIndex());
        menu.addSubMenu (category, sub);
    }
    menu.addSeparator();
    menu.addItem (10000, "Save current as user preset...");
    menu.addItem (10001, "Rescan user presets");

    juce::Component::SafePointer<PresetBar> safe (this);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this), [safe] (int result)
    {
        if (safe == nullptr || result == 0)
            return;
        if (result == 10000)
            safe->showSaveDialog();
        else if (result == 10001)
            safe->presets.rescanUserPresets();
        else
            safe->presets.loadPreset (result - 1);
        safe->timerCallback();
    });
}

void PresetBar::showSaveDialog()
{
    saveWindow = std::make_unique<juce::AlertWindow> ("Save preset", "Name of the new user preset:", juce::MessageBoxIconType::NoIcon, this);
    saveWindow->addTextEditor ("name", presets.getCurrentName());
    saveWindow->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    saveWindow->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    saveWindow->setLookAndFeel (&getLookAndFeel());

    juce::Component::SafePointer<PresetBar> safe (this);
    saveWindow->enterModalState (true, juce::ModalCallbackFunction::create ([safe] (int result)
    {
        if (safe == nullptr || safe->saveWindow == nullptr)
            return;
        if (result == 1)
        {
            const auto presetName = safe->saveWindow->getTextEditorContents ("name");
            if (! safe->presets.saveUserPreset (presetName))
                juce::AlertWindow::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon, "Save preset",
                                                        "The preset could not be saved to\n"
                                                            + PresetManager::getUserPresetFolder().getFullPathName());
        }
        safe->saveWindow.reset();
        safe->timerCallback();
    }), false);
}

// =============================================================================
CompareBar::CompareBar (OrbitEchoProcessor& p, PresetBar& presetBar) : presets (p.getPresetManager())
{
    save.setTooltip ("Save the current settings as a user preset");
    a.setTooltip ("A/B compare: settings A");
    b.setTooltip ("A/B compare: settings B (starts as a copy of A)");
    save.onClick = [&presetBar] { presetBar.showSaveDialog(); };
    a.onClick = [this] { presets.selectA(); timerCallback(); };
    b.onClick = [this] { presets.selectB(); timerCallback(); };
    for (auto* c : { &save, &a, &b })
        addAndMakeVisible (*c);
    timerCallback();
    startTimerHz (8);
}

CompareBar::~CompareBar() = default;

void CompareBar::timerCallback()
{
    a.setToggleState (! presets.isOnB(), juce::dontSendNotification);
    b.setToggleState (presets.isOnB(), juce::dontSendNotification);
}

void CompareBar::resized()
{
    auto r = getLocalBounds();
    b.setBounds (r.removeFromRight (34));
    r.removeFromRight (6);
    a.setBounds (r.removeFromRight (34));
    r.removeFromRight (12);
    save.setBounds (r.removeFromRight (70));
}

} // namespace oe::ui
