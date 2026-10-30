// Plugsalot - GPL-3.0-or-later
#include "ContextBar.h"

namespace plugsalot
{

ContextBar::ContextBar(PlugsalotProcessor& p) : proc(p)
{
    const juce::StringArray sources { "Lead vocal", "Backing vocals", "Acoustic guitar", "Electric guitar (DI)",
                                      "Electric guitar (amp)", "Bass (DI)", "Bass (amp)", "Kick", "Snare", "Full kit",
                                      "Software drums", "Synth pad", "Synth lead", "Synth bass", "Keys", "Bus", "Master" };
    const juce::StringArray styles { "Pop", "Rock", "Hip-hop", "Electronic", "Folk", "Metal", "Jazz", "R&B", "Country", "Ambient", "Lo-fi" };
    const juce::StringArray characters { "Clean", "Warm", "Vintage", "Bright", "Aggressive" };

    addField(source, "Source", sources, sources, [this]() -> juce::String& { return proc.context().source; });
    addField(stage, "Stage", { "Tracking", "Mixing", "Mastering" }, { "tracking", "mixing", "mastering" },
             [this]() -> juce::String& { return proc.context().stage; });
    addField(style, "Style", styles, styles, [this]() -> juce::String& { return proc.context().style; });
    addField(character, "Character", characters, characters, [this]() -> juce::String& { return proc.context().character; });
    addField(cpu, "CPU", { "Light", "Normal", "High" }, { "light", "normal", "high" },
             [this]() -> juce::String& { return proc.context().cpuMode; });

    settingsButton.setButtonText("SETTINGS");
    settingsButton.getProperties().set("fontSize", 16.0f);
    addAndMakeVisible(settingsButton);
    status.setColour(juce::Label::textColourId, vintage::col::textLow);
    status.setJustificationType(juce::Justification::centredRight);
    status.setFont(vintage::body(11.0f));
    status.setBorderSize({});
    addAndMakeVisible(status);
    refresh();
}

void ContextBar::addField(Field& f, const juce::String& name, const juce::StringArray& items,
                          const juce::StringArray& values, std::function<juce::String&()> target)
{
    f.values = values;
    f.label.setText(name, juce::dontSendNotification);
    f.label.setText(name.toUpperCase(), juce::dontSendNotification);
    f.label.setFont(vintage::condensed(10.5f, false).withExtraKerningFactor(0.14f));
    f.label.setColour(juce::Label::textColourId, vintage::col::textLow);
    f.label.setBorderSize({});
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

void ContextBar::pushLevel(float peak)
{
    const float db = peak > 1.0e-5f ? 20.0f * std::log10(peak) : -60.0f;
    const float target = juce::jlimit(0.0f, 1.0f, (db + 20.0f) / 23.0f);
    needle += (target - needle) * (target > needle ? 0.45f : 0.10f);
    repaint(getWidth() - 270, 8, 160, 70);
}

void ContextBar::paint(juce::Graphics& g)
{
    g.setGradientFill(juce::ColourGradient(juce::Colour(0x0dffffff), 0.0f, 0.0f, juce::Colour(0x1f000000), 0.0f, (float) getHeight(), false));
    g.fillAll();
    g.setColour(juce::Colour(0xff161c24));
    g.drawHorizontalLine(getHeight() - 1, 0.0f, (float) getWidth());

    g.setColour(vintage::col::cream);
    g.setFont(vintage::condensed(32.0f).withExtraKerningFactor(0.14f));
    g.drawText("PLUGSALOT", 22, 12, 200, 38, juce::Justification::centredLeft);
    g.setColour(vintage::col::textLow);
    g.setFont(vintage::condensedMedium(11.5f).withExtraKerningFactor(0.18f));
    g.drawText("BY ADIN WALLS", 23, 50, 200, 16, juce::Justification::centredLeft);

    vintage::drawVu(g, juce::Rectangle<float>((float) getWidth() - 22.0f - 100.0f - 22.0f - 150.0f, 12.0f, 150.0f, 62.0f), needle);
}

void ContextBar::resized()
{
    auto r = getLocalBounds().withTrimmedLeft(22 + 190 + 22).withTrimmedRight(22);
    auto right = r.removeFromRight(100);
    settingsButton.setBounds(right.getX(), 23, 100, 40);
    status.setBounds(right.getX() - 60, 66, 160, 13);
    const int widths[] = { 172, 112, 112, 122, 104 };
    Field* fields[] = { &source, &stage, &style, &character, &cpu };
    int x = r.getX();
    for (int i = 0; i < 5; ++i)
    {
        fields[i]->label.setBounds(x, 22, widths[i], 14);
        fields[i]->box.setBounds(x, 40, widths[i], 34);
        x += widths[i] + 10;
    }
}

} // namespace plugsalot
