// Plugsalot - GPL-3.0-or-later
// A pop-up over the rack with every parameter of one module (the faceplate only shows the
// main knobs), the file pickers for amps and cabinets, and a button to remove the module.
#pragma once

#include "ModuleEditor.h"
#include "Theme.h"
#include "Vintage.h"

namespace plugsalot
{

class ModuleOverlay final : public juce::Component
{
public:
    ModuleOverlay(PlugsalotProcessor& p, ModuleEditor& ed, int slotIndex) : proc(p), editor(ed), slot(slotIndex)
    {
        closeButton.onClick = [this] { if (onClose) onClose(); };
        removeButton.onClick = [this] { const int s = slot; if (onClose) onClose(); proc.removeSlot(s); };
        closeButton.getProperties().set("fontSize", 16.0f);
        removeButton.getProperties().set("fontSize", 14.0f);
        addAndMakeVisible(closeButton);
        addAndMakeVisible(removeButton);
        addAndMakeVisible(editor);
        setWantsKeyboardFocus(true);
    }

    ~ModuleOverlay() override { removeChildComponent(&editor); }

    int slotIndex() const { return slot; }
    std::function<void()> onClose;

    void paint(juce::Graphics& g) override
    {
        g.fillAll(juce::Colour(0xb3000000));
        const auto p = panelBounds().toFloat();
        g.setColour(juce::Colour(0x99000000));
        g.fillRoundedRectangle(p.translated(0.0f, 8.0f).expanded(2.0f), 8.0f);
        g.setGradientFill(juce::ColourGradient(vintage::col::enamel.interpolatedWith(juce::Colours::white, 0.07f), 0.0f, p.getY(),
                                               vintage::col::enamel.interpolatedWith(juce::Colours::black, 0.10f), 0.0f, p.getBottom(), false));
        g.fillRoundedRectangle(p, 6.0f);
        g.setColour(juce::Colour(0x1fffffff));
        g.drawRoundedRectangle(p.reduced(0.5f), 6.0f, 1.0f);
        vintage::drawScrew(g, { p.getX() + 14.0f, p.getY() + 14.0f }, 11.0f, 0.7f);
        vintage::drawScrew(g, { p.getRight() - 14.0f, p.getY() + 14.0f }, 11.0f, 2.3f);
        g.setColour(vintage::col::cream);
        g.setFont(vintage::condensed(22.0f).withExtraKerningFactor(0.1f));
        g.drawText(editor.moduleTitle().toUpperCase() + "  /  ALL SETTINGS", p.toNearestInt().withTrimmedTop(10).withHeight(40).reduced(36, 0),
                   juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto p = panelBounds().reduced(18, 0);
        p.removeFromTop(54);
        auto foot = p.removeFromBottom(54).withTrimmedTop(10).withTrimmedBottom(10);
        closeButton.setBounds(foot.removeFromRight(100));
        foot.removeFromRight(10);
        removeButton.setBounds(foot.removeFromRight(190));
        p.removeFromBottom(4);
        editor.setBounds(p);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (! panelBounds().contains(e.getPosition()) && onClose)
            onClose();
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        if (k == juce::KeyPress::escapeKey && onClose) { onClose(); return true; }
        return false;
    }

private:
    juce::Rectangle<int> panelBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre(juce::jmin(560, getWidth() - 40), juce::jmin(620, getHeight() - 40));
    }

    PlugsalotProcessor& proc;
    ModuleEditor& editor;
    int slot;
    juce::TextButton closeButton { "CLOSE" }, removeButton { "REMOVE FROM CHAIN" };
};

} // namespace plugsalot
