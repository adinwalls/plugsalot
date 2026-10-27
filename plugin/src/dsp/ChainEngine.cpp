// Plugsalot - GPL-3.0-or-later
#include "ChainEngine.h"

namespace plugsalot
{

ChainEngine::ChainEngine() = default;
ChainEngine::~ChainEngine() = default;

void ChainEngine::prepare(double sr, int block)
{
    sampleRate = sr;
    maxBlock = juce::jmax(1, block);
    // Audio is stopped while the host prepares, so live modules can be re-prepared.
    if (publishedOnMessageThread != nullptr)
        for (auto& m : publishedOnMessageThread->modules)
        {
            m->prepare(sampleRate, maxBlock);
            m->reset();
        }
    if (publishedOnMessageThread != nullptr)
        updateLatency(*publishedOnMessageThread);
}

void ChainEngine::setChain(const ChainState& chain)
{
    auto fresh = std::make_unique<LiveChain>();
    fresh->generation = nextGeneration++;

    for (auto& slot : chain.slots)
    {
        auto* spec = Catalogue::embedded().find(slot.moduleId);
        if (spec == nullptr)
            continue;

        std::shared_ptr<Module> module;
        bool reused = false;
        if (publishedOnMessageThread != nullptr && ! slot.instanceId.isNull())
            for (size_t i = 0; i < publishedOnMessageThread->ids.size(); ++i)
                if (publishedOnMessageThread->ids[i] == slot.instanceId
                    && publishedOnMessageThread->modules[i]->spec.id == slot.moduleId)
                {
                    module = publishedOnMessageThread->modules[i];
                    reused = true;
                }

        if (module == nullptr)
        {
            module = std::shared_ptr<Module>(createModule(*spec).release());
            if (module == nullptr)
                continue;
        }

        // Every exposed parameter is set explicitly: values the chain names, and the
        // catalogue default for the rest, so a reused module never keeps stale settings.
        for (auto& p : spec->params)
        {
            const auto value = slot.params.contains(p.id) ? slot.params[p.id] : p.defaultValue;
            if (value.isVoid())
                continue;
            if (reused)
            {
                ParamChange c;
                c.module = module.get();
                c.index = module->resolve(p.upstreamId);
                c.value = p.toNative(value);
                push(c);
            }
            else
                module->setNativeById(p.upstreamId, p.toNative(value));
        }

        if (reused)
        {
            ParamChange b;
            b.module = module.get();
            b.bypass = slot.bypass ? 1 : 0;
            push(b);
        }
        else
        {
            module->bypassed.store(slot.bypass);
            module->prepare(sampleRate, maxBlock);
            module->reset();
        }

        fresh->modules.push_back(std::move(module));
        fresh->ids.push_back(slot.instanceId);
    }

    updateLatency(*fresh);
    liveCount.store((int) fresh->modules.size());
    publishedOnMessageThread = fresh.get();
    next.store(fresh.get(), std::memory_order_release);
    chains.push_back(std::move(fresh));
}

void ChainEngine::setParam(int slot, const juce::String& paramId, const juce::var& value)
{
    if (publishedOnMessageThread == nullptr || slot < 0 || slot >= (int) publishedOnMessageThread->modules.size())
        return;
    auto& module = publishedOnMessageThread->modules[(size_t) slot];
    if (auto* p = module->spec.findParam(paramId))
    {
        ParamChange c;
        c.module = module.get();
        c.index = module->resolve(p->upstreamId);
        c.value = p->toNative(value);
        push(c);
    }
}

void ChainEngine::setBypass(int slot, bool bypass)
{
    if (publishedOnMessageThread == nullptr || slot < 0 || slot >= (int) publishedOnMessageThread->modules.size())
        return;
    ParamChange c;
    c.module = publishedOnMessageThread->modules[(size_t) slot].get();
    c.bypass = bypass ? 1 : 0;
    push(c);
}

void ChainEngine::push(const ParamChange& c)
{
    const auto scope = fifo.write(1);
    if (scope.blockSize1 > 0)
        fifoData[(size_t) scope.startIndex1] = c;
    else if (scope.blockSize2 > 0)
        fifoData[(size_t) scope.startIndex2] = c;
    // A full FIFO drops the change; 512 slots is far more than one chain edit needs.
}

void ChainEngine::updateLatency(const LiveChain& c)
{
    int total = 0;
    for (auto& m : c.modules)
        if (! m->bypassed.load())
            total += m->latencySamples();
    latency.store(total);
}

void ChainEngine::collectGarbage()
{
    const auto adopted = adoptedGeneration.load(std::memory_order_acquire);
    chains.erase(std::remove_if(chains.begin(), chains.end(),
                                [&](const std::unique_ptr<LiveChain>& c)
                                { return c->generation < adopted && c.get() != publishedOnMessageThread; }),
                 chains.end());
    if (publishedOnMessageThread != nullptr)
        updateLatency(*publishedOnMessageThread);
}

void ChainEngine::process(juce::AudioBuffer<float>& buffer) noexcept
{
    // 1. Apply queued parameter changes (their modules are alive: chains are only
    //    freed after the audio thread has moved past them).
    {
        const auto scope = fifo.read(fifo.getNumReady());
        auto apply = [&](int start, int size)
        {
            for (int i = start; i < start + size; ++i)
            {
                auto& c = fifoData[(size_t) i];
                if (c.bypass >= 0)
                    c.module->bypassed.store(c.bypass == 1, std::memory_order_relaxed);
                else if (c.index >= 0)
                    c.module->setNative(c.index, c.value);
            }
        };
        apply(scope.startIndex1, scope.blockSize1);
        apply(scope.startIndex2, scope.blockSize2);
    }

    // 2. Adopt a newly published chain.
    if (auto* n = next.exchange(nullptr, std::memory_order_acq_rel))
    {
        current = n;
        adoptedGeneration.store(n->generation, std::memory_order_release);
    }

    if (current == nullptr || buffer.getNumChannels() < 2)
        return;

    float* channels[2] = { buffer.getWritePointer(0), buffer.getWritePointer(1) };
    const int numSamples = buffer.getNumSamples();
    for (auto& m : current->modules)
        if (! m->bypassed.load(std::memory_order_relaxed))
            m->process(channels, numSamples);
}

} // namespace plugsalot
