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
    // `over` is the screen area to cover (the chat box), so typing feels in place.
    TypingWindow(const juce::String& initialText, std::function<void(const juce::String&)> onSend,
                 std::function<void(const juce::String&)> onCloseWithText, juce::Rectangle<int> over);
    void closeButtonPressed() override;
    void activeWindowStatusChanged() override;

private:
    struct Content;
    std::function<void(const juce::String&)> onCloseWithText;
};

class ChatPanel final : public juce::Component, private juce::Timer
{
public:
    explicit ChatPanel(PlugsalotProcessor& p);
    ~ChatPanel() override;
    void refresh();
    void resized() override;
    void paint(juce::Graphics&) override;
    void paintOverChildren(juce::Graphics&) override;

private:
    // The big red-orange pilot lamp on its bezel, with a "SEND" plate above it.
    struct LampButton final : public juce::Button
    {
        LampButton() : juce::Button("send") {}
        void paintButton(juce::Graphics&, bool over, bool down) override;
    };

    void send(const juce::String& text);
    void openTypingWindow();
    void mouseDown(const juce::MouseEvent&) override;
    void timerCallback() override;

    PlugsalotProcessor& proc;
    juce::TextEditor history, input;
    LampButton sendButton;
    juce::TextButton typingWindowButton { "Can't type? Open a typing window" };
    juce::OwnedArray<juce::TextButton> quick;
    juce::Label busy;
    juce::Rectangle<float> talkTape, tweakTape;
    std::unique_ptr<TypingWindow> typingWindow;
    // Linux plugin hosts (Ardour, for one) keep the keyboard on their own window;
    // there a click on the chat box asks the system for the keyboard directly.
    bool takeFocusFromHost = false;
    int dots = 0;
};

} // namespace plugsalot
