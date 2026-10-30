// Plugsalot - GPL-3.0-or-later
#include "ChatPanel.h"
#include "LinuxFocus.h"

namespace plugsalot
{

static constexpr const char* placeholder = "e.g. warm, intimate vocal with a little space";

// ---------------------------------------------------------------------------
// Typing window
// ---------------------------------------------------------------------------

struct TypingWindow::Content final : public juce::Component
{
    Content(const juce::String& initial, std::function<void(const juce::String&)> send)
        : onSend(std::move(send))
    {
        label.setText("Type your message, then press Enter or click Send.", juce::dontSendNotification);
        label.setColour(juce::Label::textColourId, theme::muted);
        addAndMakeVisible(label);

        editor.setMultiLine(true, true);
        editor.setReturnKeyStartsNewLine(false);
        editor.setFont(juce::FontOptions(16.0f));
        editor.setText(initial, false);
        editor.setTextToShowWhenEmpty(placeholder, theme::muted);
        editor.setColour(juce::TextEditor::outlineColourId, theme::accent.withAlpha(0.6f));
        editor.onReturnKey = [this] { submit(); };
        addAndMakeVisible(editor);

        sendButton.setColour(juce::TextButton::buttonColourId, theme::accent.darker(0.3f));
        sendButton.onClick = [this] { submit(); };
        addAndMakeVisible(sendButton);
        setSize(560, 190);
    }

    void submit()
    {
        auto text = editor.getText();
        juce::MessageManager::callAsync([cb = onSend, text] { cb(text); });
    }

    void paint(juce::Graphics& g) override { g.fillAll(theme::panel); }
    void resized() override
    {
        auto r = getLocalBounds().reduced(12);
        label.setBounds(r.removeFromTop(22));
        auto bottom = r.removeFromBottom(34);
        sendButton.setBounds(bottom.removeFromRight(100));
        r.removeFromBottom(8);
        editor.setBounds(r);
    }
    void visibilityChanged() override
    {
        if (isShowing())
            editor.grabKeyboardFocus();
    }

    std::function<void(const juce::String&)> onSend;
    juce::Label label;
    juce::TextEditor editor;
    juce::TextButton sendButton { "Send" };
};

TypingWindow::TypingWindow(const juce::String& initialText, std::function<void(const juce::String&)> onSend,
                           std::function<void(const juce::String&)> onClose, juce::Rectangle<int> over)
    : juce::DocumentWindow("Plugsalot: type your message", theme::panel, juce::DocumentWindow::closeButton),
      onCloseWithText(std::move(onClose))
{
    setUsingNativeTitleBar(true);
    setContentOwned(new Content(initialText, std::move(onSend)), true);
    setResizable(true, false);
    setAlwaysOnTop(true);
    if (over.getWidth() > 100 && over.getHeight() > 40)
        setBounds(over.withSizeKeepingCentre(juce::jmax(420, over.getWidth()), juce::jmax(170, over.getHeight() + 70)));
    else
        centreWithSize(getWidth(), getHeight());
    setVisible(true);
    toFront(true);
    if (auto* c = dynamic_cast<Content*>(getContentComponent()))
        c->editor.grabKeyboardFocus();
}

void TypingWindow::activeWindowStatusChanged()
{
    // The text box can only take the keyboard once the window is active, which
    // happens a moment after it opens; grabbing earlier is silently ignored.
    if (isActiveWindow())
        if (auto* c = dynamic_cast<Content*>(getContentComponent()))
            c->editor.grabKeyboardFocus();
}

void TypingWindow::closeButtonPressed()
{
    if (auto* c = dynamic_cast<Content*>(getContentComponent()))
        if (onCloseWithText)
            onCloseWithText(c->editor.getText());
    setVisible(false);
}

// ---------------------------------------------------------------------------
// Chat panel
// ---------------------------------------------------------------------------

ChatPanel::ChatPanel(PlugsalotProcessor& p) : proc(p)
{
    history.setMultiLine(true, true);
    history.setReadOnly(true);
    history.setCaretVisible(false);
    history.setScrollbarsShown(true);
    history.setColour(juce::TextEditor::backgroundColourId, vintage::col::transcript);
    history.setColour(juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    history.setColour(juce::TextEditor::focusedOutlineColourId, juce::Colours::transparentBlack);
    history.setIndents(10, 10);
    addAndMakeVisible(history);

    // Multi-line so longer requests stay visible; Enter sends.
    input.setMultiLine(true, true);
    input.setReturnKeyStartsNewLine(false);
    input.setScrollbarsShown(true);
    input.setTextToShowWhenEmpty(placeholder, theme::muted);
    input.setFont(vintage::body(14.5f));
    input.setIndents(8, 8);
    input.setBorder(juce::BorderSize<int>(1));
    input.setColour(juce::TextEditor::backgroundColourId, vintage::col::inputBack);
    input.setColour(juce::TextEditor::outlineColourId, juce::Colour(0xff4a576b));
    input.setColour(juce::TextEditor::focusedOutlineColourId, vintage::col::note);
    input.setTooltip("Give Plugsalot as much detail as possible so it can dial in your settings. "
                     "Tell it how the instrument or voice was recorded, what's in the mix with it etc.");
    input.onReturnKey = [this] { send(input.getText()); };
    addAndMakeVisible(input);

    sendButton.onClick = [this] { send(input.getText()); };
    addAndMakeVisible(sendButton);

    // The typing-window fallback is for hosts that keep the keyboard; the
    // standalone app always receives keys, so it doesn't need it.
    typingWindowButton.getProperties().set("ghost", true);
    typingWindowButton.getProperties().set("fontSize", 13.0f);
    typingWindowButton.setTooltip("Some DAWs (for example Ardour) keep the keyboard for their own shortcuts. "
                                  "In Ardour you can also click the keyboard button at the top right of the plugin window.");
    typingWindowButton.onClick = [this] { openTypingWindow(); };
    addAndMakeVisible(typingWindowButton);
   #if JUCE_LINUX || JUCE_BSD
    takeFocusFromHost = proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone;
   #endif
    // Hidden in the standalone app, which always gets typing.
    typingWindowButton.setVisible(proc.wrapperType != juce::AudioProcessor::wrapperType_Standalone);
    input.addMouseListener(this, true);

    for (auto label : { "Set up a chain", "Brighter", "Warmer", "Less muddy", "More punch", "More space", "Explain the chain" })
    {
        auto* b = quick.add(new juce::TextButton(label));
        b->getProperties().set("cap", true);
        b->getProperties().set("fontSize", 14.0f);
        b->onClick = [this, b]
        {
            const auto t = b->getButtonText();
            send(t == "Set up a chain" ? juce::String() : t);
        };
        addAndMakeVisible(b);
    }

    busy.setColour(juce::Label::textColourId, vintage::col::gold);
    busy.setFont(vintage::body(12.5f));
    busy.setJustificationType(juce::Justification::centredRight);
    busy.setBorderSize({});
    addAndMakeVisible(busy);
    refresh();
}

ChatPanel::~ChatPanel() = default;

void ChatPanel::openTypingWindow()
{
    if (typingWindow != nullptr && typingWindow->isVisible())
    {
        typingWindow->toFront(true);
        return;
    }
    juce::Component::SafePointer<ChatPanel> safeThis(this);
    typingWindow = std::make_unique<TypingWindow>(
        input.getText(),
        [safeThis](const juce::String& text)
        {
            if (safeThis == nullptr)
                return;
            safeThis->send(text);
            if (safeThis->typingWindow != nullptr)
                safeThis->typingWindow->setVisible(false);
        },
        [safeThis](const juce::String& text)
        {
            // Closed without sending: keep what was typed in the chat box.
            if (safeThis != nullptr)
                safeThis->input.setText(text, false);
        },
        input.getScreenBounds());
}

void ChatPanel::mouseDown(const juce::MouseEvent& e)
{
    if (! takeFocusFromHost || ! (e.eventComponent == &input || input.isParentOf(e.eventComponent)))
        return;
    // Ask the system for the keyboard, then focus the box once the request has landed.
    if (takeSystemKeyboardFocus(input))
    {
        juce::Component::SafePointer<juce::TextEditor> box(&input);
        for (int ms : { 30, 120, 300 })
            juce::Timer::callAfterDelay(ms, [box]
            {
                if (box != nullptr && ! box->hasKeyboardFocus(false))
                    box->grabKeyboardFocus();
            });
    }
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
        const bool isUser = m.from == ChatMessage::From::User, isNote = m.from == ChatMessage::From::System;
        const juce::Colour who = isUser ? vintage::col::you : isNote ? vintage::col::note : vintage::col::plugsalot;
        history.setFont(vintage::condensed(11.5f).withExtraKerningFactor(0.12f));
        history.setColour(juce::TextEditor::textColourId, who);
        history.insertTextAtCaret(juce::String(isUser ? "YOU" : isNote ? "NOTE" : "PLUGSALOT") + "\n");
        history.setFont(vintage::body(14.0f));
        history.setColour(juce::TextEditor::textColourId, isNote ? vintage::col::note : vintage::col::textHi);
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

void ChatPanel::LampButton::paintButton(juce::Graphics& g, bool over, bool down)
{
    // "SEND" plate, then the bezel with the lamp in it.
    const float w = (float) getWidth();
    juce::Rectangle<float> plate((w - 56.0f) * 0.5f, 0.0f, 56.0f, 17.0f);
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff2a2a2a), 0.0f, plate.getY(), juce::Colour(0xff121212), 0.0f, plate.getBottom(), false));
    g.fillRoundedRectangle(plate, 1.0f);
    g.setColour(juce::Colour(0x1fffffff));
    g.drawLine(plate.getX(), plate.getY() + 0.5f, plate.getRight(), plate.getY() + 0.5f);
    g.setColour(juce::Colour(0xfff1ece2));
    g.setFont(vintage::condensed(12.0f, false).withExtraKerningFactor(0.16f));
    g.drawText("SEND", plate, juce::Justification::centred);

    const auto bezel = juce::Rectangle<float>(76.0f, 76.0f).withCentre({ w * 0.5f, 22.0f + 38.0f });
    vintage::drawBezel(g, bezel, 10.0f);
    const bool on = isEnabled();
    if (! on)
        g.beginTransparencyLayer(0.45f);
    vintage::drawLamp(g, bezel.reduced(5.0f), over && on, down && on);
    if (! on)
        g.endTransparencyLayer();
}

void ChatPanel::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(vintage::col::assistant);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(juce::Colour(0xff131820));
    g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);
    g.setColour(vintage::col::textHi);
    g.setFont(vintage::condensed(15.0f).withExtraKerningFactor(0.14f));
    g.drawText("ASSISTANT", 12, 12, 200, 20, juce::Justification::centredLeft);
    // shadow at the top of the transcript, like a recessed window
    auto t = history.getBounds().toFloat();
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x99000000), 0.0f, t.getY(), juce::Colours::transparentBlack, 0.0f, t.getY() + 8.0f, false));
    g.fillRect(t.withHeight(8.0f));
}

void ChatPanel::paintOverChildren(juce::Graphics& g)
{
    vintage::drawTape(g, tweakTape, "SMALL TWEAKS, NO REBUILD", 11.0f, -2.0f);
    vintage::drawTape(g, talkTape, "Talk to Plugsalot", 14.0f, -1.5f);
}

void ChatPanel::resized()
{
    auto r = getLocalBounds().reduced(12);
    auto head = r.removeFromTop(20);
    busy.setBounds(head.removeFromRight(200));
    r.removeFromTop(10);

    // bottom up: typing-window helper, input row, tape label, quick buttons, then the transcript
    if (typingWindowButton.isVisible())
    {
        auto row = r.removeFromBottom(22);
        typingWindowButton.setBounds(row.removeFromLeft(240));
        r.removeFromBottom(4);
    }
    auto inputRow = r.removeFromBottom(98 + 0);
    auto lampCol = inputRow.removeFromRight(78);
    sendButton.setBounds(lampCol.withHeight(22 + 76 + 4));
    inputRow.removeFromRight(8);
    input.setBounds(inputRow);
    r.removeFromBottom(8);

    const auto talkFont = vintage::marker(14.0f);
    const float tw = vintage::textWidth(talkFont, "Talk to Plugsalot") + 26.0f;
    talkTape = juce::Rectangle<float>((float) r.getX(), (float) r.getBottom() - 25.0f, tw, 25.0f);
    r.removeFromBottom(25 + 10);

    juce::FlexBox fb;
    fb.flexWrap = juce::FlexBox::Wrap::wrap;
    fb.justifyContent = juce::FlexBox::JustifyContent::flexStart;
    fb.alignContent = juce::FlexBox::AlignContent::flexStart;
    const auto qFont = vintage::condensed(14.0f, false);
    for (auto* b : quick)
        fb.items.add(juce::FlexItem(*b).withWidth(vintage::textWidth(qFont, b->getButtonText()) + 22.0f).withHeight(30).withMargin({ 0, 6, 6, 0 }));
    const int rows = 2;
    auto quickArea = r.removeFromBottom(rows * 36 - 6);
    fb.performLayout(quickArea);
    r.removeFromBottom(10 - 6 + 0);
    history.setBounds(r);

    tweakTape = juce::Rectangle<float>(5.0f, (float) r.getBottom() - 12.0f, 190.0f, 22.0f);
}

} // namespace plugsalot
