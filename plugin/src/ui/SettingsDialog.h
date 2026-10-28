// Plugsalot - GPL-3.0-or-later
// AI provider settings, shown as an overlay inside the plugin window. For the
// free tier it doubles as the three-step Gemini key guide.
#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

namespace plugsalot
{

class SettingsDialog final : public juce::Component
{
public:
    explicit SettingsDialog(PlugsalotProcessor& p);
    ~SettingsDialog() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override {} // swallow clicks behind the card

    std::function<void()> onClose;
    void showMessage(const juce::String& text);

    // Opens a web page in the user's browser. Returns false when it could not be
    // confirmed (the caller then shows the copied link). Blocking: call off the message thread.
    static bool openInBrowser(const juce::String& url);

private:
    bool isGemini() const { return provider.getSelectedId() != 2; }
    ProviderSettings::Provider currentProvider() const
    { return isGemini() ? ProviderSettings::Provider::Gemini : ProviderSettings::Provider::Claude; }
    void updateProviderUi();
    void setModelList(const std::vector<ModelInfo>& models, const juce::String& select);
    juce::String selectedModel() const;
    void loadModels(const juce::String& apiKey);
    void connect();
    void openStudioPage();
    void showResult(const juce::String& text, juce::Colour colour);
    juce::Rectangle<int> card() const;

    PlugsalotProcessor& proc;
    juce::Label heading, steps, keyLabel, modelLabel, modelNote, result, note, linkNote;
    juce::ComboBox provider, modelBox;
    juce::StringArray modelIds;        // parallel to modelBox items; "" = automatic
    juce::TextEditor key, link;
    juce::TextButton openStudio { "Open Google AI Studio" }, connectButton { "Connect" }, closeButton { "Close" };
    juce::ThreadPool pool { 1 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace plugsalot
