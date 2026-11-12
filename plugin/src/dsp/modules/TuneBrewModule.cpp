// Plugsalot - GPL-3.0-or-later
// TuneBrew Autotune by Adin Walls (GPL-3.0): the header-only TuneEngine, compiled unmodified.
// The parameter mapping mirrors the upstream plug-in shell (plugins/TuneBrew/TuneBrewPlugin.cpp, applyParams).
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "TuneEngine.hpp"

namespace plugsalot
{

class TuneBrewModule final : public Module
{
public:
    using Module::Module;

    void prepare(double sampleRate, int maxBlockSize) override
    {
        scratch.prepare(maxBlockSize);
        engine.init(sampleRate, 2);
    }

    void reset() override { engine.reset(); }

    void process(float* const* ch, int n) noexcept override
    {
        auto* in = scratch.copyFrom(ch, n);
        engine.process(in, ch, (uint32_t) n);
    }

    int latencySamples() const noexcept override { return engine.getLatency(); }

    int resolve(const juce::String& id) const override { return indexIn(names, numNames, id); }

    void setNative(int index, float v) noexcept override
    {
        switch (index)
        {
            case 0:  engine.key = juce::jlimit(0, 11, juce::roundToInt(v)); break;
            case 1:  engine.scale = juce::jlimit(0, tunebrew::kScaleCount - 1, juce::roundToInt(v)); break;
            case 2:  engine.mode = v > 0.5f ? 1 : 0; break;
            case 3:  engine.retuneMs = v; break;
            case 4:  engine.amount = v * 0.01; break;
            case 5:  engine.vibrato = v * 0.01; break;
            case 6:  engine.flex = v * 0.01; break;
            case 7:  engine.humanize = v * 0.01; break;
            case 8:  engine.transitionMs = v; break;
            case 9:  engine.transpose = juce::roundToInt(v); break;
            case 10: engine.refHz = v; break;
            case 11: engine.mix = v * 0.01; break;
            default: break;
        }
    }

private:
    static constexpr int numNames = 12;
    static constexpr const char* names[numNames] = { "key", "scale", "mode", "retune", "amount", "vibrato", "flex",
                                                     "humanize", "transition", "transpose", "ref_hz", "mix" };

    tunebrew::TuneEngine engine;
    StereoScratch scratch;
};

std::unique_ptr<Module> createTuneBrewModule(const ModuleSpec& s) { return std::make_unique<TuneBrewModule>(s); }

} // namespace plugsalot
