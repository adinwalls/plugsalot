// Plugsalot - GPL-3.0-or-later
// One conversation with the AI: sends requests on a background thread,
// validates every reply (layer 2), retries once with the errors, and reports
// the outcome back on the message thread.
#pragma once

#include "LlmClient.h"
#include "PromptBuilder.h"
#include "core/ChainValidator.h"
#include <juce_events/juce_events.h>
#include <functional>

namespace plugsalot
{

struct AiOutcome
{
    enum class Kind { Chain, Question, Error };
    Kind kind = Kind::Error;
    ChainState chain;          // Kind::Chain
    juce::String message;      // explanation, question or error text
    juce::String model;
    bool retried = false;
    bool wasChangeList = false;
};

class AiSession
{
public:
    explicit AiSession(const Catalogue& c);
    ~AiSession();

    bool isBusy() const { return busy.load(); }

    // Message thread. `onDone` is called on the message thread.
    void send(const ProviderSettings& settings, const SessionContext& ctx, const AudioAnalysis& analysis,
              const ChainState& current, const juce::String& userText, std::function<void(AiOutcome)> onDone);

    void clearHistory() { history.clear(); }

    // Run one exchange synchronously (used by the background job and by tests).
    AiOutcome runExchange(const ProviderSettings& settings, const SessionContext& ctx, const AudioAnalysis& analysis,
                          const ChainState& current, const juce::String& userText,
                          std::vector<ChatTurn>& turns) const;

    // Interpret one model reply against the current chain (no network).
    AiOutcome interpret(const juce::String& replyText, const ChainState& current, const juce::String& cpuMode) const;

private:
    const Catalogue& catalogue;
    ChainValidator validator;
    juce::String systemPrompt;
    std::vector<ChatTurn> history;      // message thread only
    juce::ThreadPool pool { 1 };
    std::atomic<bool> busy { false };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace plugsalot
