// Plugsalot - GPL-3.0-or-later
// The small key display on a TuneBrew slot: an amber readout of the detected key (e.g. "A MIN"),
// a tiny LED for how sure the detector is, and a LOCK button that glows red once you commit to the key.
// Sized to fit inside any rack card (it lays itself out from its own width).
#pragma once

#include "PluginProcessor.h"
#include "Vintage.h"

namespace plugsalot
{

class KeyDisplay final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    static constexpr int height = 44;

    explicit KeyDisplay(PlugsalotProcessor& p) : proc(p)
    {
        setTooltip("Song key, heard from the input (works best on one voice or instrument).\n"
                   "LED: how sure the detector is. Amber = guessing, green = confident.\n"
                   "LOCK: commit to this key. It stops re-detecting and sets TuneBrew to it. Click again to listen afresh.");
        setMouseCursor(juce::MouseCursor::PointingHandCursor);
        poll();
        startTimerHz(8);
    }

    // The text in the readout for a key index; "- -" while nothing is known yet.
    static juce::String readout(int keyIndex)
    {
        static const char* const names[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        if (keyIndex < 0)
            return "- -";
        return juce::String(names[keyIndex % 12]) + (keyIndex < 12 ? " MAJ" : " MIN");
    }

    void paint(juce::Graphics& g) override
    {
        const auto r = getLocalBounds().toFloat();
        vintage::drawBezel(g, r, 4.0f);

        // readout
        const auto lcd = juce::Rectangle<float>(r.getX() + 4.0f, r.getY() + 4.0f, r.getWidth() - 8.0f, 20.0f);
        g.setColour(juce::Colour(0xff0a0805));
        g.fillRoundedRectangle(lcd, 2.5f);
        const bool known = shown.keyIndex >= 0;
        const juce::Colour amber(0xffffb347);
        g.setFont(vintage::condensed(17.0f).withExtraKerningFactor(0.12f));
        if (known)
        {
            g.setColour(amber.withAlpha(0.25f));            // soft glow behind the characters
            g.drawText(readout(shown.keyIndex), lcd.translated(0.0f, 0.5f).expanded(1.0f), juce::Justification::centred);
        }
        g.setColour(known ? amber : amber.withAlpha(0.28f));
        g.drawText(readout(shown.keyIndex), lcd, juce::Justification::centred);

        // confidence LED + label
        const float cy = r.getBottom() - 11.0f;
        const float conf = shown.keyIndex < 0 ? 0.0f : shown.keyConfidence;
        const bool lit = shown.keyIndex >= 0;
        const auto ledColour = conf >= 0.7f ? vintage::col::ledOn : conf >= 0.35f ? juce::Colour(0xffd3dc5c) : vintage::col::ledBusy;
        vintage::drawLed(g, { r.getX() + 11.0f, cy }, 8.0f, lit, ledColour);
        g.setColour(vintage::col::textLow);
        g.setFont(vintage::condensed(9.0f, false).withExtraKerningFactor(0.12f));
        g.drawText("SURE", juce::Rectangle<float>(r.getX() + 18.0f, cy - 6.0f, 26.0f, 12.0f), juce::Justification::centredLeft);

        // lock button
        const auto b = lockBounds();
        const bool can = shown.keyIndex >= 0;
        if (shown.keyLocked)
        {
            g.setColour(juce::Colour(0x55ff3b30));        // red glow
            g.fillRoundedRectangle(b.expanded(2.5f), 5.0f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffff6a5c), b.getX(), b.getY(), juce::Colour(0xffb3170e), b.getX(), b.getBottom(), false));
        }
        else
        {
            g.setGradientFill(juce::ColourGradient(juce::Colour(can ? 0xff3a4452 : 0xff262d37), b.getX(), b.getY(),
                                                   juce::Colour(can ? 0xff222a34 : 0xff1a2028), b.getX(), b.getBottom(), false));
        }
        g.fillRoundedRectangle(b, 3.0f);
        g.setColour(shown.keyLocked ? juce::Colour(0xff5a0803) : juce::Colour(0xff0d1116));
        g.drawRoundedRectangle(b, 3.0f, 1.0f);
        g.setColour(shown.keyLocked ? juce::Colours::white : can ? vintage::col::textMid : juce::Colour(0xff5b6573));
        g.setFont(vintage::condensed(10.0f).withExtraKerningFactor(0.1f));
        g.drawText("LOCK", b, juce::Justification::centred);
    }

    void mouseUp(const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && lockBounds().expanded(4.0f).contains(e.position))
        {
            poll();
            if (shown.keyLocked || shown.keyIndex >= 0)
                proc.setKeyLocked(! shown.keyLocked);
            poll();
            repaint();
        }
    }

private:
    juce::Rectangle<float> lockBounds() const
    {
        return { (float) getWidth() - 4.0f - 38.0f, (float) getHeight() - 4.0f - 15.0f, 38.0f, 15.0f };
    }

    void poll()
    {
        const auto a = proc.analysis();
        // The LED only needs coarse steps, so repaint just when something visible changed.
        const bool changed = a.keyIndex != shown.keyIndex || a.keyLocked != shown.keyLocked
                          || ledStep(a.keyConfidence) != ledStep(shown.keyConfidence);
        shown.keyIndex = a.keyIndex; shown.keyConfidence = a.keyConfidence; shown.keyLocked = a.keyLocked;
        if (changed)
            repaint();
    }
    static int ledStep(float c) { return c >= 0.7f ? 2 : c >= 0.35f ? 1 : 0; }
    void timerCallback() override { poll(); }

    PlugsalotProcessor& proc;
    AudioAnalysis shown;
};

} // namespace plugsalot
