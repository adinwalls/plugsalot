// Plugsalot - GPL-3.0-or-later
// TONE3000 (https://www.tone3000.com) client: sign-in with the OAuth 2.0 device
// authorisation flow (RFC 8628), the bounded tone lists, a tone's models, and
// downloading a model into the user's library.
//
// All calls block. Run them off the message thread. Tokens live only on the
// user's machine and are never sent anywhere except TONE3000.
#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <mutex>
#include <vector>

namespace plugsalot
{

class Tone3000
{
public:
    struct Result
    {
        bool ok = false;
        int status = 0;
        juce::String error;
        explicit operator bool() const { return ok; }
    };

    struct Tokens
    {
        juce::String access, refresh;
        juce::int64 expiresAtMs = 0;
        bool signedIn() const { return refresh.isNotEmpty() || access.isNotEmpty(); }
    };

    struct DeviceCode
    {
        juce::String deviceCode, userCode, verificationUri, verificationUriComplete;
        int expiresIn = 600, interval = 5;
    };

    struct Tone
    {
        juce::String id, title, gear, format, license, creator, avatarUrl, imageUrl, url;
        int modelsCount = 0;
    };

    struct Model
    {
        juce::String id, name, architecture, url, toneId;
        juce::int64 size = 0;
    };

    // The publishable key. PLUGSALOT_TONE3000_CLIENT_ID in the environment wins, then the
    // key compiled into the build. Empty means TONE3000 is not set up in this build.
    static juce::String clientId();
    static bool configured() { return clientId().isNotEmpty(); }
    // PLUGSALOT_TONE3000_BASE points tests at a local stand-in server.
    static juce::String baseUrl();

    Tone3000() = default;
    explicit Tone3000(Tokens initial) : tokens_(std::move(initial)) {}

    Tokens tokens() const;
    bool signedIn() const { return tokens().signedIn(); }
    void signOut();
    // Called (on whichever thread made the request) when tokens are issued, refreshed or cleared.
    std::function<void(const Tokens&)> onTokensChanged;

    // Sign-in: ask for a code, show it to the user, then wait for them to approve it in a browser.
    Result requestDeviceCode(DeviceCode& out);
    Result waitForApproval(const DeviceCode& code, const std::atomic<bool>& cancel);

    // which: trending | latest | favorited | downloaded | created. query only applies to the last three.
    Result list(const juce::String& which, const juce::String& gear, const juce::String& query, int page,
                std::vector<Tone>& out, int& totalPages);
    Result models(const juce::String& toneId, std::vector<Model>& out);
    // Fetches the model file. Written to `dest` only when complete and plausible for a .nam file.
    Result download(const Model& model, const juce::File& dest);

    static Tone parseTone(const juce::var& v);
    static Model parseModel(const juce::var& v, const juce::String& toneId);
    static juce::String licenceLabel(const juce::String& licence);

private:
    Result request(const juce::String& method, const juce::String& url, const juce::String& form, bool bearer,
                   juce::MemoryBlock& body, juce::String* contentTypeOut = nullptr);
    Result getJson(const juce::String& path, juce::var& out);
    Result tokenRequest(const juce::String& form, bool deviceFlow, juce::String& oauthError);
    bool ensureFreshToken(Result& failure);
    void storeTokens(const Tokens& t);

    mutable std::mutex lock;
    Tokens tokens_;
};

} // namespace plugsalot
