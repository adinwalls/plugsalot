// Plugsalot - GPL-3.0-or-later
// Browse TONE3000 captures from inside the plugin: partnership splash and sign-in
// (device flow), then trending / latest / the user's favourites and downloads, each
// with creator name and avatar. Choosing one downloads it into the library.
#pragma once

#include "PluginProcessor.h"
#include "Theme.h"
#include <map>

namespace plugsalot
{

class Tone3000Dialog final : public juce::Component, private juce::ListBoxModel
{
public:
    explicit Tone3000Dialog(PlugsalotProcessor& p);
    ~Tone3000Dialog() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseUp(const juce::MouseEvent&) override {}

    std::function<void()> onClose;
    // Called on the message thread with the library file name once a capture is downloaded.
    std::function<void(const juce::String&)> onPicked;

private:
    enum class State { NotConfigured, SignedOut, SigningIn, Browsing };

    // ListBoxModel
    int getNumRows() override { return (int) tones.size(); }
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged(int row) override;

    juce::Rectangle<int> card() const;
    void setState(State s);
    void signIn();
    void load();
    void loadModels(const juce::String& toneId);
    void use();
    void status(const juce::String& text, bool warn = false);
    void fetchAvatar(const juce::String& url);

    template <typename F> void onMessageThread(F f)
    {
        juce::MessageManager::callAsync([safe = juce::Component::SafePointer<Tone3000Dialog>(this), f]
                                        { if (safe != nullptr) f(*safe); });
    }

    PlugsalotProcessor& proc;
    std::shared_ptr<Tone3000> api;
    State state = State::SignedOut;

    juce::Label heading, body, code, statusLabel, attribution;
    juce::TextButton signInButton { "Sign in with TONE3000" }, cancelButton { "Cancel" }, closeButton { "Close" },
        useButton { "Use this capture" }, signOutButton { "Sign out" };
    juce::HyperlinkButton fullCatalogue { "Browse the full catalogue on tone3000.com", juce::URL("https://www.tone3000.com") };
    juce::TextButton tabs[4] { juce::TextButton("Trending"), juce::TextButton("Latest"), juce::TextButton("My favourites"),
                               juce::TextButton("My downloads") };
    juce::ComboBox gearBox, modelBox;
    juce::TextEditor search;
    juce::ListBox list { "tones", this };

    std::vector<Tone3000::Tone> tones;
    std::vector<Tone3000::Model> models;
    int tab = 0;
    std::map<juce::String, juce::Image> avatars;
    Tone3000::DeviceCode deviceCode;
    std::atomic<bool> cancelSignIn { false };
    juce::ThreadPool pool { 2 };
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
};

} // namespace plugsalot
