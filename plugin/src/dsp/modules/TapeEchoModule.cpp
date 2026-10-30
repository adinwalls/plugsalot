// Plugsalot - GPL-3.0-or-later
// Dusk Audio Tape Echo 2: the framework-free TapeEchoDSP core (GPL-3.0-or-later), a Roland Space
// Echo style three-head tape delay with spring reverb. The dispatch mirrors the upstream DAF shell
// (TapeEchoPlugin.cpp, applyToDsp); host tempo sync is not used (Plugsalot has no tempo input).
#include "dsp/Module.h"
#include "TapeEchoDSP.hpp"

namespace plugsalot
{

class TapeEchoModule final : public Module
{
public:
    using Module::Module;

    void prepare(double sampleRate, int maxBlockSize) override
    {
        dsp.prepare(sampleRate, maxBlockSize);
        // Controls that Plugsalot does not expose stay at the upstream plugin's defaults.
        dsp.setDryLevel(1.0f);          // Mix does the dry/wet balance
        dsp.setEchoPan(0.5f);
        dsp.setReverbPan(0.5f);
        dsp.setInputSend(true);
        dsp.setOutputVolume(0.5f);      // 0 dB
        dsp.setNoise(0.0f);
        dsp.setHumFrequency50(false);
        dsp.setBypass(false);
    }

    void reset() override { dsp.reset(); }

    void process(float* const* ch, int n) noexcept override
    {
        dsp.processBlock(reinterpret_cast<const float* const*>(ch), ch, 2, n);   // in place is supported
    }

    int resolve(const juce::String& id) const override { return indexIn(names, 11, id); }

    void setNative(int index, float v) noexcept override
    {
        const float p = v * 0.01f;   // percentages become 0..1
        switch (index)
        {
            case 0:  dsp.setMode(juce::roundToInt(v) + 1); break;   // option index 0..11 -> mode 1..12
            case 1:  dsp.setRepeatRate(p); break;
            case 2:  dsp.setIntensity(p); break;
            case 3:  dsp.setEchoLevel(p); break;
            case 4:  dsp.setReverbLevel(p); break;
            case 5:  dsp.setBass(p); break;       // -100..100 %
            case 6:  dsp.setTreble(p); break;
            case 7:  dsp.setInputGain(p); break;
            case 8:  dsp.setWowFlutter(p); break;
            case 9:  dsp.setTapeAge(juce::roundToInt(v) * 0.5f); break;   // New / Used / Old = 0 / 0.5 / 1
            case 10: dsp.setMix(p); break;
            default: break;
        }
    }

private:
    static constexpr const char* names[] = {
        "mode", "repeat_rate", "intensity", "echo_level", "reverb_level", "bass", "treble",
        "input_volume", "wow_flutter", "tape_age", "mix" };

    duskaudio::TapeEchoDSP dsp;
};

std::unique_ptr<Module> createTapeEchoModule(const ModuleSpec& s) { return std::make_unique<TapeEchoModule>(s); }

} // namespace plugsalot
