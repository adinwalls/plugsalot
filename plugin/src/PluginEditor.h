// Plugsalot - GPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "ui/ChainPanel.h"
#include "ui/ChatPanel.h"
#include "ui/ContextBar.h"
#include "ui/KeyboardFallback.h"
#include "ui/ModuleEditor.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

namespace plugsalot
{

class PlugsalotEditor final : public juce::AudioProcessorEditor, private juce::ChangeListener
{
public:
    explicit PlugsalotEditor(PlugsalotProcessor&);
    ~PlugsalotEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    void parentHierarchyChanged() override { keyboardFallback.attachToTopLevel(); }

private:
    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void showSettings();
    void refreshStatus();

    PlugsalotProcessor& proc;
    theme::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 600 };
    ContextBar contextBar;
    ChainPanel chainPanel;
    ModuleEditor moduleEditor;
    ChatPanel chatPanel;
    std::unique_ptr<SettingsDialog> settings;
    KeyboardFallback keyboardFallback { *this };   // declared last: listens to the components above
};

} // namespace plugsalot
