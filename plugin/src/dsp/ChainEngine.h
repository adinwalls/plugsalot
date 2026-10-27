// Plugsalot - GPL-3.0-or-later
// Runs the live chain on the audio thread and swaps in new chains from the
// message thread without locks or allocation on the audio thread.
//
// Threading model
// - Message thread builds a LiveChain (creating and preparing any new modules,
//   reusing module instances whose slot identity is unchanged so reverb tails and
//   compressor state survive small edits), then publishes it through `next`.
// - Audio thread adopts `next` at the start of a block and records the generation.
// - Parameter changes for live modules travel through a lock-free FIFO and are
//   applied by the audio thread. Old chains are freed on the message thread only
//   after the audio thread has adopted a newer one.
#pragma once

#include "Module.h"
#include "core/ChainState.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <memory>
#include <vector>

namespace plugsalot
{

class ChainEngine
{
public:
    ChainEngine();
    ~ChainEngine();

    // Message thread (host calls prepareToPlay with audio stopped).
    void prepare(double sampleRate, int maxBlockSize);

    // Message thread: make `chain` live. The chain must already be validated.
    void setChain(const ChainState& chain);

    // Message thread: change one AI-facing parameter / bypass on a live slot.
    void setParam(int slotIndex, const juce::String& paramId, const juce::var& value);
    void setBypass(int slotIndex, bool bypass);

    // Message thread: free retired chains. Call from a timer.
    void collectGarbage();

    // Audio thread.
    void process(juce::AudioBuffer<float>& buffer) noexcept;

    // Any thread.
    int latencySamples() const noexcept { return latency.load(std::memory_order_relaxed); }
    int numModules() const noexcept { return liveCount.load(std::memory_order_relaxed); }

private:
    struct LiveChain
    {
        std::vector<std::shared_ptr<Module>> modules;
        std::vector<juce::Uuid> ids;
        uint64_t generation = 0;
    };

    struct ParamChange
    {
        Module* module = nullptr;
        int index = -1;
        float value = 0.0f;
        int bypass = -1; // -1: parameter change, 0/1: bypass change
    };

    void push(const ParamChange& c);
    void updateLatency(const LiveChain& c);

    double sampleRate = 44100.0;
    int maxBlock = 512;

    // Owned by the message thread; the audio thread only reads `current`.
    std::vector<std::unique_ptr<LiveChain>> chains;
    LiveChain* publishedOnMessageThread = nullptr;
    uint64_t nextGeneration = 1;

    std::atomic<LiveChain*> next { nullptr };
    LiveChain* current = nullptr;                 // audio thread only
    std::atomic<uint64_t> adoptedGeneration { 0 };

    juce::AbstractFifo fifo { 512 };
    std::vector<ParamChange> fifoData = std::vector<ParamChange>(512);

    std::atomic<int> latency { 0 };
    std::atomic<int> liveCount { 0 };
};

} // namespace plugsalot
