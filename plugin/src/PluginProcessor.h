// Plugsalot - GPL-3.0-or-later
#pragma once

#include "ai/AiSession.h"
#include "core/ChainValidator.h"
#include "dsp/ChainEngine.h"
#include "core/Tone3000.h"
#include "dsp/InputAnalyser.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace plugsalot
{

struct ChatMessage
{
    enum class From { User, Assistant, System };
    From from = From::User;
    juce::String text;
};

class PlugsalotProcessor final : public juce::AudioProcessor,
                                 public juce::ChangeBroadcaster,
                                 private juce::Timer
{
public:
    PlugsalotProcessor();
    ~PlugsalotProcessor() override;

    // AudioProcessor
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Plugsalot"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    // Message thread API used by the editor.
    void askAi(const juce::String& userText);
    bool isAiBusy() const { return ai.isBusy(); }
    void setManualParam(int slot, const juce::String& paramId, const juce::var& value);
    void setManualBypass(int slot, bool bypass);
    // Choose the capture / impulse response (library file name, empty for none) for a slot.
    void setSlotFile(int slot, const juce::String& fileName);
    void postNote(const juce::String& text); // a system line in the chat
    void removeSlot(int slot);
    void undo();
    bool canUndo() const { return ! undoStack.empty(); }
    void clearChain();

    const ChainState& getChain() const { return chain; }
    const std::vector<ChatMessage>& getChat() const { return chat; }
    SessionContext& context() { return ctx; }
    AudioAnalysis analysis() const { return analyser.snapshot(); }
    int chainCpuPoints() const;

    // Provider settings are app-wide (shared by every instance), stored in the
    // user's settings folder.
    ProviderSettings loadProviderSettings() const;                         // the active provider
    ProviderSettings loadProviderSettingsFor(ProviderSettings::Provider) const;
    void saveProviderSettings(const ProviderSettings&);
    bool hasApiKey() const { return loadProviderSettings().apiKey.isNotEmpty(); }
    juce::String lastModel() const { return lastModelUsed; }

    // One TONE3000 session per process; its tokens are kept in the app-wide settings file.
    std::shared_ptr<Tone3000> tone3000() const { return t3k; }

    std::function<void()> onNeedsApiKey; // editor shows the setup pop-up

private:
    void timerCallback() override;
    void applyChain(ChainState newChain, bool pushUndo);
    void addChat(ChatMessage::From from, const juce::String& text);

    const Catalogue& catalogue;
    ChainValidator validator;
    std::shared_ptr<Tone3000> t3k;
    std::shared_ptr<std::atomic<bool>> alive = std::make_shared<std::atomic<bool>>(true);
    ChainEngine engine;
    InputAnalyser analyser;
    AiSession ai;

    ChainState chain;
    std::vector<ChainState> undoStack;
    std::vector<ChatMessage> chat;
    SessionContext ctx;
    juce::String lastModelUsed;

    juce::AudioBuffer<float> work;
    std::atomic<bool> safetyMuted { false };
    int reportedLatency = -1;
    std::unique_ptr<juce::PropertiesFile> settingsFile;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PlugsalotProcessor)
};

} // namespace plugsalot
