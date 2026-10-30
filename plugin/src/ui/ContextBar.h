// Plugsalot - GPL-3.0-or-later
// The dropdowns that tell the AI what is being recorded (layer 4 structured input).
#pragma once

#include "PluginProcessor.h"
#include "Theme.h"

namespace plugsalot
{

class ContextBar final : public juce::Component
{
public:
    explicit ContextBar(PlugsalotProcessor& p);
    void resized() override;
    void paint(juce::Graphics&) override;
    void refresh();
    // Feed the VU meter with the latest output peak (linear); call at ~30 Hz.
    void pushLevel(float peak);

    juce::TextButton settingsButton { "Settings" };
    juce::Label status;

private:
    struct Field
    {
        juce::Label label;
        juce::ComboBox box;
        juce::StringArray values; // what is sent to the AI for each item
    };

    void addField(Field& f, const juce::String& name, const juce::StringArray& items,
                  const juce::StringArray& values, std::function<juce::String&()> target);

    PlugsalotProcessor& proc;
    float needle = 0.0f;
    Field source, stage, style, character, cpu;
};

} // namespace plugsalot
