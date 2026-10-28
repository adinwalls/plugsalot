// Plugsalot - GPL-3.0-or-later
// Minimal HTTPS clients for the two providers. Blocking calls: only ever run
// them on a background thread (AiSession and the settings card do).
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
    juce::String model;   // empty = automatic

    static juce::String providerName(Provider p) { return p == Provider::Gemini ? "Google Gemini (free key)" : "Claude (API key)"; }
};

struct ModelInfo
{
    juce::String id;      // what the API expects, e.g. "gemini-3.8-flash"
    juce::String label;   // what the dropdown shows
};

struct LlmResult
{
    bool ok = false;
    juce::String text;    // model output on success
    juce::String error;   // readable error otherwise
    juce::String model;   // model actually used
    int status = 0;       // HTTP status (0 = no response)

    // Busy or rate-limited: worth trying again, or trying another model.
    bool isTemporary() const { return status == 429 || status == 500 || status == 502 || status == 503 || status == 504 || status == 529; }
    // The key itself was refused.
    bool isKeyRejected() const { return status == 401 || status == 403 || (status == 400 && error.containsIgnoreCase("key")); }
};

class LlmClient
{
public:
    static constexpr const char* defaultClaudeModel = "claude-haiku-5-5";
    static constexpr const char* fallbackGeminiModel = "gemini-2.5-flash";

    // Send one conversation. With an empty model, Gemini tries the best free
    // Flash models in turn when one is busy or rate-limited.
    static LlmResult complete(const ProviderSettings& settings, const juce::String& system,
                              const std::vector<ChatTurn>& turns);

    // Models this key can use, best first (also proves the key works).
    static LlmResult listModels(const ProviderSettings& settings, std::vector<ModelInfo>& out);

    // Quick end-to-end check used by the settings card.
    static LlmResult testConnection(const ProviderSettings& settings);

    // Pull the JSON object out of a model reply (strips ``` fences and stray prose).
    static juce::var extractJson(const juce::String& reply);

    // Gemini models tried, in order, when the model is "automatic".
    static juce::StringArray geminiAutoCandidates(const juce::String& apiKey);

private:
    static LlmResult post(const juce::String& url, const juce::String& headers, const juce::String& body);
    static LlmResult get(const juce::String& url, const juce::String& headers);
    static LlmResult completeOnce(const ProviderSettings& settings, const juce::String& model,
                                  const juce::String& system, const std::vector<ChatTurn>& turns);
    static LlmResult listGemini(const juce::String& apiKey, std::vector<ModelInfo>& out);
    static LlmResult listClaude(const juce::String& apiKey, std::vector<ModelInfo>& out);
};

} // namespace plugsalot
