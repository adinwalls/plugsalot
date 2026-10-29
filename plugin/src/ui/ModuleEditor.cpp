// Plugsalot - GPL-3.0-or-later
#include "ModuleEditor.h"
#include "core/Library.h"

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
            if (spec->fileKind.isNotEmpty())
                addFileRow(*spec, cs.file);
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

void ModuleEditor::addFileRow(const ModuleSpec& spec, const juce::String& current)
{
    const bool isNam = spec.fileKind == "nam";
    auto* c = controls.add(new Control());
    c->label.setText(isNam ? "Capture" : "Cabinet", juce::dontSendNotification);
    c->label.setColour(juce::Label::textColourId, theme::text);
    c->label.setFont(juce::FontOptions(12.5f));
    c->label.setTooltip(isNam ? "The amp or pedal capture (.nam) this module plays through."
                              : "The cabinet impulse response (.wav) this module convolves with.");

    // A combo box plus an "Add file" button, laid out inside one component.
    struct Row final : public juce::Component
    {
        juce::ComboBox box;
        juce::TextButton add { "Add file" };
        juce::TextButton browse { "TONE3000" };
        void resized() override
        {
            auto r = getLocalBounds();
            if (browse.isVisible()) { browse.setBounds(r.removeFromRight(78)); r.removeFromRight(4); }
            add.setBounds(r.removeFromRight(66));
            r.removeFromRight(4);
            box.setBounds(r);
        }
    };
    auto row = std::make_unique<Row>();
    const int index = slot;
    const auto kind = spec.fileKind;
    auto* raw = row.get();
    auto fill = [raw, kind](const juce::String& selected)
    {
        raw->box.clear(juce::dontSendNotification);
        raw->box.addItem("None (pass audio through)", 1);
        auto names = Library::list(kind);
        for (int i = 0; i < names.size(); ++i)
            raw->box.addItem(names[i], i + 2);
        const int at = names.indexOf(selected);
        raw->box.setSelectedId(at >= 0 ? at + 2 : 1, juce::dontSendNotification);
    };
    fill(current);
    row->box.onChange = [this, raw, index, kind]
    {
        const int id = raw->box.getSelectedId();
        proc.setSlotFile(index, id <= 1 ? juce::String() : Library::list(kind)[id - 2]);
    };
    row->add.onClick = [this, index, kind]
    {
        const auto pattern = kind == "nam" ? "*.nam" : "*.wav";
        chooser = std::make_unique<juce::FileChooser>(kind == "nam" ? "Choose a NAM capture" : "Choose a cabinet impulse response",
                                                      juce::File::getSpecialLocation(juce::File::userHomeDirectory), pattern);
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                             [safe = juce::Component::SafePointer<ModuleEditor>(this), index, kind](const juce::FileChooser& fc)
                             {
                                 if (safe == nullptr)
                                     return;
                                 const auto name = Library::import(kind, fc.getResult());
                                 if (name.isNotEmpty())
                                     safe->proc.setSlotFile(index, name); // the panel rebuilds and lists it
                             });
    };
    row->browse.setVisible(isNam && onBrowseTone3000 != nullptr);
    row->browse.onClick = [this, index]
    {
        if (onBrowseTone3000)
            onBrowseTone3000(index, [safe = juce::Component::SafePointer<ModuleEditor>(this), index](const juce::String& name)
                             {
                                 if (safe != nullptr && name.isNotEmpty())
                                     safe->proc.setSlotFile(index, name);
                             });
    };
    row->addAndMakeVisible(row->box);
    row->addAndMakeVisible(row->add);
    row->addAndMakeVisible(row->browse);
    c->widget = std::move(row);
    c->addAndMakeVisible(c->label);
    c->addAndMakeVisible(*c->widget);
    content.addAndMakeVisible(c);

    // Credit line for captures that came from TONE3000: tone, creator, licence, link back.
    const auto info = Library::attribution(spec.fileKind, current);
    if (info.isObject())
    {
        auto* credit = controls.add(new Control());
        credit->label.setText("Credit", juce::dontSendNotification);
        credit->label.setColour(juce::Label::textColourId, theme::text);
        credit->label.setFont(juce::FontOptions(12.5f));
        const auto text = info.getProperty("tone_title", {}).toString() + " by " + info.getProperty("creator", {}).toString()
                          + " (" + Tone3000::licenceLabel(info.getProperty("license", {}).toString()) + ") on TONE3000";
        auto link = std::make_unique<juce::HyperlinkButton>(text, juce::URL(info.getProperty("url", "https://www.tone3000.com").toString()));
        link->setFont(juce::FontOptions(12.0f), false);
        link->setColour(juce::HyperlinkButton::textColourId, theme::accent);
        link->setJustificationType(juce::Justification::centredLeft);
        credit->widget = std::move(link);
        credit->addAndMakeVisible(credit->label);
        credit->addAndMakeVisible(*credit->widget);
        content.addAndMakeVisible(credit);
    }
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
