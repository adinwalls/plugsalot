// Plugsalot - GPL-3.0-or-later
// Console8 channel stage: Airwindows Console8ChannelIn (or the brighter Console8ChannelHype) followed
// by Console8ChannelOut (all MIT, compiled unmodified). In the original, the In plugin starts a channel
// and the Out plugin ends it, with the rest of the processing between them. Here they run back to back
// as one console-style colour stage: the analog-style filtering of both, and the sine-shaped gain stage
// of the Out plugin, which saturates when its fader is raised above unity.
#include "dsp/Module.h"
#include "AirwindowsStage.h"
#include "StereoScratch.h"

namespace plugsalot
{

class Console8Module final : public Module
{
public:
    explicit Console8Module(const ModuleSpec& s)
        : Module(s), solid(createConsole8InStage()), hype(createConsole8HypeStage()), out(createConsole8OutStage()) {}

    void prepare(double sampleRate, int maxBlockSize) override
    {
        scratch.prepare(maxBlockSize);
        mid.setSize(2, juce::jmax(1, maxBlockSize), false, true, true);
        solid->prepare(sampleRate);
        hype->prepare(sampleRate);
        out->prepare(sampleRate);
        out->set(0, fader);
    }

    void reset() override {}

    void process(float* const* ch, int n) noexcept override
    {
        auto* in = scratch.copyFrom(ch, n);
        float* ins[2] = { const_cast<float*>(in[0]), const_cast<float*>(in[1]) };
        float* mids[2] = { mid.getWritePointer(0), mid.getWritePointer(1) };
        (useHype ? hype : solid)->process(ins, mids, n);
        float* outs[2] = { ch[0], ch[1] };
        out->process(mids, outs, n);
    }

    int resolve(const juce::String& id) const override { return indexIn(names, 2, id); }

    void setNative(int index, float v) noexcept override
    {
        if (index == 0)
            useHype = v >= 0.5f;
        else if (index == 1)
        {
            fader = 0.5f + 0.5f * juce::jlimit(0.0f, 100.0f, v) * 0.01f;   // 0 % = unity (0.5), 100 % = +6 dB per stage (1.0)
            out->set(0, fader);
        }
    }

private:
    static constexpr const char* names[] = { "tone", "drive" };

    std::unique_ptr<AwStage> solid, hype, out;
    StereoScratch scratch;
    juce::AudioBuffer<float> mid;
    bool useHype = false;
    float fader = 0.5f;
};

std::unique_ptr<Module> createConsole8Module(const ModuleSpec& s) { return std::make_unique<Console8Module>(s); }

} // namespace plugsalot
