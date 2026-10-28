// Plugsalot - GPL-3.0-or-later
#include "ChatPanel.h"
#include "LinuxFocus.h"

namespace plugsalot
{

static constexpr const char* placeholder = "CHAT HERE: e.g. warm, intimate vocal with a little space";

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
    history.setColour(juce::TextEditor::backgroundColourId, theme::background);
    history.setColour(juce::TextEditor::outlineColourId, theme::outline);
    addAndMakeVisible(history);

    chatLabel.setText("Tell Plugsalot a bit more about what you're recording and how you want it to sound", juce::dontSendNotification);
    chatLabel.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    chatLabel.setMinimumHorizontalScale(0.75f);
    chatLabel.setColour(juce::Label::textColourId, theme::accent);
    addAndMakeVisible(chatLabel);

    // Multi-line so longer requests stay visible; Enter sends.
    input.setMultiLine(true, true);
    input.setReturnKeyStartsNewLine(false);
    input.setScrollbarsShown(true);
    input.setTextToShowWhenEmpty(placeholder, theme::muted);
    input.setFont(juce::FontOptions(16.0f));
    input.setColour(juce::TextEditor::backgroundColourId, theme::panelHi);
    input.setColour(juce::TextEditor::outlineColourId, theme::accent.withAlpha(0.5f));
    input.setColour(juce::TextEditor::focusedOutlineColourId, theme::accent);
    input.onReturnKey = [this] { send(input.getText()); };
    addAndMakeVisible(input);

    sendButton.setColour(juce::TextButton::buttonColourId, theme::accent.darker(0.3f));
    sendButton.onClick = [this] { send(input.getText()); };
    addAndMakeVisible(sendButton);

    // The typing-window fallback is for hosts that keep the keyboard; the
    // standalone app always receives keys, so it doesn't need it.
    typingWindowButton.setColour(juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    typingWindowButton.setColour(juce::TextButton::textColourOffId, theme::muted);
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

    // Typing area: about a quarter of the panel, at least three lines.
    const int inputHeight = juce::jlimit(84, 150, r.getHeight() / 4);
    if (typingWindowButton.isVisible())
        typingWindowButton.setBounds(r.removeFromBottom(22).removeFromLeft(260));
    auto inputRow = r.removeFromBottom(inputHeight);
    sendButton.setBounds(inputRow.removeFromRight(90).withTrimmedTop(inputRow.getHeight() - 40));
    inputRow.removeFromRight(8);
    input.setBounds(inputRow);
    chatLabel.setBounds(r.removeFromBottom(20));
    r.removeFromBottom(2);

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
