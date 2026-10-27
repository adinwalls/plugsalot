// Plugsalot - GPL-3.0-or-later
// x42-dpl Digital Peak Limiter: Robin Gareus' Peaklim (GPL-3.0-or-later).
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "peaklim.h"

namespace plugsalot
{

class LimiterModule final : public Module
{
public:
    using Module::Module;

    void prepare(double sampleRate, int maxBlockSize) override
    {
        scratch.prepare(maxBlockSize);
        if (initialised)
            lim.fini();
        lim.init((float) sampleRate, 2);
        initialised = true;
        lim.set_inpgain(gain);
        lim.set_threshold(threshold);
        lim.set_release(release);
        lim.set_truepeak(truePeak);
    }

    void reset() override {}

    void process(float* const* ch, int n) noexcept override
    {
        if (! initialised) return;
        auto* in = scratch.copyFrom(ch, n);
        float* ins[2] = { const_cast<float*>(in[0]), const_cast<float*>(in[1]) };
        float* outs[2] = { ch[0], ch[1] };
        lim.process(n, ins, outs);
    }

    int resolve(const juce::String& id) const override { return indexIn(names, 4, id); }

    void setNative(int index, float v) noexcept override
    {
        // Values are stored first so prepare() can apply them; Peaklim's setters only store too.
        switch (index)
        {
            case 0: gain = v;           if (initialised) lim.set_inpgain(v);   break;
            case 1: threshold = v;      if (initialised) lim.set_threshold(v); break;
            case 2: release = v;        if (initialised) lim.set_release(v);   break;
            case 3: truePeak = v > 0.5f; if (initialised) lim.set_truepeak(truePeak); break;
            default: break; // "enable" is driven by Plugsalot's own bypass
        }
    }

    int latencySamples() const noexcept override
    {
        return initialised ? lim.get_latency() : 0;
    }

private:
    static constexpr const char* names[] = { "gain", "threshold", "release", "truepeak" };

    DPLLV2::Peaklim lim;
    StereoScratch scratch;
    bool initialised = false;
    float gain = 0.0f, threshold = -1.0f, release = 0.01f;
    bool truePeak = true;
};

std::unique_ptr<Module> createLimiterModule(const ModuleSpec& s) { return std::make_unique<LimiterModule>(s); }

} // namespace plugsalot
