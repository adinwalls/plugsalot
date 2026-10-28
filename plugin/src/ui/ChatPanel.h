// Plugsalot - GPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

namespace plugsalot
{

// A separate desktop window for typing. Some hosts (Ardour on Linux, for one)
// keep keyboard input for their own shortcuts, so typing into the plugin window
// does nothing; a window of our own gets keyboard focus from the system directly.
class TypingWindow final : public juce::DocumentWindow
{
public:
    TypingWindow(const juce::String& initialText, std::function<void(const juce::String&)> onSend);
    void closeButtonPressed() override;

private:
    struct Content;
};

class ChatPanel final : public juce::Component, private juce::Timer
{
public:
    explicit ChatPanel(PlugsalotProcessor& p);
    ~ChatPanel() override;
    void refresh();
    void resized() override;
    void paint(juce::Graphics&) override;

private:
    void send(const juce::String& text);
    void openTypingWindow();
    void timerCallback() override;

    PlugsalotProcessor& proc;
    juce::TextEditor history, input;
    juce::TextButton sendButton { "Send" };
    juce::TextButton typingWindowButton { "Can't type? Open a typing window" };
    juce::OwnedArray<juce::TextButton> quick;
    juce::Label busy, chatLabel;
    std::unique_ptr<TypingWindow> typingWindow;
    int dots = 0;
};

} // namespace plugsalot
