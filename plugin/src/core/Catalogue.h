// Plugsalot - GPL-3.0-or-later
// The module catalogue the AI reads, loaded from the JSON embedded at build time
// (catalogue/modules/*.json), plus each module's full upstream parameter list
// (catalogue/raw/*.json) used to initialise hidden parameters to upstream defaults.
#pragma once

#include <juce_core/juce_core.h>
#include <map>
#include <vector>

namespace plugsalot
{

struct ParamSpec
{
    enum class Type { Float, Int, Enum, Bool };

    juce::String id;          // Plugsalot / AI-facing id
    juce::String upstreamId;  // id the upstream DSP uses
    juce::String name;
    juce::String unit;
    juce::String aiHint;
    Type type = Type::Float;
    double min = 0.0, max = 1.0;
    juce::var defaultValue;
    juce::StringArray options;     // Enum labels
    juce::Array<int> optionValues; // upstream index per option, when narrowed
    juce::String convert;          // "none", "enum_index", "db_to_amp", ...
    juce::String core;             // upstream core enum name, when the raw data records one

    static ParamSpec fromVar(const juce::var& v);

    // Validate an AI-facing value. Returns an empty string when valid.
    juce::String check(const juce::var& value) const;

    // Convert an AI-facing value to the number the upstream DSP expects.
    float toNative(const juce::var& value) const;

    // Human-readable value for the UI.
    juce::String format(const juce::var& value) const;
};

struct KnobSpec
{
    juce::String paramId, label, colour;   // colour: blue, red, grey, cream or black
};

struct ModuleSpec
{
    juce::String id, name, category, kind, purpose;
    juce::StringArray goodFor, avoidFor;
    juce::var cpuPoints;          // int or void (not measured yet)
    bool trackingSafe = true;
    bool offered = true;          // false: runs in chains but is left out of the AI prompt
    juce::NamedValueSet trackingOverrides;
    juce::String licence, upstreamName;
    juce::String maker, tape;     // short upstream name and the masking-tape label for the faceplate
    std::vector<KnobSpec> knobs;
    juce::String knobModeParam;   // when set, knobsByMode picks the knob set by this enum parameter
    std::map<juce::String, std::vector<KnobSpec>> knobsByMode;
    juce::String fileKind;        // "nam" or "ir" when the module loads a file from the library
    std::vector<ParamSpec> params;      // what the AI may set
    std::vector<ParamSpec> rawParams;   // everything upstream declares (for defaults)
    juce::var presets;
    juce::var json;                     // the full catalogue entry

    const ParamSpec* findParam(const juce::String& paramId) const;
    // The faceplate knobs for the given settings (a compressor shows its current mode's controls).
    const std::vector<KnobSpec>& knobsFor(const juce::NamedValueSet& settings) const;
    int cpuCost() const;                // measured points, or the placeholder
};

class Catalogue
{
public:
    static constexpr int unknownCpuCost = 2;

    // Loads the catalogue embedded in the binary.
    static const Catalogue& embedded();

    // Loads from a directory containing modules/ and raw/ (used by tests).
    static Catalogue fromDirectory(const juce::File& catalogueDir);

    const ModuleSpec* find(const juce::String& moduleId) const;
    const std::vector<ModuleSpec>& modules() const { return list; }
    juce::StringArray ids() const;

    // Compact JSON describing every module for the system prompt.
    juce::String toPromptJson() const;

    void add(const juce::var& moduleJson, const juce::var& rawJson);

private:
    std::vector<ModuleSpec> list;
};

} // namespace plugsalot
