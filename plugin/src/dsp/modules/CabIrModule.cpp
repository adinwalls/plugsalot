// Plugsalot - GPL-3.0-or-later
// Cabinet IR: AudioDSPTools' ImpulseResponse convolution (Steven Atkinson, MIT)
// with low and high cut filters from the same library.
#include "dsp/Module.h"
#include "dsp/ImpulseResponse.h"
#include "dsp/RecursiveLinearFilter.h"
#include <cmath>
#include <vector>

namespace plugsalot
{

class CabIrModule final : public Module
{
public:
    using Module::Module;

    ~CabIrModule() override
    {
        delete pending.exchange(nullptr);
        delete retired.exchange(nullptr);
        delete active;
    }

    void prepare(double sr, int block) override
    {
        const bool changed = sr != sampleRate;
        sampleRate = sr;
        maxBlock = juce::jmax(1, block);
        for (int c = 0; c < 2; ++c)
        {
            buf[c].assign((size_t) maxBlock, 0.0);
            ptr[c] = buf[c].data();
            lowIn[c].assign((size_t) maxBlock, 0.0);
        }
        updateFilters();
        // The response is resampled for a given host rate, so rebuild it if the rate changed.
        if (changed && active != nullptr && loadedPath.isNotEmpty())
            rebuildFromPath();
        else if (active != nullptr)
            active->ir.Reset((size_t) maxBlock, 2);
    }

    void reset() override {}

    juce::String loadFile(const juce::File& file) override
    {
        delete retired.exchange(nullptr);
        loadedPath = file.getFullPathName();
        if (! file.existsAsFile())
        {
            delete pending.exchange(new State{});
            return file == juce::File() ? juce::String() : "Impulse response not found: " + file.getFileName();
        }
        auto st = std::make_unique<State>(file.getFullPathName().toRawUTF8(), sampleRate);
        if (st->ir.GetWavState() != dsp::wav::LoadReturnCode::SUCCESS)
            return "Could not read " + file.getFileName() + ": " + juce::String(dsp::wav::GetMsgForLoadReturnCode(st->ir.GetWavState()));
        st->ir.Reset((size_t) maxBlock, 2);
        st->valid = true;
        delete pending.exchange(st.release());
        return {};
    }

    void process(float* const* ch, int numSamples) noexcept override
    {
        if (auto* p = pending.exchange(nullptr, std::memory_order_acq_rel))
        {
            delete retired.exchange(active, std::memory_order_acq_rel);
            active = p;
        }
        if (active == nullptr || ! active->valid || buf[0].empty())
            return; // no impulse response loaded: pass through

        const double g = std::pow(10.0, levelDb / 20.0);
        for (int start = 0; start < numSamples; start += maxBlock)
        {
            const int n = juce::jmin(maxBlock, numSamples - start);
            for (int i = 0; i < n; ++i)
                buf[0][(size_t) i] = 0.5 * ((double) ch[0][start + i] + (double) ch[1][start + i]);

            double** o = active->ir.Process(ptr, 1, (size_t) n);
            const bool stereoIr = active->ir.GetNumIRChannels() > 1;
            double* l = o[0];
            double* r = stereoIr ? o[1] : o[0];

            // Filters run on the left/right results; a mono response feeds both equally.
            for (int i = 0; i < n; ++i)
            {
                lowIn[0][(size_t) i] = l[i];
                lowIn[1][(size_t) i] = r[i];
            }
            double* lp[2] = { lowIn[0].data(), lowIn[1].data() };
            double** f = hp.Process(lp, 2, (size_t) n);
            f = lpf.Process(f, 2, (size_t) n);
            for (int i = 0; i < n; ++i)
            {
                ch[0][start + i] = (float) (f[0][i] * g);
                ch[1][start + i] = (float) (f[1][i] * g);
            }
        }
    }

    int resolve(const juce::String& id) const override { return indexIn(names, 3, id); }

    void setNative(int index, float v) noexcept override
    {
        switch (index)
        {
            case 0: lowCut = v;  updateFilters(); break;
            case 1: highCut = v; updateFilters(); break;
            case 2: levelDb = v; break;
            default: break;
        }
    }

private:
    struct State
    {
        State() : ir(dsp::ImpulseResponse::IRData{ {}, 48000.0, {} }, 48000.0) {}
        State(const char* path, double sr) : ir(path, sr) {}
        dsp::ImpulseResponse ir;
        bool valid = false;
    };

    void rebuildFromPath()
    {
        // Message thread, audio stopped: swap synchronously.
        auto st = std::make_unique<State>(loadedPath.toRawUTF8(), sampleRate);
        if (st->ir.GetWavState() != dsp::wav::LoadReturnCode::SUCCESS)
            return;
        st->ir.Reset((size_t) maxBlock, 2);
        st->valid = true;
        delete active;
        active = st.release();
    }

    void updateFilters() noexcept
    {
        hp.SetParams(recursive_linear_filter::HighPassParams(sampleRate, (double) lowCut));
        lpf.SetParams(recursive_linear_filter::LowPassParams(sampleRate, juce::jmin((double) highCut, sampleRate * 0.45)));
    }

    static constexpr const char* names[] = { "low_cut", "high_cut", "level" };

    double sampleRate = 48000.0;
    int maxBlock = 512;
    std::vector<double> buf[2];
    double* ptr[2] {};
    std::vector<double> lowIn[2];

    std::atomic<State*> pending { nullptr };
    std::atomic<State*> retired { nullptr };
    State* active = nullptr;
    juce::String loadedPath;

    recursive_linear_filter::HighPass hp;
    recursive_linear_filter::LowPass lpf;
    float lowCut = 80.0f, highCut = 12000.0f, levelDb = 0.0f;
};

std::unique_ptr<Module> createCabIrModule(const ModuleSpec& s) { return std::make_unique<CabIrModule>(s); }

} // namespace plugsalot
