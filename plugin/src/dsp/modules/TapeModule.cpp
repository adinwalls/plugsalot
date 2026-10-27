// Plugsalot - GPL-3.0-or-later
// Dusk Audio TapeMachine 2: the framework-free TapeMachineDSP core (GPL-3.0-or-later).
// The dispatch mirrors the upstream DAF shell (TapeMachinePlugin.cpp, applyToDsp).
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "TapeMachineDSP.hpp"

namespace plugsalot
{

class TapeModule final : public Module
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
        const int iv = juce::roundToInt(v);
        const bool b = v > 0.5f;
        switch (index)
        {
            case 0:  dsp.setTapeMachine(iv); break;
            case 1:  dsp.setTapeSpeed(iv); break;
            case 2:  dsp.setTapeType(iv); break;
            case 3:  dsp.setSignalPath(iv); break;
            case 4:  dsp.setEqStandard(iv); break;
            case 5:  dsp.setInputGainDb(v); break;
            case 6:  dsp.setBias(v); break;
            case 7:  dsp.setCalibration(iv); break;
            case 8:  dsp.setAutoCal(b); break;
            case 9:  dsp.setHighpassHz(v); break;
            case 10: dsp.setLowpassHz(v); break;
            case 11: dsp.setNoiseAmount(v); break;
            case 12: dsp.setNoiseEnabled(b); break;
            case 13: dsp.setWow(v); break;
            case 14: dsp.setFlutter(v); break;
            case 15: dsp.setOutputGainDb(v); break;
            case 16: dsp.setAutoComp(b); break;
            case 17: dsp.setOversampling(iv); break;
            case 18: dsp.setHeadWidth(iv); break;
            case 19: dsp.setCrosstalk(b); break;
            case 20: dsp.setWowFlutterEnabled(b); break;
            case 21: dsp.setTransformer(b); break;
            case 22: dsp.setReproLf(v); break;
            case 23: dsp.setReproLmf(v); break;
            case 24: dsp.setReproHmf(v); break;
            case 25: dsp.setReproHf(v); break;
            case 26: dsp.setLevelHmfTrim(v); break;
            case 27: dsp.setLevelHfTrim(v); break;
            case 28: dsp.setLpQ(v); break;
            case 29: dsp.setProgHmfTrim(v); break;
            case 30: dsp.setProgHfTrim(v); break;
            case 31: dsp.setProgLfTrim(v); break;
            case 32: dsp.setReproSubBell(v); break;
            case 33: dsp.setBypass(b); break;
            default: break;
        }
    }

    int latencySamples() const noexcept override { return dsp.latencySamples(); }

private:
    static constexpr const char* names[] = {
        "tapeMachine", "tapeSpeed", "tapeType", "signalPath", "eqStandard", "inputGain", "bias",
        "calibration", "autoCal", "highpassFreq", "lowpassFreq", "noiseAmount", "noiseEnabled",
        "wowAmount", "flutterAmount", "outputGain", "autoComp", "oversampling", "headWidth",
        "crosstalk", "wowFlutterOn", "transformer", "reproLF", "reproLMF", "reproHMF", "reproHF",
        "levelHmfTrim", "levelHfTrim", "lpQ", "progHmfTrim", "progHfTrim", "progLfTrim",
        "reproSubBell", "bypass"
    };
    static constexpr int numNames = (int) (sizeof(names) / sizeof(names[0]));

    duskaudio::TapeMachineDSP dsp;
    StereoScratch scratch;
};

std::unique_ptr<Module> createTapeModule(const ModuleSpec& s) { return std::make_unique<TapeModule>(s); }

} // namespace plugsalot
