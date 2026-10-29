// Plugsalot - GPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace plugsalot
{

static juce::PropertiesFile::Options settingsOptions()
{
    juce::PropertiesFile::Options o;
    o.applicationName = "Plugsalot";
    o.filenameSuffix = ".settings";
    o.folderName = "Plugsalot";
    o.osxLibrarySubFolder = "Application Support";
    o.commonToAllUsers = false;
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    return o;
}

PlugsalotProcessor::PlugsalotProcessor()
    : AudioProcessor(BusesProperties()
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      catalogue(Catalogue::embedded()),
      validator(catalogue),
      ai(catalogue)
{
    settingsFile = std::make_unique<juce::PropertiesFile>(settingsOptions());
    addChat(ChatMessage::From::System,
            "Pick what you're recording above, then tell me the sound you want, or just press Send for a starting chain.");

    // PLUGSALOT_DEMO=1 loads a sample vocal chain, to try the interface without an API key.
    if (juce::SystemStats::getEnvironmentVariable("PLUGSALOT_DEMO", {}).isNotEmpty())
    {
        auto demo = juce::JSON::parse(R"({"schema_version":1,"source":"Lead vocal","stage":"mixing","chain":[
            {"module":"console_eq","params":{"hpf_enabled":true,"hpf_hz":90,"lm_hz":300,"lm_gain":-2.5}},
            {"module":"multicomp","params":{"mode":"Opto","opto_peak_reduction":45,"auto_makeup":true}},
            {"module":"deess","params":{"intensity":0.4}},
            {"module":"plate","params":{"wet":15,"predelay":30,"decay":0.9,"low_cut":200}}],
            "explanation":"Demo chain: high-pass and a small mud cut, smooth opto levelling, gentle de-essing and a short plate."})");
        auto r = validator.validateChain(demo, ctx.cpuMode);
        if (r.ok())
        {
            applyChain(r.chain, false);
            addChat(ChatMessage::From::Assistant, r.chain.explanation);
        }
    }
    engine.onMessage = [this](const juce::String& m) { postNote(m); };
    startTimerHz(5);
}

PlugsalotProcessor::~PlugsalotProcessor()
{
    stopTimer();
}

bool PlugsalotProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::stereo() && out != juce::AudioChannelSet::mono())
        return false;
    return in == out || (in == juce::AudioChannelSet::mono() && out == juce::AudioChannelSet::stereo());
}

void PlugsalotProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    work.setSize(2, juce::jmax(1, samplesPerBlock), false, true, true);
    analyser.prepare(sampleRate);
    engine.prepare(sampleRate, samplesPerBlock);
    reportedLatency = engine.latencySamples();
    setLatencySamples(reportedLatency);
}

void PlugsalotProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int inCh = getTotalNumInputChannels();
    const int outCh = getTotalNumOutputChannels();
    for (int c = inCh; c < outCh; ++c)
        buffer.clear(c, 0, n);

    analyser.push(buffer);

    if (n > work.getNumSamples())
        work.setSize(2, n, false, true, true); // hosts occasionally exceed the announced size

    // Work in stereo regardless of the host layout.
    work.copyFrom(0, 0, buffer, 0, 0, n);
    work.copyFrom(1, 0, buffer, inCh > 1 ? 1 : 0, 0, n);
    juce::AudioBuffer<float> view(work.getArrayOfWritePointers(), 2, n);
    engine.process(view);

    // Safety: never pass NaN/inf or a runaway level (> +12 dBFS) to speakers.
    const float peak = juce::jmax(view.getMagnitude(0, 0, n), view.getMagnitude(1, 0, n));
    if (! std::isfinite(peak) || peak > 4.0f)
    {
        safetyMuted.store(true);
        buffer.clear();
        return;
    }

    if (outCh == 1)
    {
        buffer.copyFrom(0, 0, view, 0, 0, n);
        buffer.addFrom(0, 0, view, 1, 0, n);
        buffer.applyGain(0, 0, n, 0.5f);
    }
    else
    {
        buffer.copyFrom(0, 0, view, 0, 0, n);
        buffer.copyFrom(1, 0, view, 1, 0, n);
    }
}

void PlugsalotProcessor::timerCallback()
{
    engine.collectGarbage();
    const int latency = engine.latencySamples();
    if (latency != reportedLatency)
    {
        reportedLatency = latency;
        setLatencySamples(latency);
        sendChangeMessage();
    }
    if (safetyMuted.exchange(false))
    {
        addChat(ChatMessage::From::System,
                "Safety mute: the chain produced an invalid or extremely loud signal, so a block was silenced. "
                "Try Undo, or lower drive and gain settings.");
        sendChangeMessage();
    }
}

void PlugsalotProcessor::addChat(ChatMessage::From from, const juce::String& text)
{
    chat.push_back({ from, text });
    if (chat.size() > 200)
        chat.erase(chat.begin(), chat.begin() + 50);
}

int PlugsalotProcessor::chainCpuPoints() const
{
    int total = 0;
    for (auto& s : chain.slots)
        if (auto* m = catalogue.find(s.moduleId))
            total += m->cpuCost();
    return total;
}

void PlugsalotProcessor::applyChain(ChainState newChain, bool pushUndo)
{
    if (pushUndo)
    {
        undoStack.push_back(chain);
        if (undoStack.size() > 50)
            undoStack.erase(undoStack.begin());
    }
    chain = std::move(newChain);
    engine.setChain(chain);
    sendChangeMessage();
}

void PlugsalotProcessor::askAi(const juce::String& userText)
{
    auto settings = loadProviderSettings();
    if (settings.apiKey.isEmpty())
    {
        addChat(ChatMessage::From::System, "No API key is saved yet, so the setup card opened. Paste your key there, close it, and send again.");
        sendChangeMessage();
        if (onNeedsApiKey)
            onNeedsApiKey();
        return;
    }
    if (ai.isBusy())
        return;

    addChat(ChatMessage::From::User, userText.trim().isNotEmpty() ? userText.trim() : "Set up a starting chain.");
    sendChangeMessage();

    ai.send(settings, ctx, analyser.snapshot(), chain, userText,
            [this](AiOutcome outcome)
            {
                lastModelUsed = outcome.model;
                switch (outcome.kind)
                {
                    case AiOutcome::Kind::Chain:
                        applyChain(outcome.chain, true);
                        addChat(ChatMessage::From::Assistant, outcome.message.isNotEmpty() ? outcome.message : "Chain updated.");
                        break;
                    case AiOutcome::Kind::Question:
                        addChat(ChatMessage::From::Assistant, outcome.message);
                        break;
                    case AiOutcome::Kind::Error:
                        addChat(ChatMessage::From::System, outcome.message);
                        break;
                }
                sendChangeMessage();
            });
}

void PlugsalotProcessor::setManualParam(int slot, const juce::String& paramId, const juce::var& value)
{
    if (slot < 0 || slot >= (int) chain.slots.size())
        return;
    auto* m = catalogue.find(chain.slots[(size_t) slot].moduleId);
    auto* p = m != nullptr ? m->findParam(paramId) : nullptr;
    if (p == nullptr || p->check(value).isNotEmpty())
        return;
    chain.slots[(size_t) slot].params.set(paramId, value);
    engine.setParam(slot, paramId, value);
}

void PlugsalotProcessor::setSlotFile(int slot, const juce::String& fileName)
{
    if (slot < 0 || slot >= (int) chain.slots.size() || chain.slots[(size_t) slot].file == fileName)
        return;
    auto c = chain;
    c.slots[(size_t) slot].file = fileName;
    applyChain(std::move(c), true);
}

void PlugsalotProcessor::postNote(const juce::String& text)
{
    addChat(ChatMessage::From::System, text);
    sendChangeMessage();
}

void PlugsalotProcessor::setManualBypass(int slot, bool bypass)
{
    if (slot < 0 || slot >= (int) chain.slots.size())
        return;
    chain.slots[(size_t) slot].bypass = bypass;
    engine.setBypass(slot, bypass);
    sendChangeMessage();
}

void PlugsalotProcessor::removeSlot(int slot)
{
    if (slot < 0 || slot >= (int) chain.slots.size())
        return;
    auto c = chain;
    c.slots.erase(c.slots.begin() + slot);
    if (c.slots.empty())
    {
        clearChain();
        return;
    }
    auto r = validator.validate(c, ctx.cpuMode);
    applyChain(r.ok() ? r.chain : c, true);
}

void PlugsalotProcessor::clearChain()
{
    applyChain({}, true);
    ai.clearHistory();
}

void PlugsalotProcessor::undo()
{
    if (undoStack.empty())
        return;
    auto previous = undoStack.back();
    undoStack.pop_back();
    applyChain(previous, false);
    addChat(ChatMessage::From::System, "Undone: back to the previous chain.");
    sendChangeMessage();
}

ProviderSettings PlugsalotProcessor::loadProviderSettings() const
{
    return loadProviderSettingsFor(settingsFile->getValue("provider", "gemini") == "claude" ? ProviderSettings::Provider::Claude
                                                                                            : ProviderSettings::Provider::Gemini);
}

ProviderSettings PlugsalotProcessor::loadProviderSettingsFor(ProviderSettings::Provider p) const
{
    // Another Plugsalot instance (or the standalone app) may have saved a key since this one loaded.
    if (! settingsFile->needsToBeSaved())
        settingsFile->reload();
    ProviderSettings s;
    s.provider = p;
    s.apiKey = settingsFile->getValue(s.provider == ProviderSettings::Provider::Claude ? "claudeKey" : "geminiKey");
    s.model = settingsFile->getValue(s.provider == ProviderSettings::Provider::Claude ? "claudeModel" : "geminiModel");
    return s;
}

void PlugsalotProcessor::saveProviderSettings(const ProviderSettings& s)
{
    const bool claude = s.provider == ProviderSettings::Provider::Claude;
    settingsFile->setValue("provider", claude ? "claude" : "gemini");
    settingsFile->setValue(claude ? "claudeKey" : "geminiKey", s.apiKey.trim());
    settingsFile->setValue(claude ? "claudeModel" : "geminiModel", s.model.trim());
    settingsFile->saveIfNeeded();
}

void PlugsalotProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto* o = new juce::DynamicObject();
    o->setProperty("plugsalot_state", 1);
    o->setProperty("chain", chain.toVar());
    o->setProperty("context", ctx.toVar());
    juce::Array<juce::var> msgs;
    const size_t start = chat.size() > 40 ? chat.size() - 40 : 0;
    for (size_t i = start; i < chat.size(); ++i)
    {
        auto* m = new juce::DynamicObject();
        m->setProperty("from", (int) chat[i].from);
        m->setProperty("text", chat[i].text);
        msgs.add(juce::var(m));
    }
    o->setProperty("chat", msgs);
    const auto json = juce::JSON::toString(juce::var(o), true);
    dest.replaceAll(json.toRawUTF8(), json.getNumBytesAsUTF8());
}

void PlugsalotProcessor::setStateInformation(const void* data, int size)
{
    auto v = juce::JSON::parse(juce::String::fromUTF8(static_cast<const char*>(data), size));
    if (! v.hasProperty("plugsalot_state"))
        return;
    ctx = SessionContext::fromVar(v.getProperty("context", {}));
    chat.clear();
    if (auto* msgs = v.getProperty("chat", {}).getArray())
        for (auto& m : *msgs)
            chat.push_back({ (ChatMessage::From) (int) m.getProperty("from", 2), m.getProperty("text", {}).toString() });

    auto saved = ChainState::fromVar(v.getProperty("chain", {}));
    if (! saved.isEmpty())
    {
        auto r = validator.validate(saved, ctx.cpuMode);
        if (r.ok())
            applyChain(r.chain, false);
        else
            addChat(ChatMessage::From::System, "The saved chain no longer matches this version of Plugsalot and was not loaded:\n"
                                                   + r.errors.joinIntoString("\n"));
    }
    sendChangeMessage();
}

juce::AudioProcessorEditor* PlugsalotProcessor::createEditor()
{
    return new PlugsalotEditor(*this);
}

} // namespace plugsalot

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new plugsalot::PlugsalotProcessor();
}
