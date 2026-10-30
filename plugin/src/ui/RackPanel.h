// Plugsalot - GPL-3.0-or-later
// The rack: six slots, each holding a module faceplate with colour-coded knobs.
// Knobs follow the catalogue's `knobs` list; every other parameter is in the
// module's settings overlay (click the name plate).
#pragma once

#include "PluginProcessor.h"
#include "Vintage.h"

namespace plugsalot
{

class RackPanel final : public juce::Component, private juce::Timer
{
public:
    static constexpr int maxSlots = 6;
    static constexpr int cardWidth = 124, cardHeight = 568, pad = 16, gap = 8;

    explicit RackPanel(PlugsalotProcessor& p);
    ~RackPanel() override;

    // Called whenever the processor's chain changes. Same modules: values update in place
    // (knobs sweep). Different modules: the old ones unplug and the new ones plug in.
    void refresh();
    void paint(juce::Graphics&) override;

    std::function<void(int slot)> onOpenSettings;
    std::function<void()> onStatusChanged;
    // "Plugging in X..." while the animation runs, otherwise empty.
    juce::String busyText() const { return statusText; }

private:
    class Knob;
    class Card;
    using Signature = juce::StringArray;

    Signature currentSignature() const;
    void buildCards(bool seated);
    void startTransition(bool hadCards);
    void timerCallback() override;
    void setStatus(const juce::String&);
    juce::Rectangle<int> slotBounds(int i) const
    {
        return { pad + i * (cardWidth + gap), pad, cardWidth, cardHeight };
    }

    PlugsalotProcessor& proc;
    Signature sig;
    juce::OwnedArray<Card> cards, leaving;
    std::vector<double> enterAt, leaveAt;   // ms after the transition began
    double transitionStart = 0.0, transitionEnd = 0.0;
    bool animating = false;
    juce::String statusText;
};

} // namespace plugsalot
