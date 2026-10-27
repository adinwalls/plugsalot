// Plugsalot - GPL-3.0-or-later
#include "AiSession.h"

namespace plugsalot
{

static constexpr size_t maxHistoryTurns = 8;

AiSession::AiSession(const Catalogue& c)
    : catalogue(c), validator(c), systemPrompt(PromptBuilder::systemPrompt(c))
{
}

AiSession::~AiSession()
{
    alive->store(false);
    pool.removeAllJobs(true, 5000);
}

AiOutcome AiSession::interpret(const juce::String& replyText, const ChainState& current, const juce::String& cpuMode) const
{
    AiOutcome out;
    auto json = LlmClient::extractJson(replyText);
    if (! json.isObject())
    {
        out.message = "The reply was not valid JSON.";
        return out;
    }
    if (json.hasProperty("question") && ! json.hasProperty("chain") && ! json.hasProperty("changes"))
    {
        out.kind = AiOutcome::Kind::Question;
        out.message = json.getProperty("question", {}).toString();
        return out;
    }

    ValidationResult r;
    if (json.hasProperty("changes"))
    {
        out.wasChangeList = true;
        if (current.isEmpty())
            r.errors.add("There is no current chain to change; send a full chain instead.");
        else
            r = validator.applyChanges(json, current, cpuMode);
    }
    else
        r = validator.validateChain(json, cpuMode);

    if (! r.ok())
    {
        out.message = r.errors.joinIntoString("\n");
        return out;
    }
    out.kind = AiOutcome::Kind::Chain;
    out.chain = r.chain;
    out.message = r.chain.explanation;
    return out;
}

AiOutcome AiSession::runExchange(const ProviderSettings& settings, const SessionContext& ctx, const AudioAnalysis& analysis,
                                 const ChainState& current, const juce::String& userText,
                                 std::vector<ChatTurn>& turns) const
{
    turns.push_back({ true, PromptBuilder::userTurn(ctx, analysis, current.isEmpty() ? nullptr : &current, userText) });

    for (int attempt = 0; attempt < 2; ++attempt)
    {
        auto reply = LlmClient::complete(settings, systemPrompt, turns);
        if (! reply.ok)
        {
            AiOutcome err;
            err.message = reply.error;
            err.model = reply.model;
            turns.pop_back();
            return err;
        }
        turns.push_back({ false, reply.text });

        auto outcome = interpret(reply.text, current, ctx.cpuMode);
        outcome.model = reply.model;
        outcome.retried = attempt > 0;
        if (outcome.kind != AiOutcome::Kind::Error)
            return outcome;

        if (attempt == 0)
            turns.push_back({ true, PromptBuilder::retryTurn(juce::StringArray::fromLines(outcome.message)) });
        else
        {
            outcome.message = "The AI's suggestion broke Plugsalot's safety rules twice, so nothing was changed. Details:\n"
                              + outcome.message;
            return outcome;
        }
    }
    return {};
}

void AiSession::send(const ProviderSettings& settings, const SessionContext& ctx, const AudioAnalysis& analysis,
                     const ChainState& current, const juce::String& userText, std::function<void(AiOutcome)> onDone)
{
    if (busy.exchange(true))
        return;

    // Trim history so prompts stay small; keep whole user/assistant pairs.
    while (history.size() > maxHistoryTurns)
        history.erase(history.begin(), history.begin() + 2);

    auto turns = std::make_shared<std::vector<ChatTurn>>(history);
    auto isAlive = alive;
    pool.addJob([this, settings, ctx, analysis, current, userText, turns, isAlive, onDone]
    {
        auto outcome = runExchange(settings, ctx, analysis, current, userText, *turns);
        juce::MessageManager::callAsync([this, outcome, turns, isAlive, onDone]
        {
            if (! isAlive->load())
                return;
            if (outcome.kind != AiOutcome::Kind::Error)
                history = *turns;   // keep successful exchanges (including any retry) as context
            busy.store(false);
            onDone(outcome);
        });
    });
}

} // namespace plugsalot
