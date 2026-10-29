// Plugsalot - GPL-3.0-or-later
// NAM Amp: Neural Amp Modeler Core (Steven Atkinson, MIT) running a .nam capture,
// with AudioDSPTools' noise gate and the NAM plugin's bass/mid/treble tone stack.
// The signal order follows the NAM plugin: input level, gate, capture, tone stack,
// loudness normalisation, output level.
#include "dsp/Module.h"
#include "NAM/get_dsp.h"
#include "dsp/NoiseGate.h"
#include "NeuralAmpModeler/ToneStack.h"
#include <cmath>
#include <vector>

namespace plugsalot
{

namespace
{
struct NamState
{
    std::unique_ptr<nam::DSP> model;
    double loudnessTrimDb = 0.0; // brings the capture to -18 dB, like the NAM plugin's "normalised"
};
}

class NamAmpModule final : public Module
{
public:
    using Module::Module;

    ~NamAmpModule() override
    {
        delete pending.exchange(nullptr);
        delete retired.exchange(nullptr);
        delete active;
    }

    void prepare(double sr, int block) override
    {
        sampleRate = sr;
        maxBlock = juce::jmax(1, block);
        in.assign((size_t) maxBlock, 0.0);
        out.assign((size_t) maxBlock, 0.0);
        inPtr[0] = in.data();
        outPtr[0] = out.data();
        setupGate();
        tone.Reset(sr, maxBlock);
        dcBlock.SetParams(recursive_linear_filter::HighPassParams(sr, 5.0));
        if (active != nullptr)
            active->model->Reset(sr, maxBlock);
        if (auto* p = pending.load())
            p->model->Reset(sr, maxBlock);
    }

    void reset() override {}

    juce::String loadFile(const juce::File& file) override
    {
        delete retired.exchange(nullptr);
        if (! file.existsAsFile())
        {
            delete pending.exchange(new NamState{}); // empty state: back to pass-through
            return file == juce::File() ? juce::String() : "Capture file not found: " + file.getFileName();
        }
        try
        {
            auto st = std::make_unique<NamState>();
            st->model = nam::get_dsp(std::filesystem::path(file.getFullPathName().toStdString()));
            if (st->model == nullptr)
                return "Could not read " + file.getFileName();
            st->model->Reset(sampleRate, maxBlock);
            st->model->prewarm();
            if (st->model->HasLoudness())
                st->loudnessTrimDb = -18.0 - st->model->GetLoudness();
            juce::String warning;
            const double expected = st->model->GetExpectedSampleRate();
            if (expected > 0 && std::abs(expected - sampleRate) > 1.0 && ! st->model->SupportsArbitrarySampleRate())
                warning = file.getFileName() + " was trained at " + juce::String((int) expected)
                          + " Hz, so it may sound slightly off at " + juce::String((int) sampleRate) + " Hz.";
            delete pending.exchange(st.release());
            return warning;
        }
        catch (const std::exception& e)
        {
            return "Could not load " + file.getFileName() + ": " + juce::String(e.what());
        }
    }

    void process(float* const* ch, int numSamples) noexcept override
    {
        if (auto* p = pending.exchange(nullptr, std::memory_order_acq_rel))
        {
            delete retired.exchange(active, std::memory_order_acq_rel); // normally null; see loadFile
            active = p;
        }
        if (active == nullptr || active->model == nullptr || in.empty())
            return; // no capture loaded: pass through

        const double inGain = std::pow(10.0, inputDb / 20.0);
        const double outGain = std::pow(10.0, (outputDb + (normalise ? active->loudnessTrimDb : 0.0)) / 20.0);
        const bool gateOn = gateDb > -99.5;

        for (int start = 0; start < numSamples; start += maxBlock)
        {
            const int n = juce::jmin(maxBlock, numSamples - start);
            for (int i = 0; i < n; ++i)
                in[(size_t) i] = 0.5 * ((double) ch[0][start + i] + (double) ch[1][start + i]) * inGain;

            if (gateOn)
                trigger.Process(inPtr, 1, (size_t) n);
            active->model->process(inPtr, outPtr, n);
            double** shaped = outPtr;
            if (gateOn)
                shaped = gain.Process(outPtr, 1, (size_t) n);
            shaped = tone.Process(shaped, 1, n);
            shaped = dcBlock.Process(shaped, 1, (size_t) n);

            for (int i = 0; i < n; ++i)
            {
                const float v = (float) (shaped[0][i] * outGain);
                ch[0][start + i] = v;
                ch[1][start + i] = v;
            }
        }
    }

    int resolve(const juce::String& id) const override { return indexIn(names, 7, id); }

    void setNative(int index, float v) noexcept override
    {
        switch (index)
        {
            case 0: inputDb = v; break;
            case 1: if (v != gateDb) { gateDb = v; setupGate(); } break;
            case 2: tone.SetParam("bass", v); break;
            case 3: tone.SetParam("middle", v); break;
            case 4: tone.SetParam("treble", v); break;
            case 5: outputDb = v; break;
            case 6: normalise = v > 0.5f; break;
            default: break;
        }
    }

private:
    void setupGate() noexcept
    {
        const dsp::noise_gate::TriggerParams params(0.01, (double) gateDb, 0.1, 0.005, 0.01, 0.05);
        trigger.SetParams(params);
        trigger.SetSampleRate(sampleRate);
        if (! gateWired)
        {
            trigger.AddListener(&gain);
            gateWired = true;
        }
    }

    static constexpr const char* names[] = { "input_gain", "gate", "bass", "mid", "treble", "output_gain", "normalise" };

    double sampleRate = 48000.0;
    int maxBlock = 512;
    std::vector<double> in, out;
    double* inPtr[1] {};
    double* outPtr[1] {};

    std::atomic<NamState*> pending { nullptr };
    std::atomic<NamState*> retired { nullptr };
    NamState* active = nullptr; // audio thread

    dsp::noise_gate::Trigger trigger;
    dsp::noise_gate::Gain gain;
    bool gateWired = false;
    dsp::tone_stack::BasicNamToneStack tone;
    recursive_linear_filter::HighPass dcBlock;

    float inputDb = 0.0f, gateDb = -80.0f, outputDb = 0.0f;
    bool normalise = true;
};

std::unique_ptr<Module> createNamAmpModule(const ModuleSpec& s) { return std::make_unique<NamAmpModule>(s); }

} // namespace plugsalot
