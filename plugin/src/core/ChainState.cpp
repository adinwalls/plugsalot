// Plugsalot - GPL-3.0-or-later
#include "ChainState.h"

namespace plugsalot
{

juce::var ChainState::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty("schema_version", 1);
    o->setProperty("source", source);
    o->setProperty("stage", stage);
    juce::Array<juce::var> chain;
    for (auto& s : slots)
    {
        auto* so = new juce::DynamicObject();
        so->setProperty("module", s.moduleId);
        auto* po = new juce::DynamicObject();
        for (auto& nv : s.params)
            po->setProperty(nv.name, nv.value);
        so->setProperty("params", juce::var(po));
        if (s.bypass)
            so->setProperty("bypass", true);
        chain.add(juce::var(so));
    }
    o->setProperty("chain", chain);
    o->setProperty("explanation", explanation);
    return juce::var(o);
}

ChainState ChainState::fromVar(const juce::var& v)
{
    ChainState c;
    c.source = v.getProperty("source", {}).toString();
    c.stage = v.getProperty("stage", "mixing").toString();
    c.explanation = v.getProperty("explanation", {}).toString();
    if (auto* arr = v.getProperty("chain", {}).getArray())
    {
        for (auto& s : *arr)
        {
            ChainSlot slot;
            slot.moduleId = s.getProperty("module", {}).toString();
            slot.bypass = (bool) s.getProperty("bypass", false);
            if (auto* po = s.getProperty("params", {}).getDynamicObject())
                slot.params = po->getProperties();
            c.slots.push_back(std::move(slot));
        }
    }
    return c;
}

juce::var SessionContext::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty("source", source);
    o->setProperty("stage", stage);
    o->setProperty("style", style);
    o->setProperty("character", character);
    o->setProperty("cpu_mode", cpuMode);
    return juce::var(o);
}

SessionContext SessionContext::fromVar(const juce::var& v)
{
    SessionContext c;
    if (! v.isObject())
        return c;
    c.source = v.getProperty("source", c.source).toString();
    c.stage = v.getProperty("stage", c.stage).toString();
    c.style = v.getProperty("style", c.style).toString();
    c.character = v.getProperty("character", c.character).toString();
    c.cpuMode = v.getProperty("cpu_mode", c.cpuMode).toString();
    return c;
}

} // namespace plugsalot
