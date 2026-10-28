// Plugsalot - GPL-3.0-or-later
#include "ChainPanel.h"

namespace plugsalot
{

static constexpr int rowHeight = 46;

// The hover text for a module: what it does, what it suits, and where it comes from.
static juce::String describe(const ModuleSpec& m)
{
    juce::String t;
    t << m.name << "\n" << m.purpose;
    if (! m.goodFor.isEmpty())
        t << "\n\nGood for: " << m.goodFor.joinIntoString(", ");
    if (! m.avoidFor.isEmpty())
        t << "\nAvoid for: " << m.avoidFor.joinIntoString(", ");
    const auto src = m.json.getProperty("source", {});
    const auto authors = src.getProperty("authors", {}).toString();
    t << "\n\nBased on " << m.upstreamName;
    if (authors.isNotEmpty())
        t << " by " << authors;
    t << " (" << m.licence << ", open source)";
    if (! m.trackingSafe)
        t << "\nAdds latency: best kept off while recording.";
    return t;
}

ChainPanel::Row::Row(ChainPanel& o, int i) : owner(o), index(i)
{
    auto& slot = owner.proc.getChain().slots[(size_t) i];
    on.setToggleState(! slot.bypass, juce::dontSendNotification);
    on.setTooltip("On / bypassed");
    on.onClick = [this] { owner.proc.setManualBypass(index, ! on.getToggleState()); };
    remove.setTooltip("Remove from chain");
    remove.onClick = [this] { owner.proc.removeSlot(index); };
    if (auto* spec = Catalogue::embedded().find(slot.moduleId))
        setTooltip(describe(*spec));
    addAndMakeVisible(on);
    addAndMakeVisible(remove);
}

void ChainPanel::Row::paint(juce::Graphics& g)
{
    auto& slot = owner.proc.getChain().slots[(size_t) index];
    auto* spec = Catalogue::embedded().find(slot.moduleId);
    const bool isSelected = owner.selectedSlot == index;
    auto r = getLocalBounds().reduced(2).toFloat();
    g.setColour(isSelected ? theme::panelHi.brighter(0.08f) : theme::panelHi);
    g.fillRoundedRectangle(r, 6.0f);
    if (isSelected)
    {
        g.setColour(theme::accent);
        g.drawRoundedRectangle(r, 6.0f, 1.5f);
    }
    auto text = getLocalBounds().reduced(12, 6).withTrimmedLeft(28).withTrimmedRight(40);
    g.setColour(slot.bypass ? theme::muted : theme::text);
    g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
    g.drawText(juce::String(index + 1) + ".  " + (spec ? spec->name : slot.moduleId), text.removeFromTop(18),
               juce::Justification::centredLeft);
    g.setColour(theme::muted);
    g.setFont(juce::FontOptions(11.5f));
    g.drawText(spec ? spec->upstreamName + "  |  " + spec->licence : juce::String(), text, juce::Justification::centredLeft);
}

void ChainPanel::Row::resized()
{
    auto r = getLocalBounds().reduced(8, 0);
    on.setBounds(r.removeFromLeft(26).withSizeKeepingCentre(24, 24));
    remove.setBounds(r.removeFromRight(28).withSizeKeepingCentre(26, 24));
}

void ChainPanel::Row::mouseUp(const juce::MouseEvent&)
{
    owner.selectedSlot = index;
    owner.repaint();
    for (auto* row : owner.rows)
        row->repaint();
    if (owner.onSelect)
        owner.onSelect(index);
}

ChainPanel::ChainPanel(PlugsalotProcessor& p) : proc(p)
{
    undoButton.onClick = [this] { proc.undo(); };
    clearButton.onClick = [this] { proc.clearChain(); };
    addAndMakeVisible(undoButton);
    addAndMakeVisible(clearButton);
    footer.setColour(juce::Label::textColourId, theme::muted);
    footer.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(footer);
    refresh();
}

void ChainPanel::refresh()
{
    rows.clear();
    const int n = (int) proc.getChain().slots.size();
    for (int i = 0; i < n; ++i)
        addAndMakeVisible(rows.add(new Row(*this, i)));
    if (selectedSlot >= n)
        selectedSlot = n - 1;
    undoButton.setEnabled(proc.canUndo());
    clearButton.setEnabled(n > 0);
    footer.setText(n == 0 ? "No chain yet."
                          : juce::String(n) + " module" + (n == 1 ? "" : "s") + "  |  "
                                + juce::String(proc.chainCpuPoints()) + " CPU pts (est.)  |  latency "
                                + juce::String(proc.getLatencySamples()) + " samples",
                   juce::dontSendNotification);
    resized();
    repaint();
}

void ChainPanel::paint(juce::Graphics& g)
{
    g.fillAll(theme::panel);
    g.setColour(theme::text);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText("CHAIN", 14, 8, 200, 18, juce::Justification::centredLeft);
    if (rows.isEmpty())
    {
        g.setColour(theme::muted);
        g.setFont(juce::FontOptions(13.0f));
        g.drawFittedText("Ask the assistant for a chain.\nModules appear here in processing order.",
                         getLocalBounds().reduced(20).withTrimmedTop(30).withHeight(60), juce::Justification::centred, 3);
    }
}

void ChainPanel::resized()
{
    auto r = getLocalBounds().reduced(8);
    r.removeFromTop(24);
    auto bottom = r.removeFromBottom(30);
    undoButton.setBounds(bottom.removeFromLeft(70));
    bottom.removeFromLeft(6);
    clearButton.setBounds(bottom.removeFromLeft(70));
    footer.setBounds(bottom.withTrimmedLeft(8));
    r.removeFromBottom(6);
    for (auto* row : rows)
        row->setBounds(r.removeFromTop(rowHeight));
}

} // namespace plugsalot
