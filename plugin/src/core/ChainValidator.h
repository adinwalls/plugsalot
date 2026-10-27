// Plugsalot - GPL-3.0-or-later
// Layer 2 of the AI design: hard limits enforced in code, never trusted to the model.
// C++ port of tools/chain_validator.py; tests/TestMain.cpp runs the same example
// chains (catalogue/examples) through both so they cannot drift apart.
#pragma once

#include "Catalogue.h"
#include "ChainState.h"

namespace plugsalot
{

struct ValidationResult
{
    juce::StringArray errors;
    ChainState chain;       // valid chain with tracking overrides applied
    bool ok() const { return errors.isEmpty(); }
};

class ChainValidator
{
public:
    static constexpr int maxModules = 8;
    static int cpuBudget(const juce::String& cpuMode);   // light 6, normal 12, high 20

    explicit ChainValidator(const Catalogue& c) : catalogue(c) {}

    // Validate a full chain reply from the LLM.
    ValidationResult validateChain(const juce::var& reply, const juce::String& cpuMode) const;

    // Apply a change-list reply to the current chain, then validate the result.
    ValidationResult applyChanges(const juce::var& reply, const ChainState& current, const juce::String& cpuMode) const;

    // Validate an already-parsed chain (used after manual edits and undo).
    ValidationResult validate(ChainState chain, const juce::String& cpuMode) const;

private:
    const Catalogue& catalogue;
};

} // namespace plugsalot
