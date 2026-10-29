// Plugsalot - GPL-3.0-or-later
#include "Tone3000.h"

#ifndef PLUGSALOT_TONE3000_CLIENT_ID
 #define PLUGSALOT_TONE3000_CLIENT_ID ""
#endif

namespace plugsalot
{

juce::String Tone3000::clientId()
{
    auto env = juce::SystemStats::getEnvironmentVariable("PLUGSALOT_TONE3000_CLIENT_ID", {}).trim();
    return env.isNotEmpty() ? env : juce::String(PLUGSALOT_TONE3000_CLIENT_ID).trim();
}

juce::String Tone3000::baseUrl()
{
    auto b = juce::SystemStats::getEnvironmentVariable("PLUGSALOT_TONE3000_BASE", {}).trim();
    return b.isNotEmpty() ? b.trimCharactersAtEnd("/") : juce::String("https://www.tone3000.com/api/v1");
}

Tone3000::Tokens Tone3000::tokens() const
{
    std::lock_guard<std::mutex> g(lock);
    return tokens_;
}

void Tone3000::storeTokens(const Tokens& t)
{
    {
        std::lock_guard<std::mutex> g(lock);
        tokens_ = t;
    }
    if (onTokensChanged)
        onTokensChanged(t);
}

void Tone3000::signOut() { storeTokens({}); }

// ---------------------------------------------------------------------------

Tone3000::Result Tone3000::request(const juce::String& method, const juce::String& url, const juce::String& form,
                                   bool bearer, juce::MemoryBlock& body, juce::String* contentTypeOut)
{
    Result r;
    juce::String headers = "Accept: application/json\r\n";
    if (form.isNotEmpty())
        headers << "Content-Type: application/x-www-form-urlencoded\r\n";
    if (bearer)
        headers << "Authorization: Bearer " << tokens().access << "\r\n";

    int status = 0;
    auto options = juce::URL::InputStreamOptions(form.isNotEmpty() ? juce::URL::ParameterHandling::inPostData
                                                                    : juce::URL::ParameterHandling::inAddress)
                       .withExtraHeaders(headers)
                       .withConnectionTimeoutMs(20000)
                       .withStatusCode(&status)
                       .withHttpRequestCmd(method);
    juce::URL u(url);
    if (form.isNotEmpty())
        u = u.withPOSTData(form);
    auto stream = u.createInputStream(options);
    r.status = status;
    if (stream == nullptr)
    {
        r.error = "Could not reach TONE3000. Check your internet connection.";
        return r;
    }
    body.reset();
    stream->readIntoMemoryBlock(body, 256 * 1024 * 1024);
    if (contentTypeOut != nullptr)
        *contentTypeOut = {};
    r.ok = status >= 200 && status < 300;
    if (! r.ok)
    {
        auto parsed = juce::JSON::parse(body.toString());
        auto msg = parsed.getProperty("error_description", parsed.getProperty("error", {})).toString();
        r.error = "TONE3000 answered " + juce::String(status) + (msg.isNotEmpty() ? ": " + msg : juce::String());
    }
    return r;
}

Tone3000::Result Tone3000::tokenRequest(const juce::String& form, bool, juce::String& oauthError)
{
    juce::MemoryBlock body;
    auto r = request("POST", baseUrl() + "/oauth/token", form, false, body);
    auto parsed = juce::JSON::parse(body.toString());
    oauthError = parsed.getProperty("error", {}).toString();
    if (! r.ok)
        return r;
    Tokens t;
    t.access = parsed.getProperty("access_token", {}).toString();
    t.refresh = parsed.getProperty("refresh_token", {}).toString();
    const int expiresIn = (int) parsed.getProperty("expires_in", 3600);
    t.expiresAtMs = juce::Time::currentTimeMillis() + (juce::int64) expiresIn * 1000;
    if (t.access.isEmpty())
    {
        r.ok = false;
        r.error = "TONE3000 did not return an access token.";
        return r;
    }
    if (t.refresh.isEmpty())
        t.refresh = tokens().refresh; // keep the old one if a refresh did not issue a new one
    storeTokens(t);
    return r;
}

bool Tone3000::ensureFreshToken(Result& failure)
{
    auto t = tokens();
    if (t.access.isNotEmpty() && juce::Time::currentTimeMillis() < t.expiresAtMs - 60000)
        return true;
    if (t.refresh.isEmpty())
    {
        failure.error = "Sign in to TONE3000 first.";
        return false;
    }
    juce::String oauthError;
    auto r = tokenRequest("grant_type=refresh_token&refresh_token=" + juce::URL::addEscapeChars(t.refresh, true)
                              + "&client_id=" + juce::URL::addEscapeChars(clientId(), true),
                          false, oauthError);
    if (r.ok)
        return true;
    if (oauthError == "invalid_grant")
    {
        signOut();
        failure.error = "Your TONE3000 sign-in expired. Please sign in again.";
    }
    else
        failure = r;
    return false;
}

// ---------------------------------------------------------------------------

Tone3000::Result Tone3000::requestDeviceCode(DeviceCode& out)
{
    juce::MemoryBlock body;
    auto r = request("POST", baseUrl() + "/oauth/device_authorization",
                     "client_id=" + juce::URL::addEscapeChars(clientId(), true) + "&scope=read", false, body);
    if (! r.ok)
        return r;
    auto j = juce::JSON::parse(body.toString());
    out.deviceCode = j.getProperty("device_code", {}).toString();
    out.userCode = j.getProperty("user_code", {}).toString();
    out.verificationUri = j.getProperty("verification_uri", "https://www.tone3000.com/activate").toString();
    out.verificationUriComplete = j.getProperty("verification_uri_complete", {}).toString();
    out.expiresIn = (int) j.getProperty("expires_in", 600);
    out.interval = juce::jmax(1, (int) j.getProperty("interval", 5));
    if (out.deviceCode.isEmpty() || out.userCode.isEmpty())
    {
        r.ok = false;
        r.error = "TONE3000 sent an unexpected reply to the sign-in request.";
    }
    return r;
}

Tone3000::Result Tone3000::waitForApproval(const DeviceCode& code, const std::atomic<bool>& cancel)
{
    Result r;
    int interval = code.interval;
    const auto deadline = juce::Time::currentTimeMillis() + (juce::int64) code.expiresIn * 1000;
    const auto form = "grant_type=" + juce::URL::addEscapeChars("urn:ietf:params:oauth:grant-type:device_code", true)
                      + "&device_code=" + juce::URL::addEscapeChars(code.deviceCode, true)
                      + "&client_id=" + juce::URL::addEscapeChars(clientId(), true);
    while (juce::Time::currentTimeMillis() < deadline)
    {
        for (int ms = 0; ms < interval * 1000; ms += 100)
        {
            if (cancel.load())
            {
                r.error = "Sign-in cancelled.";
                return r;
            }
            juce::Thread::sleep(100);
        }
        juce::String oauthError;
        r = tokenRequest(form, true, oauthError);
        if (r.ok)
            return r;
        if (oauthError == "authorization_pending")
            continue;
        if (oauthError == "slow_down")
        {
            interval += 5;
            continue;
        }
        if (oauthError == "access_denied")
            r.error = "Sign-in was declined on the TONE3000 page.";
        else if (oauthError == "expired_token")
            r.error = "The sign-in code expired. Try again.";
        return r;
    }
    r.ok = false;
    r.error = "The sign-in code expired. Try again.";
    return r;
}

// ---------------------------------------------------------------------------

Tone3000::Result Tone3000::getJson(const juce::String& path, juce::var& out)
{
    Result f;
    if (! ensureFreshToken(f))
        return f;
    juce::MemoryBlock body;
    auto r = request("GET", baseUrl() + path, {}, true, body);
    if (! r.ok && r.status == 401)
    {   // the token may have been revoked: refresh once and retry
        auto t = tokens();
        t.expiresAtMs = 0;
        storeTokens(t);
        if (! ensureFreshToken(f))
            return f;
        r = request("GET", baseUrl() + path, {}, true, body);
    }
    if (r.ok)
    {
        out = juce::JSON::parse(body.toString());
        if (! out.isObject() && ! out.isArray())
        {
            r.ok = false;
            r.error = "TONE3000 sent a reply Plugsalot could not read.";
        }
    }
    else if (r.status == 429)
        r.error = "TONE3000 is rate-limiting requests. Wait a minute and try again.";
    return r;
}

Tone3000::Tone Tone3000::parseTone(const juce::var& v)
{
    Tone t;
    t.id = v.getProperty("id", {}).toString();
    t.title = v.getProperty("title", {}).toString();
    t.gear = v.getProperty("gear", {}).toString();
    t.format = v.getProperty("format", {}).toString();
    t.license = v.getProperty("license", {}).toString();
    t.url = v.getProperty("url", {}).toString();
    t.modelsCount = (int) v.getProperty("models_count", 0);
    auto user = v.getProperty("user", {});
    auto display = user.getProperty("display_name", {}).toString();
    t.creator = display.isNotEmpty() ? display : user.getProperty("username", {}).toString();
    t.avatarUrl = user.getProperty("avatar_url", {}).toString();
    if (auto* imgs = v.getProperty("images", {}).getArray())
        if (imgs->size() > 0)
            t.imageUrl = (*imgs)[0].toString();
    return t;
}

Tone3000::Model Tone3000::parseModel(const juce::var& v, const juce::String& toneId)
{
    Model m;
    m.id = v.getProperty("id", {}).toString();
    m.name = v.getProperty("name", {}).toString();
    m.url = v.getProperty("model_url", {}).toString();
    m.size = (juce::int64) v.getProperty("size", 0);
    m.toneId = v.getProperty("tone_id", toneId).toString();
    const auto arch = v.getProperty("architecture_version", {}).toString();
    m.architecture = arch == "1" ? "A1" : arch == "2" ? "A2" : arch.isNotEmpty() ? arch : juce::String();
    return m;
}

juce::String Tone3000::licenceLabel(const juce::String& l)
{
    if (l == "t3k") return "TONE3000 licence";
    if (l == "cco") return "CC0";
    if (l.startsWith("cc-")) return "CC " + l.fromFirstOccurrenceOf("cc-", false, false).toUpperCase();
    return l;
}

Tone3000::Result Tone3000::list(const juce::String& which, const juce::String& gear, const juce::String& query, int page,
                                std::vector<Tone>& out, int& totalPages)
{
    juce::String path = "/tones/" + which;
    juce::StringArray params;
    if (gear.isNotEmpty())
        params.add("gear=" + juce::URL::addEscapeChars(gear, true));
    const bool paged = which == "favorited" || which == "downloaded" || which == "created";
    if (paged)
    {
        params.add("page=" + juce::String(juce::jmax(1, page)));
        params.add("page_size=25");
        if (query.isNotEmpty())
            params.add("query=" + juce::URL::addEscapeChars(query, true));
    }
    if (params.size() > 0)
        path << "?" << params.joinIntoString("&");

    juce::var j;
    auto r = getJson(path, j);
    if (! r.ok)
        return r;
    out.clear();
    if (auto* arr = j.getProperty("data", {}).getArray())
        for (auto& t : *arr)
            out.push_back(parseTone(t));
    totalPages = juce::jmax(1, (int) j.getProperty("total_pages", 1));
    return r;
}

Tone3000::Result Tone3000::models(const juce::String& toneId, std::vector<Model>& out)
{
    juce::var j;
    auto r = getJson("/models?tone_id=" + juce::URL::addEscapeChars(toneId, true) + "&page_size=100", j);
    if (! r.ok)
        return r;
    out.clear();
    if (auto* arr = j.getProperty("data", {}).getArray())
        for (auto& m : *arr)
            out.push_back(parseModel(m, toneId));
    return r;
}

Tone3000::Result Tone3000::download(const Model& model, const juce::File& dest)
{
    Result f;
    if (model.url.isEmpty())
    {
        f.error = "This model has no download link.";
        return f;
    }
    if (! ensureFreshToken(f))
        return f;
    juce::MemoryBlock body;
    auto r = request("GET", model.url, {}, true, body);
    if (! r.ok)
        return r;
    if (body.getSize() < 16 || body[0] != '{')
    {
        r.ok = false;
        r.error = "The downloaded file does not look like a NAM capture.";
        return r;
    }
    auto temp = dest.getSiblingFile(dest.getFileName() + ".part");
    if (! temp.replaceWithData(body.getData(), body.getSize()) || ! temp.moveFileTo(dest))
    {
        temp.deleteFile();
        r.ok = false;
        r.error = "Could not save the capture to the library folder.";
    }
    return r;
}

} // namespace plugsalot
