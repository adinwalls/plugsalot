// Plugsalot - GPL-3.0-or-later
#include "ModuleEditor.h"

namespace plugsalot
{

static constexpr int controlHeight = 30;

ModuleEditor::ModuleEditor(PlugsalotProcessor& p) : proc(p)
{
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);
    addAndMakeVisible(viewport);
}

void ModuleEditor::showSlot(int s)
{
    slot = s;
    controls.clear();
    title.clear();
    const auto& chain = proc.getChain();
    if (slot >= 0 && slot < (int) chain.slots.size())
    {
        const auto& cs = chain.slots[(size_t) slot];
        if (auto* spec = Catalogue::embedded().find(cs.moduleId))
        {
            title = spec->name;
            for (auto& p : spec->params)
            {
                auto* c = controls.add(new Control());
                const auto value = cs.params.contains(p.id) ? cs.params[p.id] : p.defaultValue;
                c->label.setText(p.name, juce::dontSendNotification);
                c->label.setColour(juce::Label::textColourId, theme::text);
                c->label.setFont(juce::FontOptions(12.5f));
                c->label.setTooltip(p.aiHint);
                const int index = slot;
                const auto paramId = p.id;

                switch (p.type)
                {
                    case ParamSpec::Type::Enum:
                    {
                        auto box = std::make_unique<juce::ComboBox>();
                        box->addItemList(p.options, 1);
                        box->setSelectedItemIndex(juce::jmax(0, p.options.indexOf(value.toString())), juce::dontSendNotification);
                        auto* raw = box.get();
                        const auto options = p.options;
                        box->onChange = [this, raw, index, paramId, options]
                        { proc.setManualParam(index, paramId, options[raw->getSelectedItemIndex()]); };
                        c->widget = std::move(box);
                        break;
                    }
                    case ParamSpec::Type::Bool:
                    {
                        auto t = std::make_unique<juce::ToggleButton>();
                        t->setToggleState((bool) value, juce::dontSendNotification);
                        auto* raw = t.get();
                        t->onClick = [this, raw, index, paramId] { proc.setManualParam(index, paramId, raw->getToggleState()); };
                        c->widget = std::move(t);
                        break;
                    }
                    case ParamSpec::Type::Int:
                    case ParamSpec::Type::Float:
                    default:
                    {
                        auto sl = std::make_unique<juce::Slider>(juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
                        juce::NormalisableRange<double> range(p.min, p.max);
                        if (p.type == ParamSpec::Type::Int)
                            range.interval = 1.0;
                        if (p.unit == "Hz" && p.min > 0.0)
                            range.setSkewForCentre(std::sqrt(p.min * p.max));
                        sl->setNormalisableRange(range);
                        sl->setTextValueSuffix(p.unit.isNotEmpty() ? " " + p.unit : juce::String());
                        sl->setNumDecimalPlacesToDisplay(p.max - p.min > 100 ? 0 : 2);
                        sl->setTextBoxStyle(juce::Slider::TextBoxRight, false, 84, 20);
                        sl->setValue((double) value, juce::dontSendNotification);
                        sl->setTooltip(p.aiHint);
                        auto* raw = sl.get();
                        const bool isInt = p.type == ParamSpec::Type::Int;
                        sl->onValueChange = [this, raw, index, paramId, isInt]
                        {
                            proc.setManualParam(index, paramId,
                                                isInt ? juce::var((int) std::round(raw->getValue())) : juce::var(raw->getValue()));
                        };
                        c->widget = std::move(sl);
                        break;
                    }
                }
                c->addAndMakeVisible(c->label);
                c->addAndMakeVisible(*c->widget);
                content.addAndMakeVisible(c);
            }
        }
    }
    resized();
    repaint();
}

void ModuleEditor::paint(juce::Graphics& g)
{
    g.fillAll(theme::panel);
    g.setColour(theme::outline);
    g.drawHorizontalLine(0, 0.0f, (float) getWidth());
    g.setColour(theme::text);
    g.setFont(juce::FontOptions(13.0f, juce::Font::bold));
    g.drawText(title.isNotEmpty() ? title.toUpperCase() + " SETTINGS" : "SETTINGS", 14, 6, getWidth() - 28, 18,
               juce::Justification::centredLeft);
    if (controls.isEmpty())
    {
        g.setColour(theme::muted);
        g.setFont(juce::FontOptions(12.5f));
        g.drawText("Select a module to see and tweak what the assistant set.", getLocalBounds().withTrimmedTop(28).reduced(14, 4),
                   juce::Justification::topLeft);
    }
}

void ModuleEditor::resized()
{
    auto r = getLocalBounds().reduced(10, 4);
    r.removeFromTop(24);
    viewport.setBounds(r);
    const int w = viewport.getWidth() - viewport.getScrollBarThickness() - 2;
    content.setSize(w, controls.size() * controlHeight);
    int y = 0;
    for (auto* c : controls)
    {
        c->setBounds(0, y, w, controlHeight);
        y += controlHeight;
    }
}

} // namespace plugsalot
