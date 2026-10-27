// Plugsalot - GPL-3.0-or-later
// A processing module: one upstream DSP core behind a common interface.
#pragma once

#include "core/Catalogue.h"
#include <atomic>
#include <memory>

namespace plugsalot
{

class Module
{
public:
    explicit Module(const ModuleSpec& s) : spec(s) {}
    virtual ~Module() = default;

    // Setup thread only (may allocate). Called before the module goes live.
    virtual void prepare(double sampleRate, int maxBlockSize) = 0;
    virtual void reset() = 0;

    // Audio thread. Always stereo in place: channels[0], channels[1].
    virtual void process(float* const* channels, int numSamples) noexcept = 0;

    // Message thread: map an upstream parameter id to this module's index (-1 if unknown).
    virtual int resolve(const juce::String& upstreamId) const = 0;

    // Audio thread safe (no allocation): set one upstream parameter by resolved index.
    virtual void setNative(int index, float nativeValue) noexcept = 0;

    void setNativeById(const juce::String& upstreamId, float nativeValue)
    {
        const int i = resolve(upstreamId);
        if (i >= 0)
            setNative(i, nativeValue);
    }

    virtual int latencySamples() const noexcept { return 0; }

    // Push every upstream default (including parameters hidden from the AI),
    // the way each upstream plugin shell does on start-up.
    void applyUpstreamDefaults()
    {
        for (auto& p : spec.rawParams)
            if (! p.defaultValue.isVoid())
                setNativeById(p.upstreamId, p.toNative(p.defaultValue));
    }

    // Set an AI-facing parameter by its catalogue id.
    void setParam(const juce::String& paramId, const juce::var& value)
    {
        if (auto* p = spec.findParam(paramId))
            setNativeById(p->upstreamId, p->toNative(value));
    }

    const ModuleSpec& spec;
    std::atomic<bool> bypassed { false };

protected:
    static int indexIn(const char* const* names, int count, const juce::String& id)
    {
        for (int i = 0; i < count; ++i)
            if (id == names[i])
                return i;
        return -1;
    }
};

std::unique_ptr<Module> createModule(const ModuleSpec& spec);

} // namespace plugsalot
