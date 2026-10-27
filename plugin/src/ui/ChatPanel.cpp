// Plugsalot - GPL-3.0-or-later
#include "ChatPanel.h"

namespace plugsalot
{

ChatPanel::ChatPanel(PlugsalotProcessor& p) : proc(p)
{
    history.setMultiLine(true, true);
    history.setReadOnly(true);
    history.setCaretVisible(false);
    history.setScrollbarsShown(true);
    history.setColour(juce::TextEditor::backgroundColourId, theme::background);
    history.setColour(juce::TextEditor::outlineColourId, theme::outline);
    addAndMakeVisible(history);

    input.setMultiLine(false);
    input.setReturnKeyStartsNewLine(false);
    input.setTextToShowWhenEmpty("e.g. warm, intimate vocal with a little space", theme::muted);
    input.setFont(juce::FontOptions(14.0f));
    input.onReturnKey = [this] { send(input.getText()); };
    addAndMakeVisible(input);

    sendButton.setColour(juce::TextButton::buttonColourId, theme::accent.darker(0.3f));
    sendButton.onClick = [this] { send(input.getText()); };
    addAndMakeVisible(sendButton);

    for (auto label : { "Set up a chain", "Brighter", "Warmer", "Less muddy", "More punch", "More space", "Explain the chain" })
    {
        auto* b = quick.add(new juce::TextButton(label));
        b->onClick = [this, b]
        {
            const auto t = b->getButtonText();
            send(t == "Set up a chain" ? juce::String() : t);
        };
        addAndMakeVisible(b);
    }

    busy.setColour(juce::Label::textColourId, theme::accent);
    busy.setFont(juce::FontOptions(12.5f));
    addAndMakeVisible(busy);
    refresh();
}

void ChatPanel::send(const juce::String& text)
{
    if (proc.isAiBusy())
        return;
    proc.askAi(text);
    input.clear();
    refresh();
}

void ChatPanel::timerCallback()
{
    if (! proc.isAiBusy())
    {
        stopTimer();
        busy.setText({}, juce::dontSendNotification);
        return;
    }
    dots = (dots + 1) % 4;
    busy.setText("Thinking" + juce::String::repeatedString(".", dots), juce::dontSendNotification);
}

void ChatPanel::refresh()
{
    history.clear();
    for (auto& m : proc.getChat())
    {
        juce::Colour c = m.from == ChatMessage::From::User ? theme::user
                       : m.from == ChatMessage::From::Assistant ? theme::text : theme::warning;
        const juce::String who = m.from == ChatMessage::From::User ? "You" : m.from == ChatMessage::From::Assistant ? "Plugsalot" : "Note";
        history.setFont(juce::FontOptions(12.0f, juce::Font::bold));
        history.setColour(juce::TextEditor::textColourId, c.withAlpha(0.8f));
        history.insertTextAtCaret(who + "\n");
        history.setFont(juce::FontOptions(14.5f));
        history.setColour(juce::TextEditor::textColourId, c);
        history.insertTextAtCaret(m.text + "\n\n");
    }
    history.moveCaretToEnd();

    const bool isBusy = proc.isAiBusy();
    sendButton.setEnabled(! isBusy);
    for (auto* b : quick)
        b->setEnabled(! isBusy);
    if (isBusy)
        startTimer(400);
    else
        busy.setText({}, juce::dontSendNotification);
}

void ChatPanel::paint(juce::Graphics& g)
{
    g.fillAll(theme::panel);
    g.setColour(theme::text);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("ASSISTANT", 14, 8, 200, 18, juce::Justification::centredLeft);
}

void ChatPanel::resized()
{
    auto r = getLocalBounds().reduced(10, 8);
    auto top = r.removeFromTop(20);
    busy.setBounds(top.removeFromRight(160));
    r.removeFromTop(4);

    auto inputRow = r.removeFromBottom(34);
    sendButton.setBounds(inputRow.removeFromRight(80));
    inputRow.removeFromRight(6);
    input.setBounds(inputRow);
    r.removeFromBottom(6);

    // Quick buttons wrap onto up to two rows.
    auto quickArea = r.removeFromBottom(60);
    juce::FlexBox fb;
    fb.flexWrap = juce::FlexBox::Wrap::wrap;
    fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    for (auto* b : quick)
        fb.items.add(juce::FlexItem(*b).withWidth((float) juce::jmax(70, b->getBestWidthForHeight(26) + 14)).withHeight(26).withMargin(2));
    fb.performLayout(quickArea);
    r.removeFromBottom(4);
    history.setBounds(r);
}

} // namespace plugsalot
