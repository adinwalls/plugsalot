// Plugsalot - GPL-3.0-or-later
#include "ContextBar.h"

namespace plugsalot
{

ContextBar::ContextBar(PlugsalotProcessor& p) : proc(p)
{
    const juce::StringArray sources { "Lead vocal", "Backing vocals", "Acoustic guitar", "Electric guitar (DI)",
                                      "Electric guitar (amp)", "Bass (DI)", "Bass (amp)", "Kick", "Snare", "Full kit",
                                      "Software drums", "Synth pad", "Synth lead", "Synth bass", "Keys", "Bus", "Master" };
    const juce::StringArray styles { "Pop", "Rock", "Hip-hop", "Electronic", "Folk", "Metal", "Jazz", "R&B", "Country", "Ambient" };
    const juce::StringArray characters { "Clean", "Warm", "Vintage", "Bright", "Aggressive" };

    addField(source, "Source", sources, sources, [this]() -> juce::String& { return proc.context().source; });
    addField(stage, "Stage", { "Tracking", "Mixing", "Mastering" }, { "tracking", "mixing", "mastering" },
             [this]() -> juce::String& { return proc.context().stage; });
    addField(style, "Style", styles, styles, [this]() -> juce::String& { return proc.context().style; });
    addField(character, "Character", characters, characters, [this]() -> juce::String& { return proc.context().character; });
    addField(cpu, "CPU", { "Light", "Normal", "High" }, { "light", "normal", "high" },
             [this]() -> juce::String& { return proc.context().cpuMode; });

    addAndMakeVisible(settingsButton);
    status.setColour(juce::Label::textColourId, theme::muted);
    status.setJustificationType(juce::Justification::centredRight);
    status.setFont(juce::FontOptions(12.0f));
    addAndMakeVisible(status);
    refresh();
}

void ContextBar::addField(Field& f, const juce::String& name, const juce::StringArray& items,
                          const juce::StringArray& values, std::function<juce::String&()> target)
{
    f.values = values;
    f.label.setText(name, juce::dontSendNotification);
    f.label.setFont(juce::FontOptions(11.0f));
    f.label.setColour(juce::Label::textColourId, theme::muted);
    addAndMakeVisible(f.label);
    f.box.addItemList(items, 1);
    f.box.onChange = [&f, target]
    {
        const int i = f.box.getSelectedItemIndex();
        if (i >= 0)
            target() = f.values[i];
    };
    addAndMakeVisible(f.box);
}

void ContextBar::refresh()
{
    auto sync = [](Field& f, const juce::String& value)
    {
        const int i = f.values.indexOf(value);
        f.box.setSelectedItemIndex(juce::jmax(0, i), juce::dontSendNotification);
    };
    auto& c = proc.context();
    sync(source, c.source);
    sync(stage, c.stage);
    sync(style, c.style);
    sync(character, c.character);
    sync(cpu, c.cpuMode);
}

void ContextBar::paint(juce::Graphics& g)
{
    g.fillAll(theme::panel);
    g.setColour(theme::accent);
    g.setFont(juce::FontOptions(20.0f, juce::Font::bold));
    g.drawText("Plugsalot", 16, 0, 120, getHeight(), juce::Justification::centredLeft);
    g.setColour(theme::outline);
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float) getWidth());
}

void ContextBar::resized()
{
    auto r = getLocalBounds().reduced(8, 6);
    r.removeFromLeft(130);
    settingsButton.setBounds(r.removeFromRight(90).withSizeKeepingCentre(90, 28));
    status.setBounds(r.removeFromRight(200));
    const int widths[] = { 170, 110, 120, 120, 90 };
    Field* fields[] = { &source, &stage, &style, &character, &cpu };
    for (int i = 0; i < 5; ++i)
    {
        auto col = r.removeFromLeft(widths[i]).reduced(4, 0);
        fields[i]->label.setBounds(col.removeFromTop(14));
        fields[i]->box.setBounds(col.removeFromTop(26));
    }
}

} // namespace plugsalot
