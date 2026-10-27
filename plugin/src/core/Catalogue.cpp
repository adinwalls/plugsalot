// Plugsalot - GPL-3.0-or-later
#include "Catalogue.h"
#include "PlugsalotData.h"

namespace plugsalot
{

static juce::StringArray toStringArray(const juce::var& v)
{
    juce::StringArray out;
    if (auto* arr = v.getArray())
        for (auto& item : *arr)
            out.add(item.toString());
    return out;
}

ParamSpec ParamSpec::fromVar(const juce::var& v)
{
    ParamSpec p;
    p.id = v.getProperty("id", v.getProperty("upstream_id", {})).toString();
    p.upstreamId = v.getProperty("upstream_id", p.id).toString();
    p.name = v.getProperty("name", p.id).toString();
    p.unit = v.getProperty("unit", {}).toString();
    p.aiHint = v.getProperty("ai_hint", {}).toString();
    p.convert = v.getProperty("convert", "none").toString();
    p.core = v.getProperty("core", {}).toString();
    p.defaultValue = v.getProperty("default", {});

    const auto t = v.getProperty("type", "float").toString();
    p.type = t == "enum" ? Type::Enum : t == "bool" ? Type::Bool : t == "int" ? Type::Int : Type::Float;
    p.min = (double) v.getProperty("min", 0.0);
    p.max = (double) v.getProperty("max", 1.0);
    p.options = toStringArray(v.getProperty("options", {}));
    if (auto* ov = v.getProperty("option_values", {}).getArray())
        for (auto& x : *ov)
            p.optionValues.add((int) x);
    return p;
}

juce::String ParamSpec::check(const juce::var& value) const
{
    switch (type)
    {
        case Type::Float:
        case Type::Int:
        {
            if (! (value.isDouble() || value.isInt() || value.isInt64()) || value.isBool())
                return "'" + id + "' must be a number.";
            const double d = (double) value;
            if (! std::isfinite(d))
                return "'" + id + "' must be a finite number.";
            if (type == Type::Int && d != std::floor(d))
                return "'" + id + "' must be a whole number.";
            if (d < min || d > max)
                return "'" + id + "' = " + juce::String(d) + " is outside " + juce::String(min) + ".." + juce::String(max)
                       + (unit.isNotEmpty() ? " " + unit : juce::String()) + ".";
            return {};
        }
        case Type::Enum:
            if (! options.contains(value.toString()))
                return "'" + id + "' must be one of [" + options.joinIntoString(", ") + "].";
            return {};
        case Type::Bool:
            if (! value.isBool())
                return "'" + id + "' must be true or false.";
            return {};
    }
    return {};
}

float ParamSpec::toNative(const juce::var& value) const
{
    switch (type)
    {
        case Type::Enum:
        {
            const int idx = juce::jmax(0, options.indexOf(value.toString()));
            return (float) (optionValues.isEmpty() ? idx : optionValues[idx]);
        }
        case Type::Bool:
            return (bool) value ? 1.0f : 0.0f;
        case Type::Float:
        case Type::Int:
        default:
        {
            auto d = juce::jlimit(min, max, (double) value);
            if (convert == "db_to_amp")
                d = d <= -144.0 ? 0.0 : std::pow(10.0, d / 20.0);
            return (float) d;
        }
    }
}

juce::String ParamSpec::format(const juce::var& value) const
{
    switch (type)
    {
        case Type::Enum: return value.toString();
        case Type::Bool: return (bool) value ? "On" : "Off";
        case Type::Int:  return juce::String((int) value) + (unit.isNotEmpty() ? " " + unit : "");
        case Type::Float:
        default:
        {
            const double d = (double) value;
            const int decimals = std::abs(d) >= 100.0 ? 0 : std::abs(d) >= 10.0 ? 1 : 2;
            return juce::String(d, decimals) + (unit.isNotEmpty() ? " " + unit : "");
        }
    }
}

const ParamSpec* ModuleSpec::findParam(const juce::String& paramId) const
{
    for (auto& p : params)
        if (p.id == paramId)
            return &p;
    return nullptr;
}

int ModuleSpec::cpuCost() const
{
    return cpuPoints.isInt() || cpuPoints.isInt64() || cpuPoints.isDouble() ? (int) cpuPoints : Catalogue::unknownCpuCost;
}

void Catalogue::add(const juce::var& j, const juce::var& raw)
{
    ModuleSpec m;
    m.json = j;
    m.id = j.getProperty("id", {}).toString();
    m.name = j.getProperty("name", m.id).toString();
    m.category = j.getProperty("category", {}).toString();
    m.kind = j.getProperty("kind", "effect").toString();
    m.purpose = j.getProperty("purpose", {}).toString();
    m.goodFor = toStringArray(j.getProperty("good_for", {}));
    m.avoidFor = toStringArray(j.getProperty("avoid_for", {}));
    m.cpuPoints = j.getProperty("cpu_points", {});
    m.trackingSafe = (bool) j.getProperty("tracking_safe", true);
    if (auto* o = j.getProperty("tracking_overrides", {}).getDynamicObject())
        m.trackingOverrides = o->getProperties();
    auto src = j.getProperty("source", {});
    m.licence = src.getProperty("license", {}).toString();
    m.upstreamName = src.getProperty("upstream_name", {}).toString();
    m.presets = j.getProperty("presets", {});

    if (auto* arr = j.getProperty("params", {}).getArray())
        for (auto& p : *arr)
            m.params.push_back(ParamSpec::fromVar(p));
    if (auto* arr = raw.getProperty("params", {}).getArray())
        for (auto& p : *arr)
            if (! (bool) p.getProperty("unresolved", false) && p.getProperty("role", {}).toString() != "meter")
                m.rawParams.push_back(ParamSpec::fromVar(p));

    list.push_back(std::move(m));
}

const Catalogue& Catalogue::embedded()
{
    static const Catalogue cat = []
    {
        Catalogue c;
        // Binary data resources are named after their file names; module and raw
        // files share names, so pair them by resource order: modules first, raw after.
        juce::Array<juce::var> modules, raws;
        for (int i = 0; i < PlugsalotData::namedResourceListSize; ++i)
        {
            const char* resName = PlugsalotData::namedResourceList[i];
            const juce::String original(PlugsalotData::getNamedResourceOriginalFilename(resName));
            if (! original.endsWithIgnoreCase(".json"))
                continue;
            int size = 0;
            const char* data = PlugsalotData::getNamedResource(resName, size);
            auto parsed = juce::JSON::parse(juce::String::fromUTF8(data, size));
            if (parsed.hasProperty("schema_version") && parsed.hasProperty("purpose"))
                modules.add(parsed);
            else if (parsed.hasProperty("module"))
                raws.add(parsed);
        }
        for (auto& m : modules)
        {
            juce::var raw;
            for (auto& r : raws)
                if (r.getProperty("module", {}).toString() == m.getProperty("id", {}).toString())
                    raw = r;
            c.add(m, raw);
        }
        return c;
    }();
    return cat;
}

Catalogue Catalogue::fromDirectory(const juce::File& dir)
{
    Catalogue c;
    for (auto& f : dir.getChildFile("modules").findChildFiles(juce::File::findFiles, false, "*.json"))
    {
        auto m = juce::JSON::parse(f);
        auto rawFile = dir.getChildFile("raw").getChildFile(f.getFileName());
        c.add(m, rawFile.existsAsFile() ? juce::JSON::parse(rawFile) : juce::var());
    }
    return c;
}

const ModuleSpec* Catalogue::find(const juce::String& moduleId) const
{
    for (auto& m : list)
        if (m.id == moduleId)
            return &m;
    return nullptr;
}

juce::StringArray Catalogue::ids() const
{
    juce::StringArray out;
    for (auto& m : list)
        out.add(m.id);
    return out;
}

juce::String Catalogue::toPromptJson() const
{
    juce::Array<juce::var> mods;
    for (auto& m : list)
    {
        auto* o = new juce::DynamicObject();
        o->setProperty("id", m.id);
        o->setProperty("name", m.name);
        o->setProperty("category", m.category);
        o->setProperty("purpose", m.purpose);
        o->setProperty("good_for", m.json.getProperty("good_for", {}));
        if (m.avoidFor.size() > 0)
            o->setProperty("avoid_for", m.json.getProperty("avoid_for", {}));
        o->setProperty("tracking_safe", m.trackingSafe);
        o->setProperty("cpu_points", m.cpuCost());

        juce::Array<juce::var> ps;
        for (auto& p : m.params)
        {
            auto* po = new juce::DynamicObject();
            po->setProperty("id", p.id);
            switch (p.type)
            {
                case ParamSpec::Type::Enum:
                    po->setProperty("type", "enum");
                    po->setProperty("options", juce::var(p.options));
                    break;
                case ParamSpec::Type::Bool: po->setProperty("type", "bool"); break;
                case ParamSpec::Type::Int:
                case ParamSpec::Type::Float:
                    po->setProperty("type", p.type == ParamSpec::Type::Int ? "int" : "float");
                    po->setProperty("min", p.min);
                    po->setProperty("max", p.max);
                    if (p.unit.isNotEmpty())
                        po->setProperty("unit", p.unit);
                    break;
            }
            po->setProperty("default", p.defaultValue);
            if (p.aiHint.isNotEmpty())
                po->setProperty("hint", p.aiHint);
            ps.add(juce::var(po));
        }
        o->setProperty("params", ps);

        if (auto* presets = m.presets.getArray())
        {
            juce::Array<juce::var> names;
            for (auto& pr : *presets)
                names.add(pr);
            o->setProperty("presets", names);
        }
        mods.add(juce::var(o));
    }
    return juce::JSON::toString(juce::var(mods), true);
}

} // namespace plugsalot
