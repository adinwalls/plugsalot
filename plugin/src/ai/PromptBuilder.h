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
    // Estimated key of what the input is playing: 0..11 major, 12..23 minor (tonic = value % 12), -1 unknown.
    int keyIndex = -1;
    float keyConfidence = 0.0f;   // 0..1
    bool keyLocked = false;       // the user committed to this key
    double keySeconds = 0.0;      // how much held singing/playing it is based on

    static juce::String keyName(int index);   // "A minor", "F# major"; empty for -1

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
