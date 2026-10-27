// Plugsalot - GPL-3.0-or-later
// Generic controls for the selected module, generated from its catalogue entry.
// Lets the user fine-tune what the AI set, within the same safe ranges.
#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

namespace plugsalot
{

class ModuleEditor final : public juce::Component
{
public:
    explicit ModuleEditor(PlugsalotProcessor& p);
    void showSlot(int slot);
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    struct Control final : public juce::Component
    {
        juce::Label label;
        std::unique_ptr<juce::Component> widget;
        juce::String hint;
        void resized() override
        {
            auto r = getLocalBounds();
            label.setBounds(r.removeFromLeft(juce::jmin(150, r.getWidth() / 3)));
            if (widget) widget->setBounds(r.reduced(0, 3));
        }
    };

    PlugsalotProcessor& proc;
    int slot = -1;
    juce::String title;
    juce::Viewport viewport;
    juce::Component content;
    juce::OwnedArray<Control> controls;
};

} // namespace plugsalot
