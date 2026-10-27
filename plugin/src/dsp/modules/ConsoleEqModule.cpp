// Plugsalot - GPL-3.0-or-later
// Dusk Audio 4K EQ 2: the framework-free FourKEQDSP core (GPL-3.0-or-later).
// Band and filter frequencies go through the core's Hz setters.
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "FourKEQDSP.hpp"

namespace plugsalot
{

class ConsoleEqModule final : public Module
{
public:
    using Module::Module;

    void prepare(double sampleRate, int maxBlockSize) override
    {
        scratch.prepare(maxBlockSize);
        dsp.prepare(sampleRate, maxBlockSize);
    }

    void reset() override { dsp.reset(); }

    void process(float* const* ch, int n) noexcept override
    {
        auto* in = scratch.copyFrom(ch, n);
        dsp.processBlock(in, ch, 2, n);
    }

    int resolve(const juce::String& id) const override { return indexIn(names, numNames, id); }

    void setNative(int index, float v) noexcept override
    {
        const bool b = v > 0.5f;
        switch (index)
        {
            case 0:  dsp.setHpfEnabled(b); break;
            case 1:  dsp.setLpfEnabled(b); break;
            case 2:  dsp.setHpfFreqHz(v); break;
            case 3:  dsp.setLpfFreqHz(v); break;
            case 4:  dsp.setLfGain(v); break;
            case 5:  dsp.setLfFreqHz(v); break;
            case 6:  dsp.setLfBell(b); break;
            case 7:  dsp.setLmGain(v); break;
            case 8:  dsp.setLmFreqHz(v); break;
            case 9:  dsp.setLmQ(v); break;
            case 10: dsp.setHmGain(v); break;
            case 11: dsp.setHmFreqHz(v); break;
            case 12: dsp.setHmQ(v); break;
            case 13: dsp.setHfGain(v); break;
            case 14: dsp.setHfFreqHz(v); break;
            case 15: dsp.setHfBell(b); break;
            case 16: dsp.setEqType(juce::roundToInt(v)); break;
            case 17: dsp.setBypass(b); break;
            case 18: dsp.setInputGainDb(v); break;
            case 19: dsp.setOutputGainDb(v); break;
            case 20: dsp.setOversampling(juce::roundToInt(v)); break;
            case 21: dsp.setMsMode(b); break;
            case 22: dsp.setAutoGain(b); break;
            default: break;
        }
    }

    int latencySamples() const noexcept override { return dsp.getLatencySamples(); }

private:
    static constexpr const char* names[] = {
        "hpf_enabled", "lpf_enabled", "hpf_hz", "lpf_hz", "lf_gain", "lf_hz", "lf_bell",
        "lm_gain", "lm_hz", "lm_q", "hm_gain", "hm_hz", "hm_q", "hf_gain", "hf_hz", "hf_bell",
        "eq_type", "bypass", "input_gain", "output_gain", "oversampling", "ms_mode", "auto_gain"
    };
    static constexpr int numNames = (int) (sizeof(names) / sizeof(names[0]));

    duskaudio::FourKEQDSP dsp;
    StereoScratch scratch;
};

std::unique_ptr<Module> createConsoleEqModule(const ModuleSpec& s) { return std::make_unique<ConsoleEqModule>(s); }

} // namespace plugsalot
