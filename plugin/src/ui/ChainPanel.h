// Plugsalot - GPL-3.0-or-later
// The chain strip: one row per module, with bypass and remove, plus undo/clear.
#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

namespace plugsalot
{

class ChainPanel final : public juce::Component
{
public:
    explicit ChainPanel(PlugsalotProcessor& p);
    void refresh();
    void resized() override;
    void paint(juce::Graphics&) override;

    std::function<void(int)> onSelect;
    int selected() const { return selectedSlot; }

private:
    class Row final : public juce::Component
    {
    public:
        Row(ChainPanel& owner, int index);
        void paint(juce::Graphics&) override;
        void resized() override;
        void mouseUp(const juce::MouseEvent&) override;

        ChainPanel& owner;
        int index;
        juce::ToggleButton on;
        juce::TextButton remove { "x" };
    };

    PlugsalotProcessor& proc;
    juce::OwnedArray<Row> rows;
    juce::TextButton undoButton { "Undo" }, clearButton { "Clear" };
    juce::Label footer;
    int selectedSlot = -1;
};

} // namespace plugsalot
