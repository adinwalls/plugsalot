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
        setColour(juce::TooltipWindow::backgroundColourId, panelHi.brighter(0.05f));
        setColour(juce::TooltipWindow::textColourId, text);
        setColour(juce::TooltipWindow::outlineColourId, accent.withAlpha(0.6f));
    }

    // Tooltips: first line as a heading, the rest as plain left-aligned text.
    static juce::TextLayout tooltipLayout(const juce::String& tip)
    {
        constexpr float maxWidth = 380.0f;
        juce::AttributedString s;
        s.setJustification(juce::Justification::topLeft);
        s.setLineSpacing(2.0f);
        const auto head = tip.upToFirstOccurrenceOf("\n", false, false);
        const auto rest = tip.fromFirstOccurrenceOf("\n", false, false);
        if (rest.isEmpty())
            s.append(tip, juce::Font(juce::FontOptions(13.5f)), theme::text);
        else
        {
            s.append(head + "\n", juce::Font(juce::FontOptions(15.0f, juce::Font::bold)), theme::accent);
            s.append(rest, juce::Font(juce::FontOptions(13.5f)), theme::text);
        }
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths(s, maxWidth);
        return tl;
    }

    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override
    {
        const auto tl = tooltipLayout(tipText);
        const int w = (int) (tl.getWidth() + 24.0f), h = (int) (tl.getHeight() + 18.0f);
        return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                    screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
            .constrainedWithin(parentArea);
    }

    void drawTooltip(juce::Graphics& g, const juce::String& tipText, int width, int height) override
    {
        const auto b = juce::Rectangle<int>(width, height).toFloat();
        g.setColour(findColour(juce::TooltipWindow::backgroundColourId));
        g.fillRoundedRectangle(b, 6.0f);
        g.setColour(findColour(juce::TooltipWindow::outlineColourId));
        g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.0f);
        tooltipLayout(tipText).draw(g, b.reduced(12.0f, 9.0f));
    }
};
} // namespace plugsalot::theme
