// Plugsalot - GPL-3.0-or-later
// Look and feel for the whole plugin (see Vintage.h for the painters).
#pragma once

#include "Vintage.h"
#include <juce_gui_basics/juce_gui_basics.h>

namespace plugsalot::theme
{
// Shared colours (the vintage-rack palette; see Vintage.h for the painters).
inline const juce::Colour background { 0xff212833 };
inline const juce::Colour panel      { 0xff212833 };
inline const juce::Colour panelHi    { 0xff1b222b };
inline const juce::Colour outline    { 0xff455164 };
inline const juce::Colour text       { 0xffe6ebf1 };
inline const juce::Colour muted      { 0xffa9b4c2 };
inline const juce::Colour accent     { 0xffe3a857 };   // gold
inline const juce::Colour warning    { 0xffec7658 };
inline const juce::Colour user       { 0xffe3a857 };

class LookAndFeel final : public juce::LookAndFeel_V4
{
public:
    LookAndFeel()
    {
        setDefaultSansSerifTypeface(vintage::bodyTypeface());
        setColourScheme({ background, panel, panelHi, outline, text, accent, juce::Colours::white, panelHi, text });
        setColour(juce::ComboBox::backgroundColourId, panelHi);
        setColour(juce::ComboBox::outlineColourId, outline);
        setColour(juce::ComboBox::textColourId, text);
        setColour(juce::ComboBox::arrowColourId, muted);
        setColour(juce::PopupMenu::backgroundColourId, panelHi);
        setColour(juce::PopupMenu::textColourId, text);
        setColour(juce::PopupMenu::highlightedBackgroundColourId, outline);
        setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
        setColour(juce::TextEditor::backgroundColourId, vintage::col::inputBack);
        setColour(juce::TextEditor::outlineColourId, outline);
        setColour(juce::TextEditor::focusedOutlineColourId, warning);
        setColour(juce::TextEditor::textColourId, text);
        setColour(juce::TextEditor::highlightColourId, outline);
        setColour(juce::CaretComponent::caretColourId, text);
        setColour(juce::TextButton::buttonColourId, panelHi);
        setColour(juce::TextButton::buttonOnColourId, accent);
        setColour(juce::TextButton::textColourOffId, text);
        setColour(juce::TextButton::textColourOnId, text);
        setColour(juce::Slider::thumbColourId, accent);
        setColour(juce::Slider::trackColourId, accent.withAlpha(0.6f));
        setColour(juce::Slider::backgroundColourId, outline);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour(juce::ToggleButton::tickColourId, accent);
        setColour(juce::ScrollBar::thumbColourId, outline);
        setColour(juce::TooltipWindow::backgroundColourId, vintage::col::tipBack);
        setColour(juce::TooltipWindow::textColourId, text);
        setColour(juce::TooltipWindow::outlineColourId, accent.withAlpha(0.6f));
    }

    // ---- buttons: dark rack buttons, or cream "cap" buttons when the component has the property "cap" ----
    juce::Font getTextButtonFont(juce::TextButton& b, int) override
    {
        return vintage::condensed(b.getProperties().getWithDefault("fontSize", 15.0f), false);
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat();
        if ((bool) b.getProperties().getWithDefault("cap", false))
            vintage::drawCapButton(g, r, down, ! b.isEnabled());
        else if ((bool) b.getProperties().getWithDefault("ghost", false))
        {
            if (over) { g.setColour(juce::Colour(0x14ffffff)); g.fillRoundedRectangle(r, 3.0f); }
        }
        else
            vintage::drawDarkButton(g, r, over && b.isEnabled(), down);
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool down) override
    {
        const bool cap = (bool) b.getProperties().getWithDefault("cap", false);
        g.setFont(getTextButtonFont(b, b.getHeight()).withExtraKerningFactor(cap ? 0.02f : 0.06f));
        juce::Colour c = cap ? juce::Colour(0xff25272b) : (bool) b.getProperties().getWithDefault("ghost", false) ? muted : text;
        if (! b.isEnabled())
            c = c.withAlpha(0.45f);
        g.setColour(c);
        auto r = b.getLocalBounds().withTrimmedBottom(cap ? 2 : 0);
        if (cap && down) r.translate(0, 2);
        g.drawFittedText(b.getButtonText(), r.reduced(4, 0), juce::Justification::centred, 1);
    }

    // ---- combo boxes: dark button with a chevron ----
    void drawComboBox(juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto r = juce::Rectangle<float>(0, 0, (float) w, (float) h);
        vintage::drawDarkButton(g, r, box.isMouseOver(), false);
        juce::Path chevron;
        const float cx = (float) w - 16.0f, cy = (float) h * 0.5f;
        chevron.startNewSubPath(cx - 5.0f, cy - 2.5f);
        chevron.lineTo(cx, cy + 2.5f);
        chevron.lineTo(cx + 5.0f, cy - 2.5f);
        g.setColour(muted);
        g.strokePath(chevron, juce::PathStrokeType(1.6f));
    }
    juce::Font getComboBoxFont(juce::ComboBox&) override { return vintage::body(15.0f); }
    void positionComboBoxText(juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds(10, 1, box.getWidth() - 34, box.getHeight() - 2);
        label.setFont(getComboBoxFont(box));
    }
    juce::Font getPopupMenuFont() override { return vintage::body(15.0f); }

    // ---- tooltips: gold heading, then plain text, in the dark box from the design ----
    static juce::TextLayout tooltipLayout(const juce::String& tip)
    {
        constexpr float maxWidth = 340.0f;
        juce::AttributedString s;
        s.setJustification(juce::Justification::topLeft);
        s.setLineSpacing(2.0f);
        const auto head = tip.upToFirstOccurrenceOf("\n", false, false);
        const auto rest = tip.fromFirstOccurrenceOf("\n", false, false);
        if (rest.isEmpty())
            s.append(tip, vintage::body(13.5f), vintage::col::textHi);
        else
        {
            s.append(head + "\n", vintage::condensed(17.0f), vintage::col::gold);
            s.append(rest, vintage::body(13.5f), vintage::col::textHi);
        }
        juce::TextLayout tl;
        tl.createLayoutWithBalancedLineLengths(s, maxWidth);
        return tl;
    }

    juce::Rectangle<int> getTooltipBounds(const juce::String& tipText, juce::Point<int> screenPos,
                                          juce::Rectangle<int> parentArea) override
    {
        const auto tl = tooltipLayout(tipText);
        const int w = (int) (tl.getWidth() + 28.0f), h = (int) (tl.getHeight() + 24.0f);
        return juce::Rectangle<int>(screenPos.x > parentArea.getCentreX() ? screenPos.x - (w + 12) : screenPos.x + 24,
                                    screenPos.y > parentArea.getCentreY() ? screenPos.y - (h + 6) : screenPos.y + 6, w, h)
            .constrainedWithin(parentArea);
    }

    void drawTooltip(juce::Graphics& g, const juce::String& tipText, int width, int height) override
    {
        const auto b = juce::Rectangle<int>(width, height).toFloat().reduced(2.0f);
        vintage::drawTooltipBox(g, b);
        tooltipLayout(tipText).draw(g, b.reduced(13.0f, 11.0f));
    }
};
} // namespace plugsalot::theme
