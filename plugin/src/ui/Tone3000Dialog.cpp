// Plugsalot - GPL-3.0-or-later
#include "Tone3000Dialog.h"
#include "SettingsDialog.h"
#include "core/Library.h"

namespace plugsalot
{

static const char* const tabQueries[4] = { "trending", "latest", "favorited", "downloaded" };

Tone3000Dialog::Tone3000Dialog(PlugsalotProcessor& p) : proc(p), api(p.tone3000())
{
    auto label = [this](juce::Label& l, float size, juce::Colour c, bool bold = false)
    {
        l.setFont(juce::FontOptions(size, bold ? juce::Font::bold : juce::Font::plain));
        l.setColour(juce::Label::textColourId, c);
        l.setMinimumHorizontalScale(0.8f);
        addAndMakeVisible(l);
    };
    label(heading, 20.0f, theme::accent, true);
    heading.setText("TONE3000", juce::dontSendNotification);
    label(body, 13.5f, theme::text);
    label(code, 30.0f, theme::text, true);
    code.setJustificationType(juce::Justification::centred);
    label(statusLabel, 12.5f, theme::muted);
    label(attribution, 12.0f, theme::muted);

    for (auto* b : { &signInButton, &cancelButton, &closeButton, &useButton, &signOutButton })
        addAndMakeVisible(*b);
    addAndMakeVisible(fullCatalogue);
    fullCatalogue.setColour(juce::HyperlinkButton::textColourId, theme::accent);
    fullCatalogue.setFont(juce::FontOptions(12.5f), false);

    for (int i = 0; i < 4; ++i)
    {
        tabs[i].setClickingTogglesState(true);
        tabs[i].setRadioGroupId(42);
        tabs[i].onClick = [this, i] { tab = i; load(); };
        addAndMakeVisible(tabs[i]);
    }
    tabs[0].setToggleState(true, juce::dontSendNotification);

    gearBox.addItem("All gear", 1);
    gearBox.addItem("Amps", 2);
    gearBox.addItem("Pedals", 3);
    gearBox.addItem("Amp + cab", 4);
    gearBox.setSelectedId(1, juce::dontSendNotification);
    gearBox.onChange = [this] { load(); };
    addAndMakeVisible(gearBox);
    addAndMakeVisible(modelBox);
    search.setTextToShowWhenEmpty("Search titles (favourites and downloads)", theme::muted);
    search.onReturnKey = [this] { load(); };
    addAndMakeVisible(search);
    list.setRowHeight(46);
    list.setColour(juce::ListBox::backgroundColourId, theme::background);
    addAndMakeVisible(list);

    closeButton.onClick = [this] { if (onClose) onClose(); };
    signInButton.onClick = [this] { signIn(); };
    cancelButton.onClick = [this] { cancelSignIn.store(true); setState(State::SignedOut); };
    useButton.onClick = [this] { use(); };
    signOutButton.onClick = [this]
    {
        api->signOut();
        tones.clear();
        list.updateContent();
        setState(State::SignedOut);
    };

    setState(! Tone3000::configured() ? State::NotConfigured : api->signedIn() ? State::Browsing : State::SignedOut);
    if (state == State::Browsing)
        load();
}

Tone3000Dialog::~Tone3000Dialog()
{
    alive->store(false);
    cancelSignIn.store(true);
    pool.removeAllJobs(true, 4000);
}

void Tone3000Dialog::status(const juce::String& text, bool warn)
{
    statusLabel.setColour(juce::Label::textColourId, warn ? theme::warning : theme::muted);
    statusLabel.setText(text, juce::dontSendNotification);
}

void Tone3000Dialog::setState(State s)
{
    state = s;
    const bool browsing = s == State::Browsing;
    const bool signing = s == State::SigningIn;
    signInButton.setVisible(s == State::SignedOut);
    cancelButton.setVisible(signing);
    code.setVisible(signing);
    for (auto& t : tabs) t.setVisible(browsing);
    gearBox.setVisible(browsing);
    search.setVisible(browsing);
    list.setVisible(browsing);
    modelBox.setVisible(browsing);
    useButton.setVisible(browsing);
    signOutButton.setVisible(browsing);
    attribution.setVisible(browsing);
    body.setVisible(! browsing);

    if (s == State::NotConfigured)
        body.setText("TONE3000 sign-in is not set up in this build yet. You can still download captures from tone3000.com in your browser, "
                     "then use \"Add file\" on the NAM Amp module to put them in your library.",
                     juce::dontSendNotification);
    else if (s == State::SignedOut)
        body.setText("Plugsalot can load Neural Amp Modeler (NAM) captures from TONE3000: a library of captures and IRs of real analog gear, "
                     "shared by a global community of musicians. Sign in with your free TONE3000 account to browse and load them here.\n\n"
                     "You sign in on tone3000.com in your browser. Plugsalot never sees your email or password.",
                     juce::dontSendNotification);
    else if (signing)
        body.setText("Open tone3000.com/activate and enter this code (your browser should open it for you):", juce::dontSendNotification);
    body.setVisible(! browsing);
    resized();
    repaint();
}

void Tone3000Dialog::signIn()
{
    cancelSignIn.store(false);
    status("Contacting TONE3000...");
    signInButton.setEnabled(false);
    pool.addJob([this, flag = alive]
    {
        Tone3000::DeviceCode dc;
        auto r = api->requestDeviceCode(dc);
        if (! flag->load()) return;
        if (! r.ok)
        {
            onMessageThread([e = r.error](Tone3000Dialog& d) { d.signInButton.setEnabled(true); d.status(e, true); });
            return;
        }
        onMessageThread([dc](Tone3000Dialog& d)
        {
            d.deviceCode = dc;
            d.code.setText(dc.userCode, juce::dontSendNotification);
            d.signInButton.setEnabled(true);
            d.setState(State::SigningIn);
            d.status("Waiting for you to approve in the browser...");
        });
        const auto url = dc.verificationUriComplete.isNotEmpty() ? dc.verificationUriComplete : dc.verificationUri;
        if (! SettingsDialog::openInBrowser(url))
        {
            juce::SystemClipboard::copyTextToClipboard(url);
            onMessageThread([](Tone3000Dialog& d) { d.status("Couldn't open your browser. The sign-in link is copied: paste it into a browser.", true); });
        }
        auto w = api->waitForApproval(dc, cancelSignIn);
        if (! flag->load()) return;
        onMessageThread([w](Tone3000Dialog& d)
        {
            if (w.ok)
            {
                d.setState(State::Browsing);
                d.load();
            }
            else if (d.state == State::SigningIn)
            {
                d.setState(State::SignedOut);
                d.status(w.error, true);
            }
        });
    });
}

void Tone3000Dialog::load()
{
    status("Loading...");
    const auto which = juce::String(tabQueries[tab]);
    const juce::String gears[] = { {}, "amp", "pedal", "amp-cab" };
    const auto gear = gears[juce::jlimit(0, 3, gearBox.getSelectedId() - 1)];
    const auto query = search.getText().trim();
    search.setEnabled(tab >= 2);
    pool.removeAllJobs(false, 0);
    pool.addJob([this, flag = alive, which, gear, query]
    {
        std::vector<Tone3000::Tone> out;
        int pages = 1;
        auto r = api->list(which, gear, which == "favorited" || which == "downloaded" ? query : juce::String(), 1, out, pages);
        if (! flag->load()) return;
        onMessageThread([r, out](Tone3000Dialog& d)
        {
            if (! r.ok)
            {
                if (! d.api->signedIn())
                    d.setState(State::SignedOut);
                d.status(r.error, true);
                return;
            }
            d.tones = out;
            d.models.clear();
            d.modelBox.clear();
            d.list.updateContent();
            d.list.repaint();
            d.status(out.empty() ? "Nothing here yet." : juce::String(out.size()) + " captures. Pick one to see its models.");
            for (auto& t : d.tones)
                d.fetchAvatar(t.avatarUrl);
        });
    });
}

void Tone3000Dialog::fetchAvatar(const juce::String& url)
{
    if (url.isEmpty() || avatars.count(url) > 0)
        return;
    avatars[url] = {};
    pool.addJob([this, flag = alive, url]
    {
        auto stream = juce::URL(url).createInputStream(juce::URL::InputStreamOptions(juce::URL::ParameterHandling::inAddress)
                                                           .withConnectionTimeoutMs(8000));
        juce::Image img;
        if (stream != nullptr)
        {
            juce::MemoryBlock mb;
            stream->readIntoMemoryBlock(mb, 2 * 1024 * 1024);
            img = juce::ImageFileFormat::loadFrom(mb.getData(), mb.getSize());
        }
        if (! flag->load() || ! img.isValid()) return;
        onMessageThread([url, img](Tone3000Dialog& d) { d.avatars[url] = img; d.list.repaint(); });
    });
}

void Tone3000Dialog::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int) tones.size())
        return;
    const auto& t = tones[(size_t) row];
    g.fillAll(selected ? theme::panelHi : theme::background);
    g.setColour(theme::outline);
    g.drawHorizontalLine(h - 1, 0.0f, (float) w);

    const juce::Rectangle<float> av(10.0f, (h - 30) / 2.0f, 30.0f, 30.0f);
    auto it = avatars.find(t.avatarUrl);
    if (it != avatars.end() && it->second.isValid())
    {
        juce::Path clip;
        clip.addEllipse(av);
        g.saveState();
        g.reduceClipRegion(clip);
        g.drawImage(it->second, av);
        g.restoreState();
    }
    else
    {
        g.setColour(theme::outline);
        g.fillEllipse(av);
        g.setColour(theme::text);
        g.setFont(juce::FontOptions(14.0f, juce::Font::bold));
        g.drawText(t.creator.substring(0, 1).toUpperCase(), av.toNearestInt(), juce::Justification::centred);
    }
    g.setColour(theme::text);
    g.setFont(juce::FontOptions(14.5f, juce::Font::bold));
    g.drawText(t.title, 50, 5, w - 220, 20, juce::Justification::centredLeft, true);
    g.setColour(theme::muted);
    g.setFont(juce::FontOptions(12.0f));
    g.drawText("by " + t.creator + "   |   " + t.gear + " | " + t.format.toUpperCase() + " | " + Tone3000::licenceLabel(t.license),
               50, 24, w - 60, 18, juce::Justification::centredLeft, true);
    g.setColour(theme::accent);
    g.setFont(juce::FontOptions(11.5f, juce::Font::bold));
    g.drawText(juce::String(t.modelsCount) + (t.modelsCount == 1 ? " model" : " models"), w - 160, 5, 150, 18, juce::Justification::centredRight);
}

void Tone3000Dialog::selectedRowsChanged(int row)
{
    models.clear();
    modelBox.clear();
    if (row < 0 || row >= (int) tones.size())
        return;
    const auto& t = tones[(size_t) row];
    attribution.setText(t.title + " by " + t.creator + "  |  " + Tone3000::licenceLabel(t.license)
                        + "  |  on TONE3000. The credit is saved with the file.", juce::dontSendNotification);
    loadModels(t.id);
}

void Tone3000Dialog::loadModels(const juce::String& toneId)
{
    status("Loading models...");
    pool.addJob([this, flag = alive, toneId]
    {
        std::vector<Tone3000::Model> out;
        auto r = api->models(toneId, out);
        if (! flag->load()) return;
        onMessageThread([r, out, toneId](Tone3000Dialog& d)
        {
            const int sel = d.list.getSelectedRow();
            if (sel < 0 || sel >= (int) d.tones.size() || d.tones[(size_t) sel].id != toneId)
                return; // the user moved on
            if (! r.ok) { d.status(r.error, true); return; }
            d.models = out;
            d.modelBox.clear(juce::dontSendNotification);
            for (size_t i = 0; i < out.size(); ++i)
                d.modelBox.addItem(out[i].name + (out[i].architecture.isNotEmpty() ? "  (" + out[i].architecture + ")" : juce::String()), (int) i + 1);
            d.modelBox.setSelectedId(out.empty() ? 0 : 1, juce::dontSendNotification);
            d.status(out.empty() ? "This tone has no downloadable NAM models." : "Choose a model, then \"Use this capture\". A2 and lite models use less CPU.");
        });
    });
}

void Tone3000Dialog::use()
{
    const int sel = list.getSelectedRow(), mi = modelBox.getSelectedId() - 1;
    if (sel < 0 || sel >= (int) tones.size() || mi < 0 || mi >= (int) models.size())
        return;
    const auto tone = tones[(size_t) sel];
    const auto model = models[(size_t) mi];
    useButton.setEnabled(false);
    status("Downloading...");
    pool.addJob([this, flag = alive, tone, model]
    {
        auto dest = Library::newFileFor("nam", tone.title + " - " + model.name + " (" + tone.creator + ")");
        auto r = api->download(model, dest);
        if (r.ok)
        {
            auto* info = new juce::DynamicObject();
            info->setProperty("source", "TONE3000");
            info->setProperty("tone_id", tone.id);
            info->setProperty("tone_title", tone.title);
            info->setProperty("creator", tone.creator);
            info->setProperty("avatar_url", tone.avatarUrl);
            info->setProperty("license", tone.license);
            info->setProperty("url", tone.url);
            info->setProperty("model", model.name);
            Library::writeAttribution(dest, juce::var(info));
        }
        if (! flag->load()) return;
        onMessageThread([r, name = dest.getFileName()](Tone3000Dialog& d)
        {
            d.useButton.setEnabled(true);
            if (! r.ok) { d.status(r.error, true); return; }
            if (d.onPicked) d.onPicked(name);
            if (d.onClose) d.onClose();
        });
    });
}

juce::Rectangle<int> Tone3000Dialog::card() const
{
    return getLocalBounds().withSizeKeepingCentre(juce::jmin(760, getWidth() - 30), juce::jmin(620, getHeight() - 24));
}

void Tone3000Dialog::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.6f));
    g.setColour(theme::panel);
    g.fillRoundedRectangle(card().toFloat(), 10.0f);
    g.setColour(theme::outline);
    g.drawRoundedRectangle(card().toFloat(), 10.0f, 1.0f);
}

void Tone3000Dialog::resized()
{
    auto r = card().reduced(22, 16);
    auto top = r.removeFromTop(30);
    closeButton.setBounds(top.removeFromRight(80));
    if (state == State::Browsing)
    {
        signOutButton.setBounds(top.removeFromRight(90).withTrimmedRight(8));
        heading.setBounds(top.removeFromLeft(180));
        fullCatalogue.setBounds(top);
    }
    else
        heading.setBounds(top);
    r.removeFromTop(8);

    if (state != State::Browsing)
    {
        body.setBounds(r.removeFromTop(state == State::SigningIn ? 60 : 120));
        if (state == State::SigningIn)
        {
            code.setBounds(r.removeFromTop(54));
            cancelButton.setBounds(r.removeFromTop(30).withSizeKeepingCentre(120, 30));
        }
        else if (state == State::SignedOut)
            signInButton.setBounds(r.removeFromTop(34).withSizeKeepingCentre(220, 34));
        r.removeFromTop(10);
        statusLabel.setBounds(r.removeFromTop(40));
        fullCatalogue.setBounds(r.removeFromTop(26));
        return;
    }

    auto tabRow = r.removeFromTop(30);
    const int tw = tabRow.getWidth() / 4;
    for (auto& t : tabs) t.setBounds(tabRow.removeFromLeft(tw).reduced(2, 0));
    r.removeFromTop(6);
    auto filters = r.removeFromTop(28);
    gearBox.setBounds(filters.removeFromLeft(130));
    filters.removeFromLeft(8);
    search.setBounds(filters);
    r.removeFromTop(8);
    auto bottom = r.removeFromBottom(98);
    list.setBounds(r);
    bottom.removeFromTop(6);
    attribution.setBounds(bottom.removeFromTop(20));
    auto row = bottom.removeFromTop(32);
    useButton.setBounds(row.removeFromRight(150));
    row.removeFromRight(8);
    modelBox.setBounds(row);
    statusLabel.setBounds(bottom.removeFromTop(30).withTrimmedTop(4));
}

} // namespace plugsalot
