// Plugsalot - GPL-3.0-or-later
#include "LlmClient.h"

namespace plugsalot
{

static juce::String readError(const juce::String& body, int status)
{
    auto v = juce::JSON::parse(body);
    auto msg = v.getProperty("error", {}).getProperty("message", {}).toString();
    if (msg.isEmpty())
        msg = body.substring(0, 300);
    if (status == 401 || status == 403 || (status == 400 && msg.containsIgnoreCase("api key")))
        return "The API key was rejected. Check that you copied the whole key. (" + msg + ")";
    if (status == 429)
        return "Rate limit reached for this model. Free keys allow a limited number of requests per minute and per day. (" + msg + ")";
    if (status == 503 || status == 529 || status == 500 || status == 502 || status == 504)
        return "This model is busy on the provider's side right now (HTTP " + juce::String(status) + "). This is temporary.";
    return "HTTP " + juce::String(status) + ": " + msg;
}

static LlmResult request(const juce::String& url, const juce::String& headers, const juce::String* body, int timeoutMs)
{
    LlmResult r;
    int status = 0;
    auto options = juce::URL::InputStreamOptions(body != nullptr ? juce::URL::ParameterHandling::inPostData
                                                                 : juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(timeoutMs)
                       .withStatusCode(&status)
                       .withHttpRequestCmd(body != nullptr ? "POST" : "GET");
    juce::URL url_(url);
    if (body != nullptr)
        url_ = url_.withPOSTData(*body);
    auto stream = url_.createInputStream(options);
    r.status = status;
    if (stream == nullptr)
    {
        r.error = status == 0 ? "Could not reach the AI service. Check your internet connection." : readError({}, status);
        return r;
    }
    r.text = stream->readEntireStreamAsString();
    r.status = status;
    if (status < 200 || status >= 300)
    {
        r.error = readError(r.text, status);
        r.text.clear();
        return r;
    }
    r.ok = true;
    return r;
}

LlmResult LlmClient::post(const juce::String& url, const juce::String& headers, const juce::String& body)
{
    return request(url, headers, &body, 90000);
}

LlmResult LlmClient::get(const juce::String& url, const juce::String& headers)
{
    return request(url, headers, nullptr, 30000);
}

// Test hook: PLUGSALOT_GEMINI_BASE points the client at a local stand-in server.
static juce::String geminiBase()
{
    auto b = juce::SystemStats::getEnvironmentVariable("PLUGSALOT_GEMINI_BASE", {});
    return b.isNotEmpty() ? b : juce::String("https://generativelanguage.googleapis.com");
}

// ---------------------------------------------------------------------------
// Model lists
// ---------------------------------------------------------------------------

// Version number in a model name like "gemini-3.8-flash" -> 3.8
static double geminiVersion(const juce::String& name)
{
    return name.fromFirstOccurrenceOf("gemini-", false, true).upToFirstOccurrenceOf("-", false, false).getDoubleValue();
}

static bool isTextGeminiModel(const juce::var& m, const juce::String& id)
{
    bool canGenerate = false;
    if (auto* arr = m.getProperty("supportedGenerationMethods", {}).getArray())
        for (auto& x : *arr)
            canGenerate |= x.toString() == "generateContent";
    if (! canGenerate || ! id.startsWith("gemini-"))
        return false;
    for (auto bad : { "image", "tts", "audio", "live", "embedding", "robotics", "computer-use", "native", "latest", "learnlm" })
        if (id.contains(bad))
            return false;
    return true;
}

// Rank: stable Flash (newest first), stable Flash-Lite, Pro, then previews.
static int geminiRank(const juce::String& id)
{
    const bool preview = id.contains("preview") || id.contains("exp");
    const int family = id.contains("flash-lite") ? 1 : id.contains("flash") ? 0 : id.contains("pro") ? 2 : 3;
    return (preview ? 10 : 0) + family;
}

LlmResult LlmClient::listGemini(const juce::String& apiKey, std::vector<ModelInfo>& out)
{
    auto r = get(geminiBase() + "/v1beta/models?pageSize=1000", "x-goog-api-key: " + apiKey + "\r\n");
    if (! r.ok)
        return r;
    struct Item { ModelInfo info; int rank; double version; };
    std::vector<Item> items;
    const auto parsed = juce::JSON::parse(r.text);   // must outlive the array pointer below
    if (auto* models = parsed.getProperty("models", {}).getArray())
        for (auto& m : *models)
        {
            const auto id = m.getProperty("name", {}).toString().fromFirstOccurrenceOf("models/", false, false);
            if (! isTextGeminiModel(m, id))
                continue;
            auto label = m.getProperty("displayName", id).toString();
            if (! label.containsIgnoreCase(id))
                label << "  (" << id << ")";
            items.push_back({ { id, label }, geminiRank(id), geminiVersion(id) });
        }
    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b)
                     { return a.rank != b.rank ? a.rank < b.rank : a.version > b.version; });
    out.clear();
    for (auto& i : items)
        out.push_back(i.info);
    return r;
}

LlmResult LlmClient::listClaude(const juce::String& apiKey, std::vector<ModelInfo>& out)
{
    auto r = get("https://api.anthropic.com/v1/models?limit=100",
                 "x-api-key: " + apiKey + "\r\nanthropic-version: 2023-06-01\r\n");
    out.clear();
    const auto parsed = juce::JSON::parse(r.text);   // must outlive the array pointer below
    if (r.ok)
        if (auto* data = parsed.getProperty("data", {}).getArray())
            for (auto& m : *data)
            {
                const auto id = m.getProperty("id", {}).toString();
                if (id.startsWith("claude-"))
                    out.push_back({ id, m.getProperty("display_name", id).toString() + "  (" + id + ")" });
            }
    if (out.empty() && ! r.isKeyRejected())
    {
        // Couldn't list (offline or endpoint unavailable): offer the current models.
        out = { { "claude-haiku-5-5", "Claude Haiku 5.5  (fast, low cost)" },
                { "claude-sonnet-5-5", "Claude Sonnet 5.5  (more detailed)" },
                { "claude-opus-5-5", "Claude Opus 5.5  (most capable, highest cost)" } };
    }
    return r;
}

LlmResult LlmClient::listModels(const ProviderSettings& s, std::vector<ModelInfo>& out)
{
    if (s.apiKey.trim().isEmpty())
        return { false, {}, "Paste your key first.", {}, 0 };
    return s.provider == ProviderSettings::Provider::Claude ? listClaude(s.apiKey.trim(), out)
                                                            : listGemini(s.apiKey.trim(), out);
}

juce::StringArray LlmClient::geminiAutoCandidates(const juce::String& apiKey)
{
    // Cached per key for an hour so chat requests don't list models every time.
    static juce::CriticalSection lock;
    static juce::String cachedKey;
    static juce::StringArray cached;
    static juce::Time cachedAt;
    {
        const juce::ScopedLock sl(lock);
        if (cachedKey == apiKey && ! cached.isEmpty() && juce::Time::getCurrentTime() - cachedAt < juce::RelativeTime::hours(1))
            return cached;
    }
    std::vector<ModelInfo> models;
    juce::StringArray candidates;
    if (listGemini(apiKey, models).ok)
        for (auto& m : models)
            if (geminiRank(m.id) <= 1 && candidates.size() < 4)   // stable Flash and Flash-Lite only
                candidates.add(m.id);
    if (candidates.isEmpty())
        candidates.add(fallbackGeminiModel);
    const juce::ScopedLock sl(lock);
    cachedKey = apiKey;
    cached = candidates;
    cachedAt = juce::Time::getCurrentTime();
    return candidates;
}

// ---------------------------------------------------------------------------
// Requests
// ---------------------------------------------------------------------------

LlmResult LlmClient::completeOnce(const ProviderSettings& s, const juce::String& model,
                                  const juce::String& system, const std::vector<ChatTurn>& turns)
{
    if (s.provider == ProviderSettings::Provider::Claude)
    {
        auto* body = new juce::DynamicObject();
        body->setProperty("model", model);
        body->setProperty("max_tokens", 2048);
        body->setProperty("temperature", 0.3);

        auto* block = new juce::DynamicObject();
        block->setProperty("type", "text");
        block->setProperty("text", system);
        auto* cache = new juce::DynamicObject();
        cache->setProperty("type", "ephemeral");
        block->setProperty("cache_control", juce::var(cache));
        body->setProperty("system", juce::Array<juce::var> { juce::var(block) });

        juce::Array<juce::var> messages;
        for (auto& t : turns)
        {
            auto* m = new juce::DynamicObject();
            m->setProperty("role", t.fromUser ? "user" : "assistant");
            m->setProperty("content", t.text);
            messages.add(juce::var(m));
        }
        body->setProperty("messages", messages);

        auto r = post("https://api.anthropic.com/v1/messages",
                      "content-type: application/json\r\nx-api-key: " + s.apiKey.trim() + "\r\nanthropic-version: 2023-06-01\r\n",
                      juce::JSON::toString(juce::var(body), true));
        r.model = model;
        if (! r.ok)
            return r;
        juce::String text;
        const auto parsed = juce::JSON::parse(r.text);   // must outlive the array pointer below
        if (auto* content = parsed.getProperty("content", {}).getArray())
            for (auto& c : *content)
                if (c.getProperty("type", {}).toString() == "text")
                    text << c.getProperty("text", {}).toString();
        r.text = text;
        r.ok = text.isNotEmpty();
        if (! r.ok)
            r.error = "The AI returned an empty reply.";
        return r;
    }

    // Gemini
    auto* body = new juce::DynamicObject();
    auto* sys = new juce::DynamicObject();
    auto* sysPart = new juce::DynamicObject();
    sysPart->setProperty("text", system);
    sys->setProperty("parts", juce::Array<juce::var> { juce::var(sysPart) });
    body->setProperty("systemInstruction", juce::var(sys));

    juce::Array<juce::var> contents;
    for (auto& t : turns)
    {
        auto* c = new juce::DynamicObject();
        c->setProperty("role", t.fromUser ? "user" : "model");
        auto* part = new juce::DynamicObject();
        part->setProperty("text", t.text);
        c->setProperty("parts", juce::Array<juce::var> { juce::var(part) });
        contents.add(juce::var(c));
    }
    body->setProperty("contents", contents);

    auto* gen = new juce::DynamicObject();
    gen->setProperty("responseMimeType", "application/json");
    gen->setProperty("temperature", 0.3);
    gen->setProperty("maxOutputTokens", 4096);
    body->setProperty("generationConfig", juce::var(gen));

    auto r = post(geminiBase() + "/v1beta/models/" + model + ":generateContent",
                  "content-type: application/json\r\nx-goog-api-key: " + s.apiKey.trim() + "\r\n",
                  juce::JSON::toString(juce::var(body), true));
    r.model = model;
    if (! r.ok)
        return r;
    auto v = juce::JSON::parse(r.text);
    juce::String text;
    if (auto* arr = v.getProperty("candidates", {}).getArray(); arr != nullptr && ! arr->isEmpty())
        if (auto* parts = arr->getReference(0).getProperty("content", {}).getProperty("parts", {}).getArray())
            for (auto& p : *parts)
                if (! (bool) p.getProperty("thought", false))
                    text << p.getProperty("text", {}).toString();
    r.text = text;
    r.ok = text.isNotEmpty();
    if (! r.ok)
    {
        auto reason = v.getProperty("promptFeedback", {}).getProperty("blockReason", {}).toString();
        r.error = reason.isNotEmpty() ? "Gemini blocked the request: " + reason : "The AI returned an empty reply.";
    }
    return r;
}

LlmResult LlmClient::complete(const ProviderSettings& s, const juce::String& system, const std::vector<ChatTurn>& turns)
{
    if (s.apiKey.trim().isEmpty())
        return { false, {}, "No API key set. Open Settings to add one.", {}, 0 };

    juce::StringArray candidates;
    if (s.model.isNotEmpty())
        candidates.add(s.model);
    if (s.provider == ProviderSettings::Provider::Claude)
        candidates.addIfNotAlreadyThere(defaultClaudeModel);
    else
        for (auto& m : geminiAutoCandidates(s.apiKey.trim()))   // a chosen model falls back to these
            candidates.addIfNotAlreadyThere(m);

    // Google's free tier often answers "busy" (503) or "slow down" (429) for a
    // few seconds at a time. Try every model, then wait and go round again,
    // up to three rounds, before giving up.
    LlmResult last;
    const int waitsMs[] = { 0, 4000, 10000 };
    for (int round = 0; round < 3; ++round)
    {
        if (waitsMs[round] > 0)
            juce::Thread::sleep(waitsMs[round]);
        for (auto& model : candidates)
        {
            last = completeOnce(s, model, system, turns);
            if (last.ok || ! last.isTemporary())
                return last;
        }
    }
    last.error << " Plugsalot tried " << candidates.size() << (candidates.size() == 1 ? " model" : " models")
               << " three times over about 15 seconds. Try again in a minute"
               << (s.provider == ProviderSettings::Provider::Gemini ? ", or switch to Claude in Settings." : ".");
    return last;
}

LlmResult LlmClient::testConnection(const ProviderSettings& s)
{
    return complete(s, "Reply with the JSON object {\"ok\": true}.", { { true, "Test" } });
}

juce::var LlmClient::extractJson(const juce::String& reply)
{
    auto t = reply.trim();
    if (t.startsWith("```"))
        t = t.fromFirstOccurrenceOf("\n", false, false).upToLastOccurrenceOf("```", false, false).trim();
    auto direct = juce::JSON::parse(t);
    if (direct.isObject())
        return direct;
    // Fall back to the outermost {...} span.
    const int start = t.indexOfChar('{');
    const int end = t.lastIndexOfChar('}');
    if (start >= 0 && end > start)
        return juce::JSON::parse(t.substring(start, end + 1));
    return {};
}

} // namespace plugsalot
