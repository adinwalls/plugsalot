// Plugsalot - GPL-3.0-or-later
#include "RackPanel.h"

namespace plugsalot
{

namespace
{
const float tapeTilt[] = { -1.4f, 1.0f, -0.8f, 1.3f, -1.1f, 0.7f };

// The hover text for a module: what it does, what it suits, and where it comes from.
juce::String describe(const ModuleSpec& m)
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

float easeOutCubic(float t) { t = juce::jlimit(0.0f, 1.0f, t); return 1.0f - std::pow(1.0f - t, 3.0f); }
float easeOutBack(float t)
{
    t = juce::jlimit(0.0f, 1.0f, t);
    const float s = 1.1f, u = t - 1.0f;
    return 1.0f + (s + 1.0f) * u * u * u + s * u * u;
}
} // namespace

// ---------------------------------------------------------------------------
// Knob
// ---------------------------------------------------------------------------

class RackPanel::Knob final : public juce::Component, public juce::SettableTooltipClient, private juce::Timer
{
public:
    static constexpr int ringSize = 52;

    Knob(PlugsalotProcessor& p, int slotIndex, ParamSpec spec, KnobSpec ks)
        : proc(p), slot(slotIndex), param(std::move(spec)), knob(std::move(ks))
    {
        range = juce::NormalisableRange<double>(param.min, param.max);
        if (param.type == ParamSpec::Type::Int)
            range.interval = 1.0;
        if (param.unit == "Hz" && param.min > 0.0)
            range.setSkewForCentre(std::sqrt(param.min * param.max));
        setMouseCursor(juce::MouseCursor::UpDownResizeCursor);
        setRepaintsOnMouseActivity(true);
    }

    const juce::String& paramId() const { return param.id; }

    double toNorm(const juce::var& v) const
    {
        switch (param.type)
        {
            case ParamSpec::Type::Enum:
            {
                const int n = juce::jmax(2, param.options.size());
                return (double) juce::jmax(0, param.options.indexOf(v.toString())) / (double) (n - 1);
            }
            case ParamSpec::Type::Bool: return (bool) v ? 1.0 : 0.0;
            default: return range.convertTo0to1(juce::jlimit(param.min, param.max, (double) v));
        }
    }

    juce::var fromNorm(double n) const
    {
        n = juce::jlimit(0.0, 1.0, n);
        switch (param.type)
        {
            case ParamSpec::Type::Enum:
            {
                const int count = param.options.size();
                return param.options[juce::jlimit(0, juce::jmax(0, count - 1), (int) std::round(n * juce::jmax(1, count - 1)))];
            }
            case ParamSpec::Type::Bool: return n > 0.5;
            case ParamSpec::Type::Int:  return (int) std::round(range.convertFrom0to1(n));
            default:                    return range.convertFrom0to1(n);
        }
    }

    double snap(double n) const { return toNorm(fromNorm(n)); }

    // Show `v`; when `animate` and the position changes, sweep there.
    void setValue(const juce::var& v, bool animate)
    {
        current = v;
        target = toNorm(v);
        if (dragging)
            return;
        if (std::abs(target - shown) < 1e-4)
            return;
        if (animate) sweep(shown, target, 700);
        else { stopTimer(); shown = target; repaint(); }
    }

    void showZero() { stopTimer(); shown = 0.0; repaint(); }
    void sweepFromZero(int ms) { sweep(0.0, target, ms); }

    void paint(juce::Graphics& g) override
    {
        vintage::drawKnob(g, { getWidth() * 0.5f, (float) ringSize * 0.5f }, knob.colour, (float) shown);
        g.setColour(juce::Colour(0xffccd4de));
        g.setFont(vintage::condensed(11.0f, false).withExtraKerningFactor(0.1f));
        const bool showValue = dragging || isMouseOver();
        g.setColour(showValue ? vintage::col::gold : juce::Colour(0xffccd4de));
        g.drawFittedText(showValue ? param.format(current) : knob.label.toUpperCase(),
                         juce::Rectangle<int>(0, ringSize + 2, getWidth(), 11), juce::Justification::centred, 1, 0.7f);
    }

    void mouseDown(const juce::MouseEvent&) override
    {
        stopTimer();
        dragging = true;
        dragStart = shown;
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        const double per = e.mods.isShiftDown() ? 700.0 : 200.0;
        commit(dragStart - (double) e.getDistanceFromDragStartY() / per);
    }
    void mouseUp(const juce::MouseEvent&) override { dragging = false; repaint(); }
    void mouseDoubleClick(const juce::MouseEvent&) override
    {
        commit(toNorm(param.defaultValue));
    }
    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        const double step = param.type == ParamSpec::Type::Enum ? 1.0 / juce::jmax(1, param.options.size() - 1) : 0.02;
        commit(shown + (w.deltaY > 0 ? step : -step));
    }

private:
    void commit(double n)
    {
        n = juce::jlimit(0.0, 1.0, n);
        const bool discrete = param.type == ParamSpec::Type::Enum || param.type == ParamSpec::Type::Bool || param.type == ParamSpec::Type::Int;
        const auto v = fromNorm(n);
        shown = discrete ? toNorm(v) : n;
        target = shown;
        current = v;
        proc.setManualParam(slot, param.id, v);
        repaint();
    }

    void sweep(double from, double to, int ms)
    {
        from0 = from; to0 = to; durationMs = ms; t0 = juce::Time::getMillisecondCounterHiRes();
        startTimerHz(60);
    }
    void timerCallback() override
    {
        const double t = (juce::Time::getMillisecondCounterHiRes() - t0) / durationMs;
        shown = from0 + (to0 - from0) * (double) easeOutCubic((float) t);
        if (t >= 1.0) { shown = to0; stopTimer(); }
        repaint();
    }

    PlugsalotProcessor& proc;
    int slot;
    ParamSpec param;
    KnobSpec knob;
    juce::NormalisableRange<double> range;
    juce::var current;
    double shown = 0.0, target = 0.0, dragStart = 0.0, from0 = 0.0, to0 = 0.0, durationMs = 700.0, t0 = 0.0;
    bool dragging = false;
};

// ---------------------------------------------------------------------------
// Card
// ---------------------------------------------------------------------------

class RackPanel::Card final : public juce::Component, public juce::SettableTooltipClient
{
public:
    Card(RackPanel& o, PlugsalotProcessor& p, int slotIndex) : owner(o), proc(p), slot(slotIndex)
    {
        const auto& cs = proc.getChain().slots[(size_t) slot];
        spec = Catalogue::embedded().find(cs.moduleId);
        if (spec != nullptr)
            tip = describe(*spec);
        setTooltip(tip);

        plate.card = this;
        plate.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        plate.setTooltip(tip + "\n\nClick the name for every setting" + (spec && spec->fileKind.isNotEmpty() ? " and to pick a file." : "."));
        addAndMakeVisible(plate);
        inButton.card = this;
        inButton.setMouseCursor(juce::MouseCursor::PointingHandCursor);
        inButton.setTooltip(tip);
        addAndMakeVisible(inButton);
        bypassed = cs.bypass;
        rebuildKnobs(false);
    }

    int slotIndex() const { return slot; }
    void freeze()
    {   // a card that is leaving no longer talks to the processor
        setInterceptsMouseClicks(false, false);
        frozen = true;
    }

    // Bring the knobs and the IN button in line with the chain; `animate` sweeps changed knobs.
    void sync(bool animate)
    {
        const auto& chain = proc.getChain();
        if (slot >= (int) chain.slots.size() || spec == nullptr)
            return;
        const auto& cs = chain.slots[(size_t) slot];
        if (bypassed != cs.bypass)
        {
            bypassed = cs.bypass;
            repaint();
        }
        const auto& wanted = spec->knobsFor(cs.params);
        bool same = wanted.size() == (size_t) knobs.size();
        for (size_t i = 0; same && i < wanted.size(); ++i)
            same = knobs[(int) i]->paramId() == wanted[i].paramId;
        if (! same)
            rebuildKnobs(animate);
        else
            for (auto* k : knobs)
                if (auto* ps = spec->findParam(k->paramId()))
                    k->setValue(cs.params.contains(ps->id) ? cs.params[ps->id] : ps->defaultValue, animate);
    }

    void showKnobsAtZero() { for (auto* k : knobs) k->showZero(); }
    void sweepKnobsIn() { for (auto* k : knobs) k->sweepFromZero(900); }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour(juce::Colour(0x99000000));
        g.fillRoundedRectangle(r.translated(0.0f, 3.0f).reduced(1.0f), 4.0f);
        g.setGradientFill(juce::ColourGradient(vintage::col::enamel.interpolatedWith(juce::Colours::white, 0.07f), 0.0f, 0.0f,
                                               vintage::col::enamel.interpolatedWith(juce::Colours::black, 0.10f), 0.0f, r.getHeight(), false));
        g.fillRoundedRectangle(r, 4.0f);
        g.setColour(juce::Colour(0x1fffffff));
        g.drawRoundedRectangle(r.reduced(0.5f), 4.0f, 1.0f);

        vintage::drawScrew(g, { 14.5f, 14.0f }, 11.0f, 0.7f);
        vintage::drawScrew(g, { r.getWidth() - 14.5f, 14.0f }, 11.0f, 2.3f);
        if (spec == nullptr)
            return;

        vintage::drawTape(g, { 6.0f, 28.0f, r.getWidth() - 12.0f, 34.0f }, spec->tape.isNotEmpty() ? spec->tape.toUpperCase() : spec->name.toUpperCase(),
                          11.5f, tapeTilt[slot % 6]);
        g.setColour(juce::Colour(0xfff1f4f7));
        g.setFont(vintage::condensed(16.0f).withExtraKerningFactor(0.08f));
        g.drawFittedText(spec->name.toUpperCase(), 4, 68, getWidth() - 8, 19, juce::Justification::centred, 1, 0.7f);
        g.setColour(vintage::col::textLow);
        g.setFont(vintage::body(10.5f));
        g.drawFittedText(spec->maker.isNotEmpty() ? spec->maker : spec->upstreamName, 4, 88, getWidth() - 8, 13, juce::Justification::centred, 1, 0.7f);

        vintage::drawLed(g, { inButton.getX() - 10.0f - 5.5f, inButton.getBounds().getCentreY() + 0.0f }, 11.0f, ! bypassed);
        g.setColour(vintage::col::textLow);
        g.setFont(vintage::body(9.5f));
        g.drawText(spec->licence, 0, inButton.getBottom() + 4, getWidth(), 12, juce::Justification::centred);
    }

    void resized() override
    {
        plate.setBounds(2, 64, getWidth() - 4, 42);
        inButton.setBounds((getWidth() - 46) / 2, 494, 46, 28);
        layoutKnobs();
    }

private:
    struct Plate final : public juce::Component, public juce::SettableTooltipClient
    {
        Card* card = nullptr;
        void paint(juce::Graphics& g) override
        {
            if (isMouseOver())
            {
                g.setColour(juce::Colour(0x14ffffff));
                g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.0f);
            }
        }
        void mouseEnter(const juce::MouseEvent&) override { repaint(); }
        void mouseExit(const juce::MouseEvent&) override { repaint(); }
        void mouseUp(const juce::MouseEvent& e) override
        {
            if (e.mouseWasClicked() && card != nullptr && card->owner.onOpenSettings)
                card->owner.onOpenSettings(card->slot);
        }
    };

    struct InButton final : public juce::Component, public juce::SettableTooltipClient
    {
        Card* card = nullptr;
        bool down = false;
        void paint(juce::Graphics& g) override
        {
            vintage::drawCapButton(g, getLocalBounds().toFloat(), down, card->bypassed);
            g.setColour(card->bypassed ? juce::Colour(0xff2a2a2a) : juce::Colour(0xff25272b));
            g.setFont(vintage::condensed(14.0f, false).withExtraKerningFactor(0.1f));
            g.drawText("IN", getLocalBounds().withTrimmedBottom(2).translated(0, down ? 2 : 0), juce::Justification::centred);
        }
        void mouseDown(const juce::MouseEvent&) override { down = true; repaint(); }
        void mouseUp(const juce::MouseEvent& e) override
        {
            down = false;
            repaint();
            if (e.mouseWasClicked() && contains(e.getPosition()) && ! card->frozen)
                card->proc.setManualBypass(card->slot, ! card->bypassed);
        }
    };

    void rebuildKnobs(bool animate)
    {
        knobs.clear();
        if (spec == nullptr)
            return;
        const auto& cs = proc.getChain().slots[(size_t) slot];
        for (const auto& ks : spec->knobsFor(cs.params))
        {
            const auto* ps = spec->findParam(ks.paramId);
            if (ps == nullptr)
                continue;
            auto* k = knobs.add(new Knob(proc, slot, *ps, ks));
            k->setTooltip(tip);
            k->setValue(cs.params.contains(ps->id) ? cs.params[ps->id] : ps->defaultValue, false);
            addAndMakeVisible(k);
            if (animate)
            {
                k->showZero();
                k->sweepFromZero(900);
            }
        }
        layoutKnobs();
    }

    void layoutKnobs()
    {
        if (knobs.isEmpty())
            return;
        const int top = 108, bottom = 490;
        const int pitch = juce::jmin(75, (bottom - top) / knobs.size());
        int y = top;
        for (auto* k : knobs)
        {
            k->setBounds((getWidth() - 76) / 2, y, 76, pitch);
            y += pitch;
        }
    }

    RackPanel& owner;
    PlugsalotProcessor& proc;
    int slot;
    const ModuleSpec* spec = nullptr;
    juce::String tip;
    bool bypassed = false, frozen = false;
    Plate plate;
    InButton inButton;
    juce::OwnedArray<Knob> knobs;
};

// ---------------------------------------------------------------------------
// Rack
// ---------------------------------------------------------------------------

RackPanel::RackPanel(PlugsalotProcessor& p) : proc(p)
{
    sig = currentSignature();
    buildCards(true);
}

RackPanel::~RackPanel() { stopTimer(); }

RackPanel::Signature RackPanel::currentSignature() const
{
    Signature s;
    for (auto& slot : proc.getChain().slots)
        s.add(slot.instanceId.toString() + "|" + slot.moduleId + "|" + slot.file);
    return s;
}

void RackPanel::buildCards(bool seated)
{
    cards.clear();
    const int n = juce::jmin(maxSlots, (int) proc.getChain().slots.size());
    for (int i = 0; i < n; ++i)
    {
        auto* c = cards.add(new Card(*this, proc, i));
        addAndMakeVisible(c);
        c->setBounds(slotBounds(i));
        if (! seated)
        {
            c->showKnobsAtZero();
            c->setAlpha(0.0f);
            c->setVisible(false);
        }
    }
}

void RackPanel::setStatus(const juce::String& s)
{
    if (s == statusText)
        return;
    statusText = s;
    if (onStatusChanged)
        onStatusChanged();
}

void RackPanel::refresh()
{
    const auto now = currentSignature();
    if (now == sig)
    {
        if (! animating)
            for (auto* c : cards)
                c->sync(true);
        else
            for (auto* c : cards)
                c->sync(false);
        return;
    }
    const bool hadCards = ! cards.isEmpty();
    sig = now;
    // A change arriving mid-animation: finish the old one instantly.
    leaving.clear();
    for (auto* c : cards)
        c->freeze();
    while (! cards.isEmpty())
        leaving.add(cards.removeAndReturn(0));
    buildCards(false);
    startTransition(hadCards);
}

void RackPanel::startTransition(bool hadCards)
{
    const int nOld = leaving.size(), nNew = cards.size();
    leaveAt.assign((size_t) nOld, 0.0);
    enterAt.assign((size_t) nNew, 0.0);
    for (int j = 0; j < nOld; ++j)
        leaveAt[(size_t) j] = (double) (nOld - 1 - j) * 280.0;   // right to left
    const double leaveEnd = hadCards ? (double) (nOld - 1) * 280.0 + 690.0 + 120.0 : 0.0;
    for (int i = 0; i < nNew; ++i)
        enterAt[(size_t) i] = leaveEnd + (double) i * 560.0;
    transitionEnd = (nNew > 0 ? enterAt.back() + 560.0 + 900.0 : leaveEnd);
    transitionStart = juce::Time::getMillisecondCounterHiRes();
    animating = true;
    for (auto* c : cards)
        c->setInterceptsMouseClicks(false, false);
    startTimerHz(60);
    timerCallback();
}

void RackPanel::timerCallback()
{
    const double t = juce::Time::getMillisecondCounterHiRes() - transitionStart;
    juce::String status;

    for (int j = 0; j < leaving.size(); ++j)
    {
        auto* c = leaving[j];
        const double lt = t - leaveAt[(size_t) j];
        float dy = 0.0f, rot = 0.0f, alpha = 1.0f;
        if (lt > 0.0 && lt < 700.0)
            status = "Unplugging...";
        if (lt > 0.0)
        {
            if (lt < 170.0)
            {
                const float e = easeOutCubic((float) (lt / 170.0));
                dy = -14.0f * e; rot = -1.0f * e;
            }
            else
            {
                const float u = juce::jlimit(0.0f, 1.0f, (float) ((lt - 170.0) / 520.0));
                const float e = u * u * u;
                dy = -14.0f - 626.0f * e; rot = -1.0f - 3.0f * e;
                alpha = 1.0f - juce::jlimit(0.0f, 1.0f, (float) ((lt - 270.0) / 420.0));
            }
        }
        const auto b = c->getBounds().toFloat();
        c->setTransform(juce::AffineTransform::rotation(juce::degreesToRadians(rot), b.getCentreX(), b.getCentreY()).translated(0.0f, dy));
        c->setAlpha(alpha);
    }

    for (int i = 0; i < cards.size(); ++i)
    {
        auto* c = cards[i];
        const double et = t - enterAt[(size_t) i];
        if (et < 0.0)
            continue;
        if (! c->isVisible())
        {
            c->setVisible(true);
            c->toFront(false);
        }
        const float u = (float) (et / 560.0);
        const float f = easeOutBack(u);
        c->setTransform(juce::AffineTransform::translation(0.0f, -640.0f * (1.0f - f)));
        c->setAlpha(juce::jlimit(0.0f, 1.0f, u / 0.3f));
        if (et < 560.0)
        {
            if (auto* spec = Catalogue::embedded().find(proc.getChain().slots[(size_t) i].moduleId))
                status = "Plugging in " + spec->name + "...";
        }
        else if (! (c->getProperties().contains("swept")))
        {
            c->getProperties().set("swept", true);
            c->setTransform({});
            c->setAlpha(1.0f);
            c->sweepKnobsIn();
            c->setInterceptsMouseClicks(true, true);
        }
    }

    setStatus(status);

    if (t >= transitionEnd)
    {
        stopTimer();
        leaving.clear();
        for (auto* c : cards)
        {
            c->setTransform({});
            c->setAlpha(1.0f);
            c->setVisible(true);
            c->setInterceptsMouseClicks(true, true);
        }
        animating = false;
        setStatus({});
        for (auto* c : cards)
            c->sync(false);
    }
}

void RackPanel::paint(juce::Graphics& g)
{
    const auto r = getLocalBounds().toFloat();
    g.setColour(vintage::col::rackBack);
    g.fillRoundedRectangle(r, 4.0f);
    g.setColour(juce::Colour(0x12ffffff));
    g.drawLine(2.0f, r.getBottom() + 0.5f, r.getRight() - 2.0f, r.getBottom() + 0.5f);

    for (int i = 0; i < maxSlots; ++i)
    {
        const auto b = slotBounds(i).toFloat();
        g.setGradientFill(juce::ColourGradient(vintage::col::slotTop, 0.0f, b.getY(), vintage::col::slotBottom, 0.0f, b.getBottom(), false));
        g.fillRoundedRectangle(b, 3.0f);
        // inset shadow
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xd9000000), 0.0f, b.getY(), juce::Colours::transparentBlack, 0.0f, b.getY() + 12.0f, false));
        g.fillRect(b.withHeight(12.0f));
        g.setColour(juce::Colour(0xff232a33));
        g.drawRoundedRectangle(b.reduced(0.5f), 3.0f, 1.0f);
        // pins at the foot of the slot
        const auto pins = juce::Rectangle<float>(b.getX() + 27.0f, b.getBottom() - 26.0f, b.getWidth() - 54.0f, 14.0f);
        g.setColour(juce::Colour(0xff1d2229));
        g.fillRect(pins.expanded(2.0f));
        for (float x = pins.getX(); x < pins.getRight(); x += 7.0f)
        {
            g.setColour(juce::Colour(0xffece3c6));
            g.fillRect(x, pins.getY(), juce::jmin(4.0f, pins.getRight() - x), pins.getHeight());
            g.setColour(juce::Colour(0xff7d7458));
            if (x + 4.0f < pins.getRight())
                g.fillRect(x + 4.0f, pins.getY(), juce::jmin(3.0f, pins.getRight() - x - 4.0f), pins.getHeight());
        }
        if (i >= (int) proc.getChain().slots.size() && ! animating)
        {
            g.setColour(juce::Colour(0xff59636f));
            g.setFont(vintage::condensed(12.0f, false).withExtraKerningFactor(0.2f));
            g.drawText("EMPTY SLOT", b.withTrimmedTop(b.getHeight() * 0.46f - 8.0f).withHeight(16.0f), juce::Justification::centred);
        }
    }
}

} // namespace plugsalot
