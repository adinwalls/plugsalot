// Plugsalot - GPL-3.0-or-later
#include "SettingsDialog.h"

namespace plugsalot
{

static constexpr const char* studioUrl = "https://aistudio.google.com/apikey";

SettingsDialog::SettingsDialog(PlugsalotProcessor& p) : proc(p)
{
    heading.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    heading.setColour(juce::Label::textColourId, theme::text);
    addAndMakeVisible(heading);

    steps.setFont(juce::FontOptions(14.0f));
    steps.setColour(juce::Label::textColourId, theme::text);
    steps.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(steps);

    provider.addItem("Google Gemini: free key (recommended)", 1);
    provider.addItem("Claude: your own API key", 2);
    provider.onChange = [this] { updateProviderUi(); };
    addAndMakeVisible(provider);

    openStudio.onClick = [this] { openStudioPage(); };
    addAndMakeVisible(openStudio);

    // The link is always visible and selectable, so the user can copy it by hand.
    link.setReadOnly(true);
    link.setCaretVisible(false);
    link.setText(studioUrl, false);
    link.setFont(juce::FontOptions(13.5f));
    link.setColour(juce::TextEditor::textColourId, theme::accent);
    addAndMakeVisible(link);

    linkNote.setFont(juce::FontOptions(12.5f));
    linkNote.setColour(juce::Label::textColourId, theme::muted);
    linkNote.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(linkNote);

    keyLabel.setText("API key", juce::dontSendNotification);
    keyLabel.setColour(juce::Label::textColourId, theme::muted);
    addAndMakeVisible(keyLabel);
    key.setPasswordCharacter((juce::juce_wchar) 0x2022);
    key.setTextToShowWhenEmpty("paste key here (Ctrl+V, or right-click > Paste)", theme::muted);
    key.onReturnKey = [this] { connect(); };
    // Save the key as soon as it is pasted or typed: Connect only tests it, so a
    // busy model or a closed card can never leave Plugsalot without the key.
    key.onTextChange = [this]
    {
        auto s = proc.loadProviderSettingsFor(currentProvider());
        s.apiKey = key.getText().trim();
        s.model = selectedModel();
        proc.saveProviderSettings(s);
        if (s.apiKey.isNotEmpty())
            showResult("Key saved. Click Connect to check it works.", theme::muted);
    };
    addAndMakeVisible(key);

    modelLabel.setText("Model", juce::dontSendNotification);
    modelLabel.setColour(juce::Label::textColourId, theme::muted);
    addAndMakeVisible(modelLabel);
    modelBox.setTextWhenNothingSelected("Automatic");
    modelBox.onChange = [this]
    {
        // Changing the model on an already-connected key takes effect straight away.
        auto saved = proc.loadProviderSettingsFor(currentProvider());
        if (saved.apiKey.isNotEmpty() && saved.apiKey == key.getText().trim())
        {
            saved.model = selectedModel();
            proc.saveProviderSettings(saved);
            showResult("Model saved: " + (saved.model.isEmpty() ? juce::String("Automatic") : saved.model) + ".", theme::accent);
        }
    };
    addAndMakeVisible(modelBox);
    modelNote.setFont(juce::FontOptions(12.0f));
    modelNote.setColour(juce::Label::textColourId, theme::muted);
    modelNote.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(modelNote);

    connectButton.setColour(juce::TextButton::buttonColourId, theme::accent.darker(0.3f));
    connectButton.onClick = [this] { connect(); };
    addAndMakeVisible(connectButton);
    closeButton.onClick = [this] { if (onClose) onClose(); };
    addAndMakeVisible(closeButton);

    result.setFont(juce::FontOptions(13.5f));
    result.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(result);

    note.setFont(juce::FontOptions(11.5f));
    note.setColour(juce::Label::textColourId, theme::muted);
    note.setJustificationType(juce::Justification::topLeft);
    addAndMakeVisible(note);

    provider.setSelectedId(proc.loadProviderSettings().provider == ProviderSettings::Provider::Claude ? 2 : 1,
                           juce::dontSendNotification);
    updateProviderUi();
}

void SettingsDialog::showMessage(const juce::String& text)
{
    showResult(text, theme::warning);
}

SettingsDialog::~SettingsDialog()
{
    alive->store(false);
    pool.removeAllJobs(true, 4000);
}

// ---------------------------------------------------------------------------
// Opening the browser
// ---------------------------------------------------------------------------

bool SettingsDialog::openInBrowser(const juce::String& url)
{
#if JUCE_LINUX || JUCE_BSD
    // DAWs such as Ardour run plugins with their own bundled libraries
    // (LD_LIBRARY_PATH), which can make xdg-open and the browser fail silently.
    // Launch with those variables cleared, and check that the opener succeeded.
    const auto q = url.quoted('\'');
    const juce::String cmd = "env -u LD_LIBRARY_PATH -u LD_PRELOAD -u GTK_PATH -u GIO_MODULE_DIR "
                             "sh -c 'xdg-open \"$0\" || gio open \"$0\" || sensible-browser \"$0\" || firefox \"$0\"' " + q;
    juce::ChildProcess child;
    if (child.start(juce::StringArray { "/bin/sh", "-c", cmd }, 0))
    {
        if (child.waitForProcessToFinish(5000))
            return child.getExitCode() == 0;
        return true; // still running: some openers stay in the foreground while the browser starts
    }
    return juce::URL(url).launchInDefaultBrowser();
#else
    return juce::URL(url).launchInDefaultBrowser();
#endif
}

void SettingsDialog::openStudioPage()
{
    juce::SystemClipboard::copyTextToClipboard(studioUrl);
    linkNote.setText("Opening your browser... The link is also copied, so you can paste it into a browser yourself.",
                     juce::dontSendNotification);
    auto isAlive = alive;
    pool.addJob([this, isAlive]
    {
        const bool ok = openInBrowser(studioUrl);
        juce::MessageManager::callAsync([this, ok, isAlive]
        {
            if (! isAlive->load())
                return;
            linkNote.setText(ok ? "Opened in your browser. If you don't see it, the link is copied: paste it into your browser."
                                : "Couldn't open your browser from here. The link is copied: open your browser and paste it.",
                             juce::dontSendNotification);
            linkNote.setColour(juce::Label::textColourId, ok ? theme::muted : theme::warning);
        });
    });
}

// ---------------------------------------------------------------------------
// Provider and models
// ---------------------------------------------------------------------------

void SettingsDialog::updateProviderUi()
{
    const bool gemini = isGemini();
    heading.setText(gemini ? "Turn on Plugsalot AI (free)" : "Use your Claude API key", juce::dontSendNotification);
    steps.setText(gemini
        ? "Plugsalot uses Google's free Gemini AI. It takes about a minute to set up.\n\n"
          "1.  Click Open Google AI Studio (or paste the link below into your browser) and sign in with any Google account. No credit card needed.\n"
          "2.  Click Create API key, and copy the key.\n"
          "3.  Paste it below and click Connect."
        : "Paste an API key from the Claude Console (console.anthropic.com). Requests are billed to your Anthropic account.",
        juce::dontSendNotification);
    openStudio.setVisible(gemini);
    link.setVisible(gemini);
    linkNote.setVisible(gemini);
    linkNote.setText({}, juce::dontSendNotification);
    modelNote.setText(gemini
        ? "Automatic uses the best free Flash model and switches to another if one is busy. Pro models usually need billing turned on in Google AI Studio."
        : "Automatic uses " + juce::String(LlmClient::defaultClaudeModel) + " (fast, low cost). Sonnet gives more detailed advice.",
        juce::dontSendNotification);

    // Each provider keeps its own key and model.
    auto saved = proc.loadProviderSettingsFor(currentProvider());
    key.setText(saved.apiKey, false);
    setModelList({}, saved.model);
    if (saved.apiKey.isNotEmpty())
        loadModels(saved.apiKey);

    note.setText(gemini
        ? "Your key is stored only on this computer, in Plugsalot's settings file. Google's free tier has daily limits "
          "and may use requests to improve its products. Supporters on Patreon will get a hosted AI with no setup (coming later)."
        : "Your key is stored only on this computer, in Plugsalot's settings file.",
        juce::dontSendNotification);
    result.setText({}, juce::dontSendNotification);
    resized();
}

void SettingsDialog::setModelList(const std::vector<ModelInfo>& models, const juce::String& select)
{
    modelBox.clear(juce::dontSendNotification);
    modelIds.clear();
    modelBox.addItem(isGemini() ? "Automatic: best free model (recommended)" : "Automatic: Claude Haiku (recommended)", 1);
    modelIds.add({});
    for (auto& m : models)
    {
        modelBox.addItem(m.label, modelIds.size() + 1);
        modelIds.add(m.id);
    }
    if (select.isNotEmpty() && ! modelIds.contains(select))
    {
        // A saved model that isn't (or isn't yet) in the list stays selectable.
        modelBox.addItem(select, modelIds.size() + 1);
        modelIds.add(select);
    }
    modelBox.setSelectedItemIndex(juce::jmax(0, modelIds.indexOf(select)), juce::dontSendNotification);
}

juce::String SettingsDialog::selectedModel() const
{
    const int i = modelBox.getSelectedItemIndex();
    return i >= 0 && i < modelIds.size() ? modelIds[i] : juce::String();
}

void SettingsDialog::loadModels(const juce::String& apiKey)
{
    ProviderSettings s;
    s.provider = currentProvider();
    s.apiKey = apiKey;
    auto isAlive = alive;
    pool.addJob([this, s, isAlive]
    {
        std::vector<ModelInfo> models;
        auto r = LlmClient::listModels(s, models);
        juce::MessageManager::callAsync([this, s, r, models, isAlive]
        {
            if (! isAlive->load() || s.provider != currentProvider() || key.getText().trim() != s.apiKey)
                return;
            if (! models.empty())
                setModelList(models, selectedModel());
        });
    });
}

void SettingsDialog::showResult(const juce::String& text, juce::Colour colour)
{
    result.setColour(juce::Label::textColourId, colour);
    result.setText(text, juce::dontSendNotification);
}

void SettingsDialog::connect()
{
    ProviderSettings s;
    s.provider = currentProvider();
    s.apiKey = key.getText().trim();
    s.model = selectedModel();
    if (s.apiKey.isEmpty())
    {
        showResult("Paste your key first.", theme::warning);
        return;
    }
    showResult("Checking the key...", theme::muted);
    connectButton.setEnabled(false);

    auto isAlive = alive;
    pool.addJob([this, s, isAlive]
    {
        // 1. Listing models proves the key works, and fills the dropdown.
        std::vector<ModelInfo> models;
        auto listed = LlmClient::listModels(s, models);
        // 2. A real request through the chosen model (Automatic falls back between models).
        LlmResult test;
        const bool keyOk = listed.ok || (! listed.isKeyRejected() && ! models.empty() && listed.status != 0);
        if (keyOk)
            test = LlmClient::testConnection(s);

        juce::MessageManager::callAsync([this, s, listed, test, models, keyOk, isAlive]
        {
            if (! isAlive->load())
                return;
            connectButton.setEnabled(true);
            if (! models.empty())
                setModelList(models, s.model);

            if (! keyOk)
            {
                showResult(listed.error, theme::warning);
                return;
            }
            // The key works: save it even if the model is busy right now.
            proc.saveProviderSettings(s);
            const auto tick = juce::String::fromUTF8("\xe2\x9c\x93 ");
            if (test.ok)
                showResult(tick + "Connected, using " + test.model + ". You can close this and start chatting.", theme::accent);
            else if (test.isTemporary())
                showResult(tick + "Your key works and is saved. " + test.model + " is busy on the provider's side right now; "
                           "try again in a minute or pick another model above.", theme::warning);
            else if (test.isKeyRejected())
                showResult(test.error, theme::warning);
            else
                showResult(tick + "Your key works and is saved, but " + test.model + " didn't answer: " + test.error
                           + " Pick another model above.", theme::warning);
        });
    });
}

// ---------------------------------------------------------------------------
// Layout
// ---------------------------------------------------------------------------

juce::Rectangle<int> SettingsDialog::card() const
{
    return getLocalBounds().withSizeKeepingCentre(juce::jmin(680, getWidth() - 40), juce::jmin(640, getHeight() - 30));
}

void SettingsDialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.6f));
    g.setColour(theme::panel);
    g.fillRoundedRectangle(card().toFloat(), 10.0f);
    g.setColour(theme::outline);
    g.drawRoundedRectangle(card().toFloat(), 10.0f, 1.0f);
}

void SettingsDialog::resized()
{
    auto r = card().reduced(24, 18);
    heading.setBounds(r.removeFromTop(30));
    r.removeFromTop(6);
    provider.setBounds(r.removeFromTop(28));
    r.removeFromTop(10);
    steps.setBounds(r.removeFromTop(isGemini() ? 104 : 40));
    if (isGemini())
    {
        auto row = r.removeFromTop(30);
        openStudio.setBounds(row.removeFromLeft(200));
        row.removeFromLeft(10);
        link.setBounds(row.withSizeKeepingCentre(row.getWidth(), 26));
        linkNote.setBounds(r.removeFromTop(34).withTrimmedTop(4));
    }
    r.removeFromTop(6);
    keyLabel.setBounds(r.removeFromTop(18));
    auto keyRow = r.removeFromTop(30);
    connectButton.setBounds(keyRow.removeFromRight(100));
    keyRow.removeFromRight(8);
    key.setBounds(keyRow);
    r.removeFromTop(10);
    modelLabel.setBounds(r.removeFromTop(18));
    modelBox.setBounds(r.removeFromTop(28));
    modelNote.setBounds(r.removeFromTop(34).withTrimmedTop(3));
    r.removeFromTop(4);
    result.setBounds(r.removeFromTop(54));
    auto bottom = r.removeFromBottom(30);
    closeButton.setBounds(bottom.removeFromRight(90));
    note.setBounds(r);
}

} // namespace plugsalot
