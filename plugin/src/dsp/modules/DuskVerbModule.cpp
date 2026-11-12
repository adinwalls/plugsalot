// Plugsalot - GPL-3.0-or-later
// Dusk Audio DuskVerb: the framework-free DuskVerbDSP core (GPL-3.0-or-later), compiled unmodified.
// DuskVerb's sounds are its factory presets: each one selects an engine (plate, hall, chamber, room,
// spring, shimmer ...) and about ninety voicing values, plus engine configuration keyed on the preset.
// Plugsalot therefore exposes the preset as the main choice and layers a few macro controls on top,
// the same way the upstream shell does (DuskVerbPlugin.cpp: write the preset's parameters, then
// applyFactoryPresetConfig).
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "DuskVerbDSP.hpp"

namespace plugsalot
{

class DuskVerbModule final : public Module
{
public:
    explicit DuskVerbModule(const ModuleSpec& s) : Module(s)
    {
        for (int i = 0; i < duskverb::kNumParams; ++i)
            plain[(size_t) i] = duskverb::paramDesc(i).def;
        loadPreset();
    }

    void prepare(double sampleRate, int maxBlockSize) override
    {
        scratch.prepare(maxBlockSize);
        push();
        dsp.prepare(sampleRate, maxBlockSize);
    }

    void reset() override {}

    void process(float* const* ch, int n) noexcept override
    {
        auto* in = scratch.copyFrom(ch, n);
        dsp.processBlock(in, ch, 2, n);
    }

    int resolve(const juce::String& id) const override { return indexIn(names, numNames, id); }

    void setNative(int index, float v) noexcept override
    {
        switch (index)
        {
            case 0:
            {
                const int count = (int) getFactoryPresets().size();
                const int next = juce::jlimit(0, count - 1, juce::roundToInt(v));
                if (next != presetIndex) { presetIndex = next; loadPreset(); }
                break;
            }
            case 1: mixPercent = v; break;
            case 2: decayScale = v; break;
            case 3: tone = v; break;
            case 4: character = v; break;
            case 5: duck = v; break;
            default: return;
        }
        push();
    }

private:
    static int indexOfId(const char* id) noexcept
    {
        for (int i = 0; i < duskverb::kNumParams; ++i)
            if (std::strcmp(duskverb::paramDesc(i).id, id) == 0)
                return i;
        return -1;
    }

    static float toDsp(int index, float plainValue) noexcept
    {
        const auto& d = duskverb::paramDesc(index);
        return duskverb::dspFromHost(d, duskverb::plainToHost(d, plainValue));
    }

    // Write the preset's own values into the parameter snapshot and arm its engine configuration.
    void loadPreset() noexcept
    {
        const auto& preset = getFactoryPresets()[(size_t) presetIndex];
        preset.collectParameters([this](const char* id, float value)
        {
            const int i = indexOfId(id);
            if (i >= 0) plain[(size_t) i] = value;
        });
        presetDecay = plain[(size_t) duskverb::Decay];
        presetDirty = true;
    }

    // Layer the macro controls over the preset and hand everything to the DSP.
    void push() noexcept
    {
        auto values = plain;
        values[(size_t) duskverb::BusMode] = 0.0f;   // always an insert here: Mix sets the wet amount
        values[(size_t) duskverb::Bypass] = 0.0f;
        values[(size_t) duskverb::Freeze] = 0.0f;
        values[(size_t) duskverb::PredelaySync] = 0.0f;   // Plugsalot does not pass host tempo
        values[(size_t) duskverb::Mix] = juce::jlimit(0.0f, 1.0f, mixPercent * 0.01f);
        values[(size_t) duskverb::Decay] = juce::jlimit(0.2f, 30.0f, presetDecay * decayScale);
        values[(size_t) duskverb::Tone] = tone;
        values[(size_t) duskverb::Character] = character;
        values[(size_t) duskverb::Duck] = duck;
        for (int i = 0; i < duskverb::kNumParams; ++i)
            dsp.setParameter(i, toDsp(i, values[(size_t) i]));
        if (presetDirty)
        {
            dsp.applyFactoryPresetConfig(getFactoryPresets()[(size_t) presetIndex]);
            presetDirty = false;
        }
    }

    static constexpr int numNames = 6;
    static constexpr const char* names[numNames] = { "preset", "mix", "decay_scale", "tone", "character", "duck" };

    duskverb::DuskVerbDSP dsp;
    StereoScratch scratch;
    std::array<float, duskverb::kNumParams> plain {};
    int presetIndex = 0;
    bool presetDirty = true;
    float presetDecay = 1.0f, mixPercent = 20.0f, decayScale = 1.0f, tone = 0.0f, character = 0.0f, duck = 0.0f;
};

std::unique_ptr<Module> createDuskVerbModule(const ModuleSpec& s) { return std::make_unique<DuskVerbModule>(s); }

} // namespace plugsalot
