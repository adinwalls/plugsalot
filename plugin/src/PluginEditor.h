// Plugsalot - GPL-3.0-or-later
#pragma once

#include "PluginProcessor.h"
#include "ui/ChatPanel.h"
#include "ui/ContextBar.h"
#include "ui/KeyboardFallback.h"
#include "ui/ModuleEditor.h"
#include "ui/ModuleOverlay.h"
#include "ui/RackPanel.h"
#include "ui/SettingsDialog.h"
#include "ui/Theme.h"
#include "ui/Tone3000Dialog.h"

namespace plugsalot
{

// The window is laid out once in a 1280 x 800 "design space" (the vintage rack) and the
// whole thing is scaled to the window size, so it looks the same at any size.
class PlugsalotEditor final : public juce::AudioProcessorEditor, private juce::ChangeListener, private juce::Timer
{
public:
    explicit PlugsalotEditor(PlugsalotProcessor&);
    ~PlugsalotEditor() override;
    void paint(juce::Graphics&) override {}
    void resized() override;
    void parentHierarchyChanged() override { keyboardFallback.attachToTopLevel(); }

private:
    // Everything inside the scaled stage.
    struct Stage final : public juce::Component
    {
        void paint(juce::Graphics& g) override;
    };

    // The masking-tape labels that sit across panel edges (under any pop-up, above the panels).
    struct TapeLayer final : public juce::Component
    {
        TapeLayer() { setInterceptsMouseClicks(false, false); }
        void paint(juce::Graphics& g) override;
    };

    // Undo / Clear, the status lamp and the knob colour legend under the rack.
    struct BottomBar final : public juce::Component
    {
        explicit BottomBar(PlugsalotEditor& e);
        void paint(juce::Graphics&) override;
        void resized() override;
        PlugsalotEditor& owner;
        juce::TextButton undo { "UNDO" }, clear { "CLEAR" };
        juce::Rectangle<int> statusArea;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;
    void showSettings();
    void showModuleSettings(int slot);
    void closeModuleSettings();
    void showTone3000(std::function<void(const juce::String&)> picked);
    void refreshStatus();
    juce::String statusLine() const;

    PlugsalotProcessor& proc;
    theme::LookAndFeel lnf;
    juce::TooltipWindow tooltips { this, 2000 };   // module descriptions appear after a 2 s hover
    juce::ComponentBoundsConstrainer constrainer;
    Stage stage;
    TapeLayer tapes;
    ContextBar contextBar;
    RackPanel rack;
    BottomBar bottom;
    ModuleEditor moduleEditor;
    ChatPanel chatPanel;
    std::unique_ptr<ModuleOverlay> overlay;
    std::unique_ptr<SettingsDialog> settings;
    std::unique_ptr<Tone3000Dialog> tone3000Dialog;
    juce::String aiStatus;
    bool blinkOn = true;
    int blinkTicks = 0;
    KeyboardFallback keyboardFallback { *this };   // declared last: listens to the components above
};

} // namespace plugsalot
