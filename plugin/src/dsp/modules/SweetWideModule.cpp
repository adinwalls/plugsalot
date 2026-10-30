// Plugsalot - GPL-3.0-or-later
// Airwindows SweetWide (MIT), compiled unmodified against the VST2 shim.
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "SweetWide.h"

namespace plugsalot
{

class SweetWideModule final : public Module
{
public:
    explicit SweetWideModule(const ModuleSpec& s) : Module(s), fx(nullptr) {}

    void prepare(double sampleRate, int maxBlockSize) override
    {
        scratch.prepare(maxBlockSize);
        fx.plugsalotSetSampleRate((float) sampleRate);
    }

    void reset() override {}

    void process(float* const* ch, int n) noexcept override
    {
        auto* in = scratch.copyFrom(ch, n);
        float* ins[2] = { const_cast<float*>(in[0]), const_cast<float*>(in[1]) };
        float* outs[2] = { ch[0], ch[1] };
        fx.processReplacing(ins, outs, n);
    }

    // Airwindows parameters are lettered A, B, C ...
    int resolve(const juce::String& id) const override
    {
        if (id.length() != 1) return -1;
        const int index = id[0] - 'A';
        return index >= 0 && index < kNumParameters ? index : -1;
    }

    void setNative(int index, float v) noexcept override
    {
        if (index >= 0 && index < kNumParameters)
            fx.setParameter(index, v);
    }

private:
    SweetWide fx;
    StereoScratch scratch;
};

std::unique_ptr<Module> createSweetWideModule(const ModuleSpec& s) { return std::make_unique<SweetWideModule>(s); }

} // namespace plugsalot
