// Plugsalot - GPL-3.0-or-later
#include "Vintage.h"
#include <PlugsalotData.h>

namespace plugsalot::vintage
{

// ---------------------------------------------------------------------------
// Fonts
// ---------------------------------------------------------------------------

static juce::Typeface::Ptr loadFace(const char* originalName)
{
    for (int i = 0; i < PlugsalotData::namedResourceListSize; ++i)
    {
        const char* resName = PlugsalotData::namedResourceList[i];
        if (juce::String(PlugsalotData::getNamedResourceOriginalFilename(resName)) == originalName)
        {
            int size = 0;
            const char* data = PlugsalotData::getNamedResource(resName, size);
            if (data != nullptr)
                return juce::Typeface::createSystemTypefaceFor(data, (size_t) size);
        }
    }
    return {};
}

static juce::Font make(const juce::Typeface::Ptr& face, float height)
{
    if (face == nullptr)
        return juce::Font(juce::FontOptions(height));
    return juce::Font(juce::FontOptions(face).withHeight(height));
}

juce::Font condensed(float h, bool bold)
{
    static const auto semi = loadFace("BarlowCondensed-SemiBold.ttf");
    static const auto heavy = loadFace("BarlowCondensed-Bold.ttf");
    return make(bold ? heavy : semi, h);
}
juce::Font condensedMedium(float h)
{
    static const auto f = loadFace("BarlowCondensed-Medium.ttf");
    return make(f, h);
}
juce::Typeface::Ptr bodyTypeface()
{
    static const auto f = loadFace("Barlow-Regular.ttf");
    return f;
}
juce::Font body(float h, bool medium)
{
    static const auto reg = loadFace("Barlow-Regular.ttf");
    static const auto med = loadFace("Barlow-Medium.ttf");
    return make(medium ? med : reg, h);
}
juce::Font marker(float h)
{
    static const auto f = loadFace("PermanentMarker-Regular.ttf");
    return make(f, h);
}

KnobColour knobColour(const juce::String& name)
{
    if (name == "red")   return { juce::Colour(0xffb4402e), juce::Colour(0xffe2725c), juce::Colours::white };
    if (name == "grey")  return { juce::Colour(0xffa9b1bb), juce::Colour(0xffeef1f4), juce::Colour(0xff1d232b) };
    if (name == "cream") return { juce::Colour(0xffe3d5b0), juce::Colour(0xfffffaea), juce::Colour(0xff2a2a2a) };
    if (name == "black") return { juce::Colour(0xff22262c), juce::Colour(0xff565d67), juce::Colour(0xfff2f2f2) };
    return { juce::Colour(0xff2f62b8), juce::Colour(0xff6b97e6), juce::Colours::white };   // blue
}

// ---------------------------------------------------------------------------
// Painters
// ---------------------------------------------------------------------------

static juce::Image renderWood(int w, int h)
{
    juce::Image img(juce::Image::RGB, w, h, true);
    juce::Graphics g(img);
    juce::ColourGradient base(juce::Colour(0xff6a3d1f), 0.0f, 0.0f, juce::Colour(0xff6b3e20), (float) w, 0.0f, false);
    base.addColour(0.25, juce::Colour(0xff9a6236));
    base.addColour(0.50, juce::Colour(0xff81502b));
    base.addColour(0.78, juce::Colour(0xffa46d3f));
    g.setGradientFill(base);
    g.fillAll();

    // Long grain: thin dark and light lines with a slight lean, spaced like the design.
    juce::Random rng(7);
    for (float x = -20.0f; x < (float) w + 20.0f; x += 17.0f)
    {
        const float lean = 0.05f * (float) h;
        g.setColour(juce::Colour::fromFloatRGBA(0.24f, 0.11f, 0.03f, 0.18f + 0.10f * rng.nextFloat()));
        g.drawLine(x + 6.0f, 0.0f, x + 6.0f + lean, (float) h, 1.0f);
        g.setColour(juce::Colour::fromFloatRGBA(1.0f, 0.86f, 0.67f, 0.05f + 0.04f * rng.nextFloat()));
        g.drawLine(x + 17.0f, 0.0f, x + 17.0f + lean, (float) h, 2.0f);
    }
    for (float x = 0.0f; x < (float) w; x += 29.0f)
    {
        g.setColour(juce::Colour::fromFloatRGBA(0.27f, 0.12f, 0.04f, 0.12f + 0.08f * rng.nextFloat()));
        g.drawLine(x, 0.0f, x + 0.03f * (float) h, (float) h, 2.0f);
    }
    return img;
}

void drawWood(juce::Graphics& g, juce::Rectangle<int> r)
{
    static const juce::Image wood = renderWood(designWidth, designHeight);
    g.drawImage(wood, r.toFloat(), juce::RectanglePlacement::stretchToFit);
}

void drawScrew(juce::Graphics& g, juce::Point<float> c, float d, float angle)
{
    const auto r = juce::Rectangle<float>(d, d).withCentre(c);
    g.setColour(juce::Colour(0x99000000));
    g.fillEllipse(r.translated(0.0f, 1.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff2f4f6), r.getX() + d * 0.35f, r.getY() + d * 0.30f,
                                           juce::Colour(0xff9aa1aa), r.getX() + d * 0.35f + d * 0.72f, r.getY() + d * 0.30f, true));
    g.fillEllipse(r);
    g.setColour(juce::Colour(0xff40464e));
    const float len = d * 0.42f;
    g.drawLine(c.x - std::cos(angle) * len, c.y - std::sin(angle) * len, c.x + std::cos(angle) * len, c.y + std::sin(angle) * len, 1.4f);
}

void drawTape(juce::Graphics& g, juce::Rectangle<float> a, const juce::String& text, float fontHeight, float tilt,
              juce::Justification just)
{
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(tilt), a.getCentreX(), a.getCentreY()));
    const float w = a.getWidth(), h = a.getHeight(), x = a.getX(), y = a.getY();
    juce::Path p;
    p.startNewSubPath(x + 6, y);
    p.lineTo(x + w - 5, y);
    p.lineTo(x + w, y + h * 0.22f);
    p.lineTo(x + w - 4, y + h * 0.42f);
    p.lineTo(x + w, y + h * 0.63f);
    p.lineTo(x + w - 3, y + h * 0.82f);
    p.lineTo(x + w - 6, y + h);
    p.lineTo(x + 5, y + h);
    p.lineTo(x, y + h * 0.80f);
    p.lineTo(x + 4, y + h * 0.60f);
    p.lineTo(x, y + h * 0.38f);
    p.lineTo(x + 3, y + h * 0.18f);
    p.closeSubPath();

    g.setColour(juce::Colour(0x55000000));
    g.fillPath(p, juce::AffineTransform::translation(0.0f, 1.5f));
    g.setGradientFill(juce::ColourGradient(col::tapeBody.interpolatedWith(juce::Colours::white, 0.35f), x, y,
                                           col::tapeBody.interpolatedWith(juce::Colour(0xff786440), 0.08f), x, y + h, false));
    g.fillPath(p);
    g.setColour(col::tapeInk);
    g.setFont(marker(fontHeight));
    g.drawFittedText(text, a.reduced(6.0f, 1.0f).toNearestInt(), just, 2, 0.85f);
}

void drawKnob(juce::Graphics& g, juce::Point<float> c, const juce::String& colourName, float norm, float ring, float capSize)
{
    const auto kc = knobColour(colourName);
    // tick ring: 11 ticks over 270 degrees
    g.setColour(juce::Colour(0xff8f9bab));
    for (int i = 0; i < 11; ++i)
    {
        const float a = juce::degreesToRadians(-135.0f + 27.0f * (float) i);
        const float r0 = capSize * 0.5f + 1.5f, r1 = ring * 0.5f;
        g.drawLine(c.x + std::sin(a) * r0, c.y - std::cos(a) * r0, c.x + std::sin(a) * r1, c.y - std::cos(a) * r1, 1.3f);
    }
    const auto cap = juce::Rectangle<float>(capSize, capSize).withCentre(c);
    g.setColour(juce::Colour(0x99000000));
    g.fillEllipse(cap.translated(0.0f, 3.0f).expanded(0.5f));
    const auto hot = juce::Point<float>(cap.getX() + capSize * 0.35f, cap.getY() + capSize * 0.28f);
    juce::ColourGradient grad(kc.hi, hot.x, hot.y, kc.cap, hot.x + capSize * 0.62f, hot.y, true);
    grad.addColour(0.62, kc.cap);
    g.setGradientFill(grad);
    g.fillEllipse(cap);
    g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 0.0f, cap.getY() + capSize * 0.55f,
                                           juce::Colour(0x59000000), 0.0f, cap.getBottom(), false));
    g.fillEllipse(cap);

    // pointer
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::rotation(juce::degreesToRadians(-135.0f + 270.0f * juce::jlimit(0.0f, 1.0f, norm)), c.x, c.y));
    g.setColour(kc.pointer);
    g.fillRoundedRectangle(c.x - 1.5f, cap.getY() + 3.0f, 3.0f, capSize * 0.32f, 1.5f);
}

void drawLed(juce::Graphics& g, juce::Point<float> c, float d, bool on, juce::Colour colour)
{
    const auto r = juce::Rectangle<float>(d, d).withCentre(c);
    if (on)
    {
        for (int i = 3; i >= 1; --i)
        {
            g.setColour(colour.withAlpha(0.10f));
            g.fillEllipse(r.expanded((float) i * 2.0f));
        }
        g.setColour(colour);
        g.fillEllipse(r);
        g.setColour(juce::Colour(0x55ffffff));
        g.fillEllipse(r.reduced(d * 0.3f).translated(-d * 0.1f, -d * 0.12f));
    }
    else
    {
        g.setColour(juce::Colour(0xff233026));
        g.fillEllipse(r);
        g.setColour(juce::Colour(0xb3000000));
        g.drawEllipse(r.reduced(0.5f), 1.0f);
    }
}

void drawCapButton(juce::Graphics& g, juce::Rectangle<float> r, bool pressed, bool dim)
{
    const float drop = pressed ? 0.0f : (dim ? 1.0f : 2.0f);
    auto face = r.withTrimmedBottom(2.0f);
    if (pressed)
        face = face.translated(0.0f, 2.0f);
    g.setColour(dim ? juce::Colour(0xff5d5644) : juce::Colour(0xff8f8262));
    g.fillRoundedRectangle(face.translated(0.0f, drop), 3.0f);
    g.setColour(dim ? juce::Colour(0xff8f8770) : juce::Colour(0xffe7dcc0));
    g.fillRoundedRectangle(face, 3.0f);
    if (! dim)
    {
        g.setColour(juce::Colour(0xfffffaf0));
        g.drawLine(face.getX() + 2.0f, face.getY() + 0.8f, face.getRight() - 2.0f, face.getY() + 0.8f, 1.0f);
    }
}

void drawDarkButton(juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down)
{
    g.setColour(down ? col::darkButton.darker(0.3f) : col::darkButton);
    g.fillRoundedRectangle(r, 3.0f);
    g.setColour(over ? juce::Colour(0xff6c7a90) : col::darkBorder);
    g.drawRoundedRectangle(r.reduced(0.5f), 3.0f, 1.0f);
}

void drawBezel(juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour(juce::Colour(0x8c000000));
    g.fillRoundedRectangle(r.translated(0.0f, 4.0f), corner);
    juce::ColourGradient grad(juce::Colour(0xff3a3d42), r.getX(), r.getY(), juce::Colour(0xff08090b), r.getRight(), r.getBottom(), false);
    grad.addColour(0.5, juce::Colour(0xff121418));
    g.setGradientFill(grad);
    g.fillRoundedRectangle(r, corner);
    g.setColour(juce::Colour(0x24ffffff));
    g.drawLine(r.getX() + corner * 0.6f, r.getY() + 1.0f, r.getRight() - corner * 0.6f, r.getY() + 1.0f, 1.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(r.reduced(0.5f), corner, 1.0f);
}

void drawLamp(juce::Graphics& g, juce::Rectangle<float> r, bool over, bool down)
{
    const float corner = 6.0f;
    const float boost = over ? 0.12f : 0.0f;
    if (down)
        r = r.translated(0.0f, 1.0f);
    // glow
    for (int i = 4; i >= 1; --i)
    {
        g.setColour(juce::Colour(0xfff05a32).withAlpha((over ? 0.12f : 0.07f)));
        g.fillRoundedRectangle(r.expanded((float) i * 3.0f), corner + (float) i * 3.0f);
    }
    const auto c = juce::Point<float>(r.getCentreX(), r.getY() + r.getHeight() * 0.46f);
    const float rad = r.getWidth() * 0.72f;
    juce::ColourGradient grad(juce::Colour(0xfffff2d6), c.x, c.y, juce::Colour(0xff78160f), c.x + rad, c.y, true);
    grad.addColour(0.14, juce::Colour(0xffff965c));
    grad.addColour(0.36, juce::Colour(0xffec542e));
    grad.addColour(0.66, juce::Colour(0xffbc2c1e));
    {
        juce::Graphics::ScopedSaveState s(g);
        juce::Path clip; clip.addRoundedRectangle(r, corner);
        g.reduceClipRegion(clip);
        g.setGradientFill(grad);
        g.fillRect(r);
        // faint lens rings
        g.setColour(juce::Colour(0x29ffebd7));
        for (float k = 4.0f; k < rad; k += 5.0f)
            g.drawEllipse(juce::Rectangle<float>(k * 2, k * 2).withCentre(c), 1.0f);
        if (boost > 0.0f) { g.setColour(juce::Colour::fromFloatRGBA(1, 0.8f, 0.6f, boost)); g.fillRect(r); }
        if (down) { g.setColour(juce::Colour(0x24000000)); g.fillRect(r); }
        g.setGradientFill(juce::ColourGradient(juce::Colour(0x8cffffff), 0.0f, r.getY() + r.getHeight() * 0.08f,
                                               juce::Colour(0x00ffffff), 0.0f, r.getY() + r.getHeight() * 0.30f, false));
        g.fillEllipse(juce::Rectangle<float>(r.getX() + r.getWidth() * 0.12f, r.getY() + r.getHeight() * 0.08f,
                                             r.getWidth() * 0.58f, r.getHeight() * 0.22f));
        g.setGradientFill(juce::ColourGradient(juce::Colours::transparentBlack, 0.0f, r.getY() + r.getHeight() * 0.6f,
                                               juce::Colour(0x8c3c0604), 0.0f, r.getBottom(), false));
        g.fillRect(r);
    }
    g.setColour(juce::Colour(0x59ffaa82));
    g.drawRoundedRectangle(r.reduced(0.5f), corner, 1.0f);
}

void drawVu(juce::Graphics& g, juce::Rectangle<float> box, float needle01)
{
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::translation(box.getX(), box.getY()));
    const juce::Rectangle<float> r(0, 0, 150, 62);
    juce::Path clip; clip.addRoundedRectangle(r, 4.0f);

    // warm backlit face
    g.setColour(juce::Colour(0x47ffbe5a));
    g.fillRoundedRectangle(r.expanded(2.0f), 6.0f);
    g.reduceClipRegion(clip);
    juce::ColourGradient face(juce::Colour(0xfffff3c4), 75.0f, 70.0f, juce::Colour(0xff7e5220), 75.0f + 128.0f, 70.0f, true);
    face.addColour(0.28, juce::Colour(0xffffdf8a));
    face.addColour(0.55, juce::Colour(0xfff2bb5a));
    face.addColour(0.80, juce::Colour(0xffc98a35));
    g.setGradientFill(face);
    g.fillRect(r);
    g.setColour(juce::Colour(0x73000000));
    g.fillRect(r.withHeight(6.0f));

    const juce::Colour ink(0xff2b2b2b);
    const juce::Point<float> arcC(75.0f, 82.2f);
    const float arcR = 62.0f, lim = juce::degreesToRadians(58.7f);
    juce::Path arc;  arc.addCentredArc(arcC.x, arcC.y, arcR, arcR, 0.0f, -lim, lim, true);
    g.setColour(ink);
    g.strokePath(arc, juce::PathStrokeType(1.2f));
    juce::Path red;  red.addCentredArc(arcC.x, arcC.y, arcR, arcR, 0.0f, juce::degreesToRadians(29.5f), lim, true);
    g.setColour(juce::Colour(0xff9c2a1c));
    g.strokePath(red, juce::PathStrokeType(3.0f));
    g.setColour(ink);
    const float ticks[5][4] = { { 31, 38, 27, 34 }, { 48, 27, 45, 22 }, { 75, 22, 75, 16 }, { 102, 27, 105, 22 }, { 119, 38, 123, 34 } };
    for (auto& t : ticks)
        g.drawLine(t[0], t[1], t[2], t[3], 1.2f);
    g.setFont(condensed(11.0f));
    g.drawText("VU", juce::Rectangle<float>(55, 34, 40, 14), juce::Justification::centred);
    g.setFont(condensed(8.0f, false));
    g.drawText("-20", juce::Rectangle<float>(18, 22, 20, 10), juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff9c2a1c));
    g.drawText("+3", juce::Rectangle<float>(119, 18, 20, 10), juce::Justification::centredLeft);

    // needle
    const float ang = juce::degreesToRadians(-50.0f + 100.0f * juce::jlimit(0.0f, 1.0f, needle01));
    const juce::Point<float> pivot(75.0f, 68.0f);
    g.setColour(juce::Colour(0xff1d1d1d));
    g.drawLine(pivot.x, pivot.y, pivot.x + std::sin(ang) * 56.0f, pivot.y - std::cos(ang) * 56.0f, 2.0f);

    g.setColour(juce::Colour(0x1affffff));
    g.fillRect(0.0f, 0.0f, 150.0f, 20.0f);
    g.setColour(juce::Colour(0xff1b2129));
    g.drawRoundedRectangle(r.reduced(1.5f), 3.0f, 3.0f);
}

void drawTooltipBox(juce::Graphics& g, juce::Rectangle<float> b)
{
    g.setColour(juce::Colour(0x99000000));
    g.fillRoundedRectangle(b.translated(0.0f, 6.0f).expanded(2.0f), 8.0f);
    g.setColour(col::tipBack);
    g.fillRoundedRectangle(b, 6.0f);
    g.setColour(col::gold.withAlpha(0.6f));
    g.drawRoundedRectangle(b.reduced(0.5f), 6.0f, 1.0f);
}

} // namespace plugsalot::vintage
