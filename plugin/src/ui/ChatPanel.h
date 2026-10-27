// Plugsalot - GPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

namespace plugsalot
{

class ChatPanel final : public juce::Component, private juce::Timer
{
public:
    explicit ChatPanel(PlugsalotProcessor& p);
    void refresh();
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void send(const juce::String& text);
    void timerCallback() override;

    PlugsalotProcessor& proc;
    juce::TextEditor history, input;
    juce::TextButton sendButton { "Send" };
    juce::OwnedArray<juce::TextButton> quick;
    juce::Label busy;
    int dots = 0;
};

} // namespace plugsalot
