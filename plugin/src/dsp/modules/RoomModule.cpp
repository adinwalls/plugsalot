// Plugsalot - GPL-3.0-or-later
// Dragonfly Room Reverb: Michael Willis' DSP with freeverb3 (GPL-3.0 / GPL-2.0-or-later).
// Parameter indices follow the upstream PARAMS[] order, which is the raw extraction order.
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "DragonflyBridge.h"

namespace plugsalot
{

class RoomModule final : public Module
{
    static constexpr int kNumParams = 17; // Dragonfly Room PARAMS[] size, checked in the constructor

public:
    explicit RoomModule(const ModuleSpec& s) : Module(s)
    {
        for (auto& p : spec.rawParams)
            ids.add(p.upstreamId);
        for (auto& v : pending)
            v = notSet;
        jassert(DragonflyRoom::numParams() == kNumParams);
    }

    void prepare(double sampleRate, int maxBlockSize) override
    {
        scratch.prepare(maxBlockSize);
        dsp = std::make_unique<DragonflyRoom>(sampleRate);
        for (int i = 0; i < kNumParams; ++i)
            if (pending[i] != notSet)
                dsp->setParameterValue(i, pending[i]);
    }

    void reset() override { if (dsp) dsp->mute(); }

    void process(float* const* ch, int n) noexcept override
    {
        if (! dsp) return;
        auto* in = scratch.copyFrom(ch, n);
        const float* ins[2] = { in[0], in[1] };
        float* outs[2] = { ch[0], ch[1] };
        dsp->run(ins, outs, (uint32_t) n);
    }

    int resolve(const juce::String& id) const override
    {
        const int i = ids.indexOf(id);
        return i < kNumParams ? i : -1;
    }

    void setNative(int index, float v) noexcept override
    {
        if (index < 0 || index >= kNumParams) return;
        pending[index] = v;  // kept so prepare() can restore after a sample-rate change
        if (dsp) dsp->setParameterValue(index, v);
    }

private:
    static constexpr float notSet = -1.0e9f;
    std::unique_ptr<DragonflyRoom> dsp;
    StereoScratch scratch;
    juce::StringArray ids;
    float pending[kNumParams];
};

std::unique_ptr<Module> createRoomModule(const ModuleSpec& s) { return std::make_unique<RoomModule>(s); }

} // namespace plugsalot
