// Plugsalot - GPL-3.0-or-later
#include "SettingsDialog.h"

namespace plugsalot
{

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

    keyLabel.setText("API key", juce::dontSendNotification);
    keyLabel.setColour(juce::Label::textColourId, theme::muted);
    addAndMakeVisible(keyLabel);
    key.setPasswordCharacter((juce::juce_wchar) 0x2022);
    key.setTextToShowWhenEmpty("paste key here", theme::muted);
    addAndMakeVisible(key);

    modelLabel.setText("Model (optional)", juce::dontSendNotification);
    modelLabel.setColour(juce::Label::textColourId, theme::muted);
    addAndMakeVisible(modelLabel);
    addAndMakeVisible(model);

    openStudio.onClick = [] { juce::URL("https://aistudio.google.com/apikey").launchInDefaultBrowser(); };
    addAndMakeVisible(openStudio);
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

    loadFromSettings();
}

SettingsDialog::~SettingsDialog()
{
    alive->store(false);
    pool.removeAllJobs(true, 3000);
}

void SettingsDialog::loadFromSettings()
{
    auto s = proc.loadProviderSettings();
    provider.setSelectedId(s.provider == ProviderSettings::Provider::Claude ? 2 : 1, juce::dontSendNotification);
    updateProviderUi();
}

void SettingsDialog::updateProviderUi()
{
    const bool gemini = provider.getSelectedId() != 2;
    heading.setText(gemini ? "Turn on Plugsalot AI (free)" : "Use your Claude API key", juce::dontSendNotification);
    steps.setText(gemini
        ? "Plugsalot uses Google's free Gemini AI. It takes about a minute to set up.\n\n"
          "1.  Click Open Google AI Studio and sign in with any Google account. No credit card needed.\n"
          "2.  Click Get API key, then Create API key, and copy the key.\n"
          "3.  Paste it below and click Connect."
        : "Paste an API key from the Claude Console. Requests are billed to your Anthropic account.\n"
          "Default model: " + juce::String(LlmClient::defaultClaudeModel) + " (fast and inexpensive). "
          "Enter claude-sonnet-5-5 below for more detailed advice.",
        juce::dontSendNotification);
    openStudio.setVisible(gemini);
    model.setTextToShowWhenEmpty(gemini ? "automatic: newest Gemini Flash" : LlmClient::defaultClaudeModel, theme::muted);
    // Each provider keeps its own key and model.
    auto saved = proc.loadProviderSettingsFor(gemini ? ProviderSettings::Provider::Gemini : ProviderSettings::Provider::Claude);
    key.setText(saved.apiKey, false);
    model.setText(saved.model, false);
    note.setText(gemini
        ? "Your key is stored only on this computer, in Plugsalot's settings file. Google's free tier has daily limits "
          "and may use requests to improve its products. Supporters on Patreon will get a hosted AI with no setup (coming later)."
        : "Your key is stored only on this computer, in Plugsalot's settings file.",
        juce::dontSendNotification);
    result.setText({}, juce::dontSendNotification);
    resized();
}

void SettingsDialog::connect()
{
    ProviderSettings s;
    s.provider = provider.getSelectedId() == 2 ? ProviderSettings::Provider::Claude : ProviderSettings::Provider::Gemini;
    s.apiKey = key.getText().trim();
    s.model = model.getText().trim();
    if (s.apiKey.isEmpty())
    {
        result.setColour(juce::Label::textColourId, theme::warning);
        result.setText("Paste your key first.", juce::dontSendNotification);
        return;
    }
    result.setColour(juce::Label::textColourId, theme::muted);
    result.setText("Checking the key...", juce::dontSendNotification);
    connectButton.setEnabled(false);

    auto isAlive = alive;
    pool.addJob([this, s, isAlive]
    {
        auto r = LlmClient::testConnection(s);
        juce::MessageManager::callAsync([this, s, r, isAlive]
        {
            if (! isAlive->load())
                return;
            connectButton.setEnabled(true);
            if (r.ok)
            {
                proc.saveProviderSettings(s);
                result.setColour(juce::Label::textColourId, theme::accent);
                result.setText(juce::String::fromUTF8("\xe2\x9c\x93") + " Connected (" + r.model + "). You can close this and start chatting.",
                               juce::dontSendNotification);
            }
            else
            {
                result.setColour(juce::Label::textColourId, theme::warning);
                result.setText(r.error, juce::dontSendNotification);
            }
        });
    });
}

juce::Rectangle<int> SettingsDialog::card() const
{
    return getLocalBounds().withSizeKeepingCentre(juce::jmin(620, getWidth() - 40), juce::jmin(520, getHeight() - 40));
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
    auto r = card().reduced(24, 20);
    heading.setBounds(r.removeFromTop(30));
    r.removeFromTop(6);
    provider.setBounds(r.removeFromTop(28));
    r.removeFromTop(10);
    steps.setBounds(r.removeFromTop(118));
    if (openStudio.isVisible())
        openStudio.setBounds(r.removeFromTop(30).removeFromLeft(200));
    r.removeFromTop(10);
    keyLabel.setBounds(r.removeFromTop(18));
    auto keyRow = r.removeFromTop(30);
    connectButton.setBounds(keyRow.removeFromRight(100));
    keyRow.removeFromRight(8);
    key.setBounds(keyRow);
    r.removeFromTop(8);
    modelLabel.setBounds(r.removeFromTop(18));
    model.setBounds(r.removeFromTop(28).removeFromLeft(300));
    r.removeFromTop(8);
    result.setBounds(r.removeFromTop(40));
    auto bottom = r.removeFromBottom(30);
    closeButton.setBounds(bottom.removeFromRight(90));
    note.setBounds(r);
}

} // namespace plugsalot
