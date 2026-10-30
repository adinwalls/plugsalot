// Plugsalot - GPL-3.0-or-later
// The vintage-rack look: fonts, colours and the painting helpers the UI shares
// (wood case, enamel panels, masking tape, screws, knobs, lamps, VU meter).
// Everything is laid out in a 1280 x 800 "design space" and scaled by the editor.
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugsalot::vintage
{

constexpr int designWidth = 1280, designHeight = 800;

// ---- fonts (embedded; Barlow + Barlow Condensed are OFL, Permanent Marker is Apache-2.0) ----
juce::Font condensed(float height, bool bold = true);   // Barlow Condensed SemiBold / Bold
juce::Font condensedMedium(float height);
juce::Font body(float height, bool medium = false);     // Barlow Regular / Medium
juce::Font marker(float height);                        // Permanent Marker (masking-tape writing)
juce::Typeface::Ptr bodyTypeface();
inline float textWidth(const juce::Font& f, const juce::String& s) { return juce::GlyphArrangement::getStringWidth(f, s); }

// ---- colours ----
namespace col
{
inline const juce::Colour enamel      { 0xff2b3443 };
inline const juce::Colour slotTop     { 0xff0e1216 };
inline const juce::Colour slotBottom  { 0xff171c23 };
inline const juce::Colour rackBack    { 0xff0a0d11 };
inline const juce::Colour tapeBody    { 0xffece3c6 };
inline const juce::Colour tapeInk     { 0xff23262b };
inline const juce::Colour cream       { 0xffefe7d2 };
inline const juce::Colour textHi      { 0xffe6ebf1 };
inline const juce::Colour textMid     { 0xffc3ccd7 };
inline const juce::Colour textLow     { 0xffa9b4c2 };
inline const juce::Colour gold        { 0xffe3a857 };
inline const juce::Colour you         { 0xffe3a857 };
inline const juce::Colour plugsalot   { 0xff9fb3c8 };
inline const juce::Colour note        { 0xffec7658 };
inline const juce::Colour assistant   { 0xff212833 };
inline const juce::Colour transcript  { 0xff0f1318 };
inline const juce::Colour inputBack   { 0xff11161c };
inline const juce::Colour darkButton  { 0xff1b222b };
inline const juce::Colour darkBorder  { 0xff455164 };
inline const juce::Colour tipBack     { 0xff1a2029 };
inline const juce::Colour ledOn       { 0xff6ee07c };
inline const juce::Colour ledBusy     { 0xfff0b45c };
}

// Knob colour families: cap, highlight and pointer colour.
struct KnobColour { juce::Colour cap, hi, pointer; };
KnobColour knobColour(const juce::String& name);   // blue, red, grey, cream, black

// ---- painters ----
void drawWood(juce::Graphics&, juce::Rectangle<int>);
void drawScrew(juce::Graphics&, juce::Point<float> centre, float diameter = 11.0f, float angleRadians = 0.7f);
// Masking tape with ragged ends; text is written in marker pen. `tilt` in degrees.
void drawTape(juce::Graphics&, juce::Rectangle<float> area, const juce::String& text, float fontHeight, float tiltDegrees,
              juce::Justification just = juce::Justification::centred);
void drawKnob(juce::Graphics&, juce::Point<float> centre, const juce::String& colourName, float norm, float ringSize = 52.0f, float capSize = 40.0f);
void drawLed(juce::Graphics&, juce::Point<float> centre, float diameter, bool on, juce::Colour colour = col::ledOn);
void drawCapButton(juce::Graphics&, juce::Rectangle<float>, bool pressed, bool dim);
void drawDarkButton(juce::Graphics&, juce::Rectangle<float>, bool over, bool down);
void drawBezel(juce::Graphics&, juce::Rectangle<float>, float corner);
void drawLamp(juce::Graphics&, juce::Rectangle<float>, bool over, bool down);
void drawVu(juce::Graphics&, juce::Rectangle<float>, float needle01);   // 0..1 across the arc
void drawTooltipBox(juce::Graphics&, juce::Rectangle<float>);

} // namespace plugsalot::vintage
