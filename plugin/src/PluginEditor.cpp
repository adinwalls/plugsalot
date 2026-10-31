// Plugsalot - GPL-3.0-or-later
#include "PluginEditor.h"

namespace plugsalot
{

namespace
{
using vintage::designHeight;
using vintage::designWidth;

// Where things sit in the design space.
constexpr int headerX = 34, headerY = 20, headerW = 1212, headerH = 86;
constexpr int rackX = 50, rackY = 120, rackW = 816, rackH = 600;
constexpr int barY = rackY + rackH + 10, barH = 36;
constexpr int chatX = rackX + rackW + 16, chatY = 120, chatW = 1230 - chatX, chatH = 646;
}

// ---------------------------------------------------------------------------
// Stage
// ---------------------------------------------------------------------------

void PlugsalotEditor::Stage::paint(juce::Graphics& g)
{
    vintage::drawWood(g, getLocalBounds());

    // As in the design: the wood is only the case frame (20px top and bottom, 34px at the sides);
    // everything inside sits on the dark enamel panel.
    const auto panel = juce::Rectangle<float>((float) headerX, (float) headerY,
                                              (float) (designWidth - 2 * headerX), (float) (designHeight - 2 * headerY));
    g.setColour(juce::Colour(0xff1a130c));
    g.drawRoundedRectangle(panel.expanded(1.0f), 5.5f, 2.0f);
    g.setColour(juce::Colour::fromFloatRGBA(1.0f, 0.86f, 0.67f, 0.15f));
    g.drawRoundedRectangle(panel.expanded(2.5f), 6.5f, 1.0f);
    g.setColour(vintage::col::enamel);
    g.fillRoundedRectangle(panel, 5.0f);
    g.setColour(juce::Colour(0x14ffffff));
    g.drawHorizontalLine((int) panel.getY(), panel.getX() + 5.0f, panel.getRight() - 5.0f);
}

void PlugsalotEditor::TapeLayer::paint(juce::Graphics& g)
{
    // Masking-tape labels that sit across panel edges.
    const auto chain = vintage::marker(13.0f);
    const float cw = vintage::textWidth(chain, "YOUR CHAIN: SIGNAL FLOWS LEFT TO RIGHT") + 30.0f;
    vintage::drawTape(g, { (float) rackX + 22.0f, (float) rackY - 10.0f, cw, 24.0f }, "YOUR CHAIN: SIGNAL FLOWS LEFT TO RIGHT", 13.0f, -1.0f);
    const auto guide = vintage::marker(11.0f);
    const float gw = vintage::textWidth(guide, "KNOB COLOUR GUIDE") + 22.0f;
    vintage::drawTape(g, { (float) (rackX + rackW) - 6.0f - gw, (float) (rackY + rackH + 10 + barH) - 30.0f - 20.0f, gw, 20.0f },
                      "KNOB COLOUR GUIDE", 11.0f, -2.0f);
}

// ---------------------------------------------------------------------------
// Bottom bar
// ---------------------------------------------------------------------------

PlugsalotEditor::BottomBar::BottomBar(PlugsalotEditor& e) : owner(e)
{
    for (auto* b : { &undo, &clear })
    {
        b->getProperties().set("fontSize", 14.0f);
        addAndMakeVisible(*b);
    }
    undo.onClick = [this] { owner.proc.undo(); };
    clear.onClick = [this] { owner.proc.clearChain(); };
}

void PlugsalotEditor::BottomBar::resized()
{
    auto r = getLocalBounds().withSizeKeepingCentre(getWidth(), 32);
    undo.setBounds(r.removeFromLeft(66));
    r.removeFromLeft(8);
    clear.setBounds(r.removeFromLeft(70));
    r.removeFromLeft(16);
    statusArea = r;
}

void PlugsalotEditor::BottomBar::paint(juce::Graphics& g)
{
    const bool busy = owner.proc.isAiBusy() || owner.rack.busyText().isNotEmpty();
    const float cy = (float) getHeight() * 0.5f;
    const float lx = (float) statusArea.getX() + 5.0f;
    vintage::drawLed(g, { lx, cy }, 10.0f, true, busy ? (owner.blinkOn ? vintage::col::ledBusy : vintage::col::ledBusy.withAlpha(0.2f)) : vintage::col::ledOn);
    g.setColour(vintage::col::textMid);
    g.setFont(vintage::body(13.0f));
    const auto text = owner.statusLine();
    g.drawText(text, (int) lx + 14, 0, 380, getHeight(), juce::Justification::centredLeft);

    // knob colour legend, right-aligned
    struct Item { const char* label; const char* colour; };
    const Item items[] = { { "TONE", "blue" }, { "DYNAMICS", "red" }, { "LEVEL", "grey" }, { "TIME + SPACE", "cream" }, { "FILTER", "black" } };
    const auto font = vintage::condensed(12.5f, false).withExtraKerningFactor(0.06f);
    float x = (float) getWidth();
    for (int i = 4; i >= 0; --i)
    {
        const float tw = vintage::textWidth(font, items[i].label);
        x -= tw;
        g.setColour(vintage::col::textMid);
        g.setFont(font);
        g.drawText(items[i].label, juce::Rectangle<float>(x, 0.0f, tw + 2.0f, (float) getHeight()), juce::Justification::centredLeft);
        x -= 17.0f;
        const auto kc = vintage::knobColour(items[i].colour);
        const auto dot = juce::Rectangle<float>(12.0f, 12.0f).withCentre({ x + 6.0f, cy });
        g.setColour(juce::Colour(0x99000000));
        g.fillEllipse(dot.translated(0.0f, 1.0f));
        g.setGradientFill(juce::ColourGradient(kc.hi, dot.getX() + 4.0f, dot.getY() + 3.6f, kc.cap, dot.getX() + 4.0f + 7.8f, dot.getY() + 3.6f, true));
        g.fillEllipse(dot);
        x -= 14.0f;
    }
}

// ---------------------------------------------------------------------------
// Editor
// ---------------------------------------------------------------------------

PlugsalotEditor::PlugsalotEditor(PlugsalotProcessor& p)
    : AudioProcessorEditor(p), proc(p), contextBar(p), rack(p), bottom(*this), moduleEditor(p), chatPanel(p)
{
    setLookAndFeel(&lnf);
    addAndMakeVisible(stage);
    stage.addAndMakeVisible(contextBar);
    stage.addAndMakeVisible(rack);
    stage.addAndMakeVisible(bottom);
    stage.addAndMakeVisible(chatPanel);
    stage.addAndMakeVisible(tapes);

    rack.onOpenSettings = [this](int slot) { showModuleSettings(slot); };
    rack.onStatusChanged = [this] { bottom.repaint(); };
    // TONE3000 browsing stays built in but hidden (no handler, so the button never shows) until NAM captures are offered again.
    // moduleEditor.onBrowseTone3000 = [this](int, std::function<void(const juce::String&)> done) { showTone3000(std::move(done)); };
    contextBar.settingsButton.onClick = [this] { showSettings(); };
    proc.onNeedsApiKey = [this]
    {
        showSettings();
        settings->showMessage("Add your API key here first: paste it into the API key box (it saves straight away), then Close and send your message again.");
    };
    proc.addChangeListener(this);

    stage.setBounds(0, 0, designWidth, designHeight);
    tapes.setBounds(stage.getLocalBounds());
    contextBar.setBounds(headerX, headerY, headerW, headerH);
    rack.setBounds(rackX, rackY, rackW, rackH);
    bottom.setBounds(rackX, barY, rackW, barH);
    chatPanel.setBounds(chatX, chatY, chatW, chatH);

    constrainer.setFixedAspectRatio((double) designWidth / (double) designHeight);
    constrainer.setSizeLimits(640, 400, 2560, 1600);
    setConstrainer(&constrainer);
    setResizable(true, true);
    setSize(1120, 700);
    bottom.undo.setEnabled(proc.canUndo());
    bottom.clear.setEnabled(! proc.getChain().isEmpty());
    refreshStatus();
    startTimerHz(30);

    if (! proc.hasApiKey() && juce::SystemStats::getEnvironmentVariable("PLUGSALOT_DEMO", {}).isEmpty())
        juce::MessageManager::callAsync([sp = juce::Component::SafePointer<PlugsalotEditor>(this)]
                                        { if (sp != nullptr) sp->showSettings(); });
}

PlugsalotEditor::~PlugsalotEditor()
{
    stopTimer();
    overlay.reset();
    proc.onNeedsApiKey = nullptr;
    proc.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void PlugsalotEditor::timerCallback()
{
    contextBar.pushLevel(proc.takeOutputPeak());
    if (++blinkTicks >= 10)
    {
        blinkTicks = 0;
        blinkOn = ! blinkOn;
        if (proc.isAiBusy() || rack.busyText().isNotEmpty())
            bottom.repaint();
    }
}

juce::String PlugsalotEditor::statusLine() const
{
    if (rack.busyText().isNotEmpty())
        return rack.busyText();
    if (proc.isAiBusy())
        return "Choosing modules...";
    const int n = (int) proc.getChain().slots.size();
    return juce::String(n) + (n == 1 ? " module" : " modules") + "  |  " + juce::String(proc.chainCpuPoints())
           + " CPU pts  |  latency " + juce::String(proc.getLatencySamples()) + " samples";
}

void PlugsalotEditor::showSettings()
{
    settings = std::make_unique<SettingsDialog>(proc);
    juce::Component::SafePointer<PlugsalotEditor> safeThis(this);
    settings->onClose = [safeThis]
    {
        juce::MessageManager::callAsync([sp = safeThis]
        {
            if (sp == nullptr) return;
            sp->settings.reset();
            sp->refreshStatus();
        });
    };
    stage.addAndMakeVisible(*settings);
    settings->setBounds(stage.getLocalBounds());
}

void PlugsalotEditor::showModuleSettings(int slot)
{
    if (slot < 0 || slot >= (int) proc.getChain().slots.size())
        return;
    closeModuleSettings();
    moduleEditor.showSlot(slot);
    overlay = std::make_unique<ModuleOverlay>(proc, moduleEditor, slot);
    juce::Component::SafePointer<PlugsalotEditor> safeThis(this);
    overlay->onClose = [safeThis]
    {
        juce::MessageManager::callAsync([sp = safeThis] { if (sp != nullptr) sp->closeModuleSettings(); });
    };
    stage.addAndMakeVisible(*overlay);
    overlay->setBounds(stage.getLocalBounds());
    overlay->toFront(true);
    if (settings != nullptr) settings->toFront(false);
    if (tone3000Dialog != nullptr) tone3000Dialog->toFront(false);
}

void PlugsalotEditor::closeModuleSettings()
{
    overlay.reset();
}

void PlugsalotEditor::showTone3000(std::function<void(const juce::String&)> picked)
{
    tone3000Dialog = std::make_unique<Tone3000Dialog>(proc);
    juce::Component::SafePointer<PlugsalotEditor> safeThis(this);
    tone3000Dialog->onPicked = std::move(picked);
    tone3000Dialog->onClose = [safeThis]
    {
        juce::MessageManager::callAsync([sp = safeThis] { if (sp != nullptr) sp->tone3000Dialog.reset(); });
    };
    stage.addAndMakeVisible(*tone3000Dialog);
    tone3000Dialog->setBounds(stage.getLocalBounds());
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
    rack.refresh();
    if (overlay != nullptr)
    {
        if (overlay->slotIndex() >= (int) proc.getChain().slots.size())
            closeModuleSettings();
        else
            moduleEditor.showSlot(overlay->slotIndex());
    }
    bottom.undo.setEnabled(proc.canUndo());
    bottom.clear.setEnabled(! proc.getChain().isEmpty());
    bottom.repaint();
    chatPanel.refresh();
    refreshStatus();
}

void PlugsalotEditor::resized()
{
    const float scale = (float) getWidth() / (float) designWidth;
    stage.setTransform(juce::AffineTransform::scale(scale));
}

} // namespace plugsalot
