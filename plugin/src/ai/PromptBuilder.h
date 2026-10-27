// Plugsalot - GPL-3.0-or-later
// Builds the system prompt (guidelines + catalogue) and each user turn
// (context dropdowns, audio analysis, current chain, the user's words).
#pragma once

#include "core/Catalogue.h"
#include "core/ChainState.h"

namespace plugsalot
{

struct AudioAnalysis
{
    bool valid = false;
    float peakDb = -120.0f, rmsDb = -120.0f, crestDb = 0.0f;
    float lowDb = -120.0f, midDb = -120.0f, highDb = -120.0f; // band energy, relative
    double seconds = 0.0;

    juce::String describe() const;
};

class PromptBuilder
{
public:
    static juce::String systemPrompt(const Catalogue& catalogue);

    static juce::String userTurn(const SessionContext& ctx, const AudioAnalysis& analysis,
                                 const ChainState* current, const juce::String& userText);

    static juce::String retryTurn(const juce::StringArray& errors);
};

} // namespace plugsalot
