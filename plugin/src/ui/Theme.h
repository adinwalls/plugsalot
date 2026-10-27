// Plugsalot - GPL-3.0-or-later
// Placeholder visual theme for the prototype; the real design comes later.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugsalot::theme
{
inline const juce::Colour background { 0xff15171c };
inline const juce::Colour panel      { 0xff1e2128 };
inline const juce::Colour panelHi    { 0xff272b34 };
inline const juce::Colour outline    { 0xff323743 };
inline const juce::Colour text       { 0xffe6e8ec };
inline const juce::Colour muted      { 0xff9aa0aa };
inline const juce::Colour accent     { 0xff4fb3a9 };
inline const juce::Colour warning    { 0xffe0a458 };
inline const juce::Colour user       { 0xff8fb8ff };

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setColourScheme({ background, panel, panelHi, outline, text, accent, juce::Colours::white, panelHi, text });
        setColour(juce::ComboBox::backgroundColourId, panelHi);
        setColour(juce::ComboBox::outlineColourId, outline);
        setColour(juce::TextEditor::backgroundColourId, panel);
        setColour(juce::TextEditor::outlineColourId, outline);
        setColour(juce::TextEditor::focusedOutlineColourId, accent);
        setColour(juce::TextButton::buttonColourId, panelHi);
        setColour(juce::TextButton::buttonOnColourId, accent);
        setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::Slider::trackColourId, accent.withAlpha(0.6f));
        setColour(juce::Slider::backgroundColourId, outline);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ToggleButton::tickColourId, accent);
        setColour(juce::ScrollBar::thumbColourId, outline);
    }
};
} // namespace plugsalot::theme
