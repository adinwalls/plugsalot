// Plugsalot - GPL-3.0-or-later
#include "ChainValidator.h"

namespace plugsalot
{

int ChainValidator::cpuBudget(const juce::String& mode)
{
    if (mode == "light") return 6;
    if (mode == "high") return 20;
    return 12;
}

static bool isNumber(const juce::var& v) { return v.isInt() || v.isInt64() || v.isDouble(); }

ValidationResult ChainValidator::validateChain(const juce::var& reply, const juce::String& cpuMode) const
{
    ValidationResult r;
    if (! reply.isObject())
    {
        r.errors.add("reply: expected a JSON object.");
        return r;
    }
    if ((int) reply.getProperty("schema_version", 0) != 1)
        r.errors.add("schema_version must be 1.");
    const auto stage = reply.getProperty("stage", {}).toString();
    if (! juce::StringArray { "tracking", "mixing", "mastering" }.contains(stage))
        r.errors.add("stage must be one of tracking, mixing, mastering.");
    auto* chain = reply.getProperty("chain", {}).getArray();
    if (chain == nullptr || chain->isEmpty())
        r.errors.add("chain must be a non-empty list of modules.");
    else if (chain->size() > maxModules)
        r.errors.add("chain is too long: " + juce::String(chain->size()) + " modules; the limit is " + juce::String(maxModules) + ".");
    if (! reply.getProperty("explanation", {}).isString())
        r.errors.add("explanation must be a string.");
    if (chain != nullptr)
        for (int i = 0; i < chain->size(); ++i)
        {
            auto& s = chain->getReference(i);
            if (! s.isObject() || ! s.getProperty("module", {}).isString() || ! s.getProperty("params", {}).isObject())
                r.errors.add("chain[" + juce::String(i) + "]: each entry needs 'module' (string) and 'params' (object).");
        }
    if (! r.ok())
        return r;
    return validate(ChainState::fromVar(reply), cpuMode);
}

ValidationResult ChainValidator::validate(ChainState chain, const juce::String& cpuMode) const
{
    ValidationResult r;
    int cpu = 0;
    juce::Array<int> limiterPositions;

    if (chain.slots.empty())
        r.errors.add("chain must contain at least one module.");
    if ((int) chain.slots.size() > maxModules)
        r.errors.add("chain is too long: " + juce::String((int) chain.slots.size()) + " modules; the limit is " + juce::String(maxModules) + ".");

    for (int i = 0; i < (int) chain.slots.size(); ++i)
    {
        auto& slot = chain.slots[(size_t) i];
        const auto where = "chain[" + juce::String(i) + "]: ";
        auto* mod = catalogue.find(slot.moduleId);
        if (mod == nullptr)
        {
            r.errors.add(where + "unknown module '" + slot.moduleId + "'. Available: " + catalogue.ids().joinIntoString(", ") + ".");
            continue;
        }
        if (mod->category == "limiting")
            limiterPositions.add(i);
        cpu += mod->cpuCost();

        for (auto& nv : slot.params)
        {
            auto* spec = mod->findParam(nv.name.toString());
            if (spec == nullptr)
            {
                juce::StringArray valid;
                for (auto& p : mod->params)
                    valid.add(p.id);
                r.errors.add(where + "'" + mod->id + "' has no parameter '" + nv.name.toString() + "'. Valid parameters: "
                             + valid.joinIntoString(", ") + ".");
                continue;
            }
            // Accept whole-number floats for int params and numeric strings the model sometimes sends.
            if (spec->type != ParamSpec::Type::Enum && nv.value.isString() && nv.value.toString().containsOnly("-+.0123456789eE"))
                slot.params.set(nv.name, nv.value.toString().getDoubleValue());
            auto err = spec->check(slot.params[nv.name]);
            if (err.isNotEmpty())
                r.errors.add(where + err);
        }

        if (chain.stage == "tracking")
        {
            if (! mod->trackingSafe)
                r.errors.add(where + "'" + mod->id + "' cannot be used while tracking (too much latency, or not suited to recording).");
            for (auto& o : mod->trackingOverrides)
                slot.params.set(o.name, o.value);
        }
        if (slot.instanceId.isNull())
            slot.instanceId = juce::Uuid();
    }

    if (limiterPositions.size() > 1)
        r.errors.add("Only one limiter is allowed per chain.");
    if (! limiterPositions.isEmpty() && limiterPositions.getLast() != (int) chain.slots.size() - 1)
        r.errors.add("The limiter must be the last module in the chain.");

    const int budget = cpuBudget(cpuMode);
    if (cpu > budget)
        r.errors.add("Chain uses " + juce::String(cpu) + " CPU points; the '" + cpuMode + "' CPU mode allows "
                     + juce::String(budget) + ". Remove or swap a module.");

    if (r.ok())
        r.chain = std::move(chain);
    return r;
}

ValidationResult ChainValidator::applyChanges(const juce::var& reply, const ChainState& current, const juce::String& cpuMode) const
{
    ValidationResult r;
    auto* changes = reply.getProperty("changes", {}).getArray();
    if (changes == nullptr || changes->isEmpty())
    {
        r.errors.add("changes must be a non-empty list.");
        return r;
    }
    if (changes->size() > 6)
    {
        r.errors.add("changes is too long; send at most 6 changes, or a whole new chain.");
        return r;
    }

    ChainState chain = current;
    chain.explanation = reply.getProperty("explanation", {}).toString();
    auto& slots = chain.slots;
    auto indexOk = [&](int i) { return i >= 0 && i < (int) slots.size(); };

    for (int n = 0; n < changes->size(); ++n)
    {
        auto& ch = changes->getReference(n);
        const auto where = "changes[" + juce::String(n) + "]: ";
        const auto op = ch.getProperty("op", {}).toString();
        if (op == "set")
        {
            const int i = ch.getProperty("module_index", -1);
            if (! indexOk(i)) { r.errors.add(where + "module_index out of range (chain has " + juce::String((int) slots.size()) + " modules)."); continue; }
            slots[(size_t) i].params.set(ch.getProperty("param", {}).toString(), ch.getProperty("value", {}));
        }
        else if (op == "insert")
        {
            const int at = ch.getProperty("at", -1);
            if (at < 0 || at > (int) slots.size()) { r.errors.add(where + "'at' out of range."); continue; }
            ChainSlot s;
            s.moduleId = ch.getProperty("module", {}).toString();
            if (auto* po = ch.getProperty("params", {}).getDynamicObject())
                s.params = po->getProperties();
            slots.insert(slots.begin() + at, s);
        }
        else if (op == "remove")
        {
            const int i = ch.getProperty("module_index", -1);
            if (! indexOk(i)) { r.errors.add(where + "module_index out of range."); continue; }
            slots.erase(slots.begin() + i);
        }
        else if (op == "move")
        {
            const int from = ch.getProperty("from", -1), to = ch.getProperty("to", -1);
            if (! indexOk(from) || ! indexOk(to)) { r.errors.add(where + "'from' or 'to' out of range."); continue; }
            auto s = slots[(size_t) from];
            slots.erase(slots.begin() + from);
            slots.insert(slots.begin() + to, s);
        }
        else if (op == "bypass")
        {
            const int i = ch.getProperty("module_index", -1);
            if (! indexOk(i)) { r.errors.add(where + "module_index out of range."); continue; }
            slots[(size_t) i].bypass = (bool) ch.getProperty("bypass", true);
        }
        else
            r.errors.add(where + "unknown op '" + op + "'. Use set, insert, remove, move or bypass.");
    }
    if (! r.ok())
        return r;
    return validate(std::move(chain), cpuMode);
}

} // namespace plugsalot
