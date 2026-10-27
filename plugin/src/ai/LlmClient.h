// Plugsalot - GPL-3.0-or-later
// Minimal HTTPS clients for the two providers. Blocking calls: only ever run
// them on a background thread (AiSession does).
#pragma once

#include <juce_core/juce_core.h>
#include <vector>

namespace plugsalot
{

struct ChatTurn
{
    bool fromUser = true;
    juce::String text;
};

struct ProviderSettings
{
    enum class Provider { Gemini, Claude };
    Provider provider = Provider::Gemini;
    juce::String apiKey;
    juce::String model;   // empty = provider default (Gemini: picked automatically)

    static juce::String providerName(Provider p) { return p == Provider::Gemini ? "Google Gemini (free key)" : "Claude (API key)"; }
};

struct LlmResult
{
    bool ok = false;
    juce::String text;    // model output on success
    juce::String error;   // readable error otherwise
    juce::String model;   // model actually used
};

class LlmClient
{
public:
    static constexpr const char* defaultClaudeModel = "claude-haiku-5-5";
    static constexpr const char* fallbackGeminiModel = "gemini-2.5-flash";

    static LlmResult complete(const ProviderSettings& settings, const juce::String& system,
                              const std::vector<ChatTurn>& turns, juce::Thread* thread = nullptr);

    // Gemini: ask Google which models this key can use and pick the newest Flash model.
    static LlmResult pickGeminiModel(const juce::String& apiKey);

    // Quick key check used by the settings pop-up.
    static LlmResult testConnection(const ProviderSettings& settings);

    // Pull the JSON object out of a model reply (strips ``` fences and stray prose).
    static juce::var extractJson(const juce::String& reply);

private:
    static LlmResult post(const juce::String& url, const juce::String& headers, const juce::String& body);
    static LlmResult get(const juce::String& url, const juce::String& headers);
};

} // namespace plugsalot
