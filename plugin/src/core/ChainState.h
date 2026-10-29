// Plugsalot - GPL-3.0-or-later
// A chain as data: the shape the LLM returns, the plugin saves and the UI shows.
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace plugsalot
{

struct ChainSlot
{
    juce::String moduleId;
    juce::NamedValueSet params;   // AI-facing ids and values
    bool bypass = false;
    juce::String file;            // library file name for NAM / cabinet modules
    juce::Uuid instanceId;        // stable identity so DSP state survives edits
};

struct ChainState
{
    juce::String source, stage = "mixing";
    std::vector<ChainSlot> slots;
    juce::String explanation;

    juce::var toVar() const;
    static ChainState fromVar(const juce::var& v); // no validation; use ChainValidator
    juce::String toJson() const { return juce::JSON::toString(toVar(), true); }
    bool isEmpty() const { return slots.empty(); }
};

// Recording context chosen in the dropdowns.
struct SessionContext
{
    juce::String source = "Lead vocal";
    juce::String stage = "mixing";      // tracking | mixing | mastering
    juce::String style = "Pop";
    juce::String character = "Clean";
    juce::String cpuMode = "normal";    // light | normal | high

    juce::var toVar() const;
    static SessionContext fromVar(const juce::var& v);
};

} // namespace plugsalot
