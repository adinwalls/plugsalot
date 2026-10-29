// Plugsalot - GPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "ui/ChainPanel.h"
#include "ui/ChatPanel.h"
#include "ui/ContextBar.h"
#include "ui/KeyboardFallback.h"
#include "ui/ModuleEditor.h"
#include "ui/Tone3000Dialog.h"
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
    void showTone3000(std::function<void(const juce::String&)> picked);
    void refreshStatus();

    PlugsalotProcessor& proc;
    theme::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 2000 };   // module descriptions appear after a 2 s hover
    ContextBar contextBar;
    ChainPanel chainPanel;
    ModuleEditor moduleEditor;
    ChatPanel chatPanel;
    std::unique_ptr<SettingsDialog> settings;
    std::unique_ptr<Tone3000Dialog> tone3000Dialog;
    KeyboardFallback keyboardFallback { *this };   // declared last: listens to the components above
};

} // namespace plugsalot
