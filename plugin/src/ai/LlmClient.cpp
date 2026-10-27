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
    if (status == 401 || status == 403)
        return "The API key was rejected (" + juce::String(status) + "). Check it in Settings. " + msg;
    if (status == 429)
        return "Rate limit reached (429). Free keys allow a limited number of requests per minute and per day; wait a moment and try again. " + msg;
    return "HTTP " + juce::String(status) + ": " + msg;
}

LlmResult LlmClient::post(const juce::String& url, const juce::String& headers, const juce::String& body)
{
    LlmResult r;
    int status = 0;
    juce::StringPairArray responseHeaders;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inPostData)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(90000)
                       .withStatusCode(&status)
                       .withResponseHeaders(&responseHeaders)
                       .withHttpRequestCmd("POST");
    auto stream = juce::URL(url).withPOSTData(body).createInputStream(options);
    if (stream == nullptr)
    {
        r.error = "Could not reach the AI service. Check your internet connection.";
        return r;
    }
    r.text = stream->readEntireStreamAsString();
    if (status < 200 || status >= 300)
    {
        r.error = readError(r.text, status);
        r.text.clear();
        return r;
    }
    r.ok = true;
    return r;
}

LlmResult LlmClient::get(const juce::String& url, const juce::String& headers)
{
    LlmResult r;
    int status = 0;
    auto options = juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(30000)
                       .withStatusCode(&status);
    auto stream = juce::URL(url).createInputStream(options);
    if (stream == nullptr)
    {
        r.error = "Could not reach the AI service. Check your internet connection.";
        return r;
    }
    r.text = stream->readEntireStreamAsString();
    if (status < 200 || status >= 300)
    {
        r.error = readError(r.text, status);
        r.text.clear();
        return r;
    }
    r.ok = true;
    return r;
}

// Version number in a model name like "gemini-3.8-flash" -> 3.8
static double geminiVersion(const juce::String& name)
{
    auto rest = name.fromFirstOccurrenceOf("gemini-", false, true);
    return rest.upToFirstOccurrenceOf("-", false, false).getDoubleValue();
}

LlmResult LlmClient::pickGeminiModel(const juce::String& apiKey)
{
    auto r = get("https://generativelanguage.googleapis.com/v1beta/models?pageSize=200",
                 "x-goog-api-key: " + apiKey + "\r\n");
    if (! r.ok)
        return r;
    auto v = juce::JSON::parse(r.text);
    juce::String best;
    double bestVersion = -1.0;
    if (auto* models = v.getProperty("models", {}).getArray())
        for (auto& m : *models)
        {
            auto name = m.getProperty("name", {}).toString().fromFirstOccurrenceOf("models/", false, false);
            auto methods = m.getProperty("supportedGenerationMethods", {});
            bool canGenerate = false;
            if (auto* arr = methods.getArray())
                for (auto& x : *arr)
                    canGenerate |= x.toString() == "generateContent";
            // Stable Flash text models only: skip lite, image, audio, live, preview and experimental variants.
            const bool wanted = canGenerate && name.startsWith("gemini-") && name.contains("flash")
                             && ! name.contains("lite") && ! name.contains("image") && ! name.contains("tts")
                             && ! name.contains("audio") && ! name.contains("live") && ! name.contains("exp")
                             && ! name.contains("preview") && ! name.contains("thinking");
            if (wanted && geminiVersion(name) > bestVersion)
            {
                bestVersion = geminiVersion(name);
                best = name;
            }
        }
    LlmResult out;
    out.ok = true;
    out.model = best.isNotEmpty() ? best : juce::String(fallbackGeminiModel);
    return out;
}

LlmResult LlmClient::complete(const ProviderSettings& s, const juce::String& system,
                              const std::vector<ChatTurn>& turns, juce::Thread*)
{
    if (s.apiKey.trim().isEmpty())
        return { false, {}, "No API key set. Open Settings to add one.", {} };

    if (s.provider == ProviderSettings::Provider::Claude)
    {
        auto* body = new juce::DynamicObject();
        const auto model = s.model.isNotEmpty() ? s.model : juce::String(defaultClaudeModel);
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
        if (! r.ok)
            return r;
        auto v = juce::JSON::parse(r.text);
        juce::String text;
        if (auto* content = v.getProperty("content", {}).getArray())
            for (auto& c : *content)
                if (c.getProperty("type", {}).toString() == "text")
                    text << c.getProperty("text", {}).toString();
        return { text.isNotEmpty(), text, text.isEmpty() ? "The AI returned an empty reply." : juce::String(), model };
    }

    // Gemini
    auto model = s.model;
    if (model.isEmpty())
    {
        auto picked = pickGeminiModel(s.apiKey.trim());
        if (! picked.ok)
            return picked;
        model = picked.model;
    }

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

    auto r = post("https://generativelanguage.googleapis.com/v1beta/models/" + model + ":generateContent",
                  "content-type: application/json\r\nx-goog-api-key: " + s.apiKey.trim() + "\r\n",
                  juce::JSON::toString(juce::var(body), true));
    if (! r.ok)
        return r;
    auto v = juce::JSON::parse(r.text);
    juce::String text;
    auto candidates = v.getProperty("candidates", {});
    if (auto* arr = candidates.getArray(); arr != nullptr && ! arr->isEmpty())
        if (auto* parts = arr->getReference(0).getProperty("content", {}).getProperty("parts", {}).getArray())
            for (auto& p : *parts)
                if (! (bool) p.getProperty("thought", false))
                    text << p.getProperty("text", {}).toString();
    if (text.isEmpty())
    {
        auto reason = v.getProperty("promptFeedback", {}).getProperty("blockReason", {}).toString();
        return { false, {}, reason.isNotEmpty() ? "Gemini blocked the request: " + reason : "The AI returned an empty reply.", model };
    }
    return { true, text, {}, model };
}

LlmResult LlmClient::testConnection(const ProviderSettings& s)
{
    if (s.provider == ProviderSettings::Provider::Gemini && s.model.isEmpty())
    {
        auto picked = pickGeminiModel(s.apiKey.trim());
        if (! picked.ok)
            return picked;
        auto s2 = s;
        s2.model = picked.model;
        auto r = complete(s2, "Reply with the JSON object {\"ok\": true}.", { { true, "Test" } });
        r.model = picked.model;
        return r;
    }
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
