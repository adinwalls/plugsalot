// Plugsalot - GPL-3.0-or-later
#include "PluginEditor.h"

namespace plugsalot
{

PlugsalotEditor::PlugsalotEditor(PlugsalotProcessor& p)
    : AudioProcessorEditor(p), proc(p), contextBar(p), chainPanel(p), moduleEditor(p), chatPanel(p)
{
    setLookAndFeel(&lnf);
    addAndMakeVisible(contextBar);
    addAndMakeVisible(chainPanel);
    addAndMakeVisible(moduleEditor);
    addAndMakeVisible(chatPanel);

    chainPanel.onSelect = [this](int slot) { moduleEditor.showSlot(slot); };
    contextBar.settingsButton.onClick = [this] { showSettings(); };
    proc.onNeedsApiKey = [this] { showSettings(); };
    proc.addChangeListener(this);

    setResizable(true, true);
    setResizeLimits(900, 580, 2000, 1400);
    setSize(1080, 700);
    refreshStatus();

    if (! proc.hasApiKey() && juce::SystemStats::getEnvironmentVariable("PLUGSALOT_DEMO", {}).isEmpty())
        juce::MessageManager::callAsync([sp = juce::Component::SafePointer<PlugsalotEditor>(this)]
                                        { if (sp != nullptr) sp->showSettings(); });
}

PlugsalotEditor::~PlugsalotEditor()
{
    proc.onNeedsApiKey = nullptr;
    proc.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void PlugsalotEditor::showSettings()
{
    settings = std::make_unique<SettingsDialog>(proc);
    settings->onClose = [this]
    {
        juce::MessageManager::callAsync([sp = juce::Component::SafePointer<PlugsalotEditor>(this)]
        {
            if (sp == nullptr) return;
            sp->settings.reset();
            sp->refreshStatus();
        });
    };
    addAndMakeVisible(*settings);
    settings->setBounds(getLocalBounds());
}

void PlugsalotEditor::refreshStatus()
{
    auto s = proc.loadProviderSettings();
    juce::String text;
    if (s.apiKey.isEmpty())
        text = "AI not set up";
    else
        text = juce::String(s.provider == ProviderSettings::Provider::Claude ? "Claude" : "Gemini")
             + (proc.lastModel().isNotEmpty() ? " | " + proc.lastModel() : juce::String());
    contextBar.status.setText(text, juce::dontSendNotification);
}

void PlugsalotEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    contextBar.refresh();
    chainPanel.refresh();
    moduleEditor.showSlot(chainPanel.selected());
    chatPanel.refresh();
    refreshStatus();
}

void PlugsalotEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme::background);
}

void PlugsalotEditor::resized()
{
    auto r = getLocalBounds();
    contextBar.setBounds(r.removeFromTop(56));
    r.reduce(8, 8);
    auto left = r.removeFromLeft(juce::jmax(360, (int) (r.getWidth() * 0.42f)));
    r.removeFromLeft(8);
    chainPanel.setBounds(left.removeFromTop(left.getHeight() / 2));
    left.removeFromTop(8);
    moduleEditor.setBounds(left);
    chatPanel.setBounds(r);
    if (settings != nullptr)
        settings->setBounds(getLocalBounds());
}

} // namespace plugsalot
