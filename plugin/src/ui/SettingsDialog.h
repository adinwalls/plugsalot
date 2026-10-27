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

private:
    void loadFromSettings();
    void connect();
    void updateProviderUi();
    juce::Rectangle<int> card() const;

    PlugsalotProcessor& proc;
    juce::Label heading, steps, keyLabel, modelLabel, result, note;
    juce::ComboBox provider;
    juce::TextEditor key, model;
    juce::TextButton openStudio { "Open Google AI Studio" }, connectButton { "Connect" }, closeButton { "Close" };
    juce::ThreadPool pool { 1 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace plugsalot
