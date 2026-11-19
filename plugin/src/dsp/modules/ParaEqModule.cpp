// Plugsalot - GPL-3.0-or-later
// LSP Parametric Equalizer (LGPL-3.0-or-later), IIR mode only: eight bands per channel, each one
// the vendored LSP Filter class with the same filter-type mapping the LSP plug-in uses
// (RLC, bilinear-transform family). FIR, FFT and spectral modes, which add latency and CPU, are not used.
#include "dsp/Module.h"
#include <lsp-plug.in/dsp-units/filters/Filter.h>
#include <algorithm>
#include <cmath>

namespace plugsalot
{

class ParaEqModule final : public Module
{
public:
    using Module::Module;

    void prepare(double sampleRate, int) override
    {
        sr = (size_t) std::lround(sampleRate);
        for (auto& ch : filters)
            for (auto& f : ch)
                f.init(nullptr);   // own filter bank; allocates, so setup thread only
        dirty.store(true);
    }

    void reset() override
    {
        for (auto& ch : filters)
            for (auto& f : ch)
                f.reset();
    }

    void process(float* const* ch, int n) noexcept override
    {
        if (dirty.exchange(false))
            rebuild();

        for (int c = 0; c < 2; ++c)
        {
            float* buf = ch[c];
            if (gIn != 1.0f)
                for (int i = 0; i < n; ++i)
                    buf[i] *= gIn;
            for (int b = 0; b < numBands; ++b)
                if (bands[b].type > 0 && bands[b].type <= 6)   // Filter::active() is false until its first rebuild
                    filters[c][b].process(buf, buf, (size_t) n);
            if (gOut != 1.0f)
                for (int i = 0; i < n; ++i)
                    buf[i] *= gOut;
        }
    }

    // Upstream ids: g_in, g_out, mode, then ft_/fm_/s_/f_/g_/q_/w_ followed by the band number 0-7.
    int resolve(const juce::String& id) const override
    {
        if (id == "g_in")  return 0;
        if (id == "g_out") return 1;
        if (id == "mode")  return 2;
        static constexpr const char* prefixes[] = { "ft_", "fm_", "s_", "f_", "g_", "q_", "w_" };
        for (int p = 0; p < numFields; ++p)
            if (id.startsWith(prefixes[p]))
            {
                const auto digits = id.fromFirstOccurrenceOf("_", false, false);
                if (digits.isNotEmpty() && digits.containsOnly("0123456789"))
                {
                    const int band = digits.getIntValue();
                    if (band >= 0 && band < numBands)
                        return 3 + p * numBands + band;
                }
            }
        return -1;
    }

    void setNative(int index, float v) noexcept override
    {
        if (index == 0) { gIn = v; return; }
        if (index == 1) { gOut = v; return; }
        if (index < 3)  return;     // mode: always IIR
        const int field = (index - 3) / numBands;
        auto& b = bands[(index - 3) % numBands];
        switch (field)
        {
            case 0: b.type  = juce::roundToInt(v); break;
            case 1: break;                           // filter mode: always RLC (bilinear)
            case 2: b.slope = juce::jlimit(0, 3, juce::roundToInt(v)); break;
            case 3: b.freq  = v; break;
            case 4: b.gain  = v; break;
            case 5: b.q     = v; break;
            default: break;                          // width: only for band-pass filters
        }
        dirty.store(true);
    }

private:
    static constexpr int numBands = 8;
    static constexpr int numFields = 7;

    struct Band { int type = 0; int slope = 0; float freq = 1000.0f, gain = 1.0f, q = 0.0f; };

    // LSP's "Filter type" menu: 0 Off, 1 Bell, 2 Hi-pass, 3 Hi-shelf, 4 Lo-pass, 5 Lo-shelf, 6 Notch.
    void rebuild() noexcept
    {
        using namespace lsp::dspu;
        for (int b = 0; b < numBands; ++b)
        {
            const auto& s = bands[b];
            filter_params_t fp;
            fp.nType = FLT_NONE;
            fp.nSlope = 1;
            fp.fGain = 1.0f;
            fp.fQuality = s.q;
            fp.fFreq = fp.fFreq2 = juce::jlimit(10.0f, 0.49f * (float) sr, s.freq);

            const auto slope = (uint32_t) (s.slope + 1);
            switch (s.type)
            {
                case 1: fp.nType = FLT_BT_RLC_BELL;    fp.nSlope = slope;     fp.fGain = s.gain; break;
                case 2: fp.nType = FLT_BT_RLC_HIPASS;  fp.nSlope = 2 * slope; break;
                case 3: fp.nType = FLT_BT_RLC_HISHELF; fp.nSlope = slope;     fp.fGain = s.gain; break;
                case 4: fp.nType = FLT_BT_RLC_LOPASS;  fp.nSlope = 2 * slope; break;
                case 5: fp.nType = FLT_BT_RLC_LOSHELF; fp.nSlope = slope;     fp.fGain = s.gain; break;
                case 6: fp.nType = FLT_BT_RLC_NOTCH;   fp.nSlope = slope;     break;
                default: break;
            }
            for (auto& ch : filters)
                ch[b].update(sr, &fp);
        }
    }

    lsp::dspu::Filter filters[2][numBands];
    Band bands[numBands];
    float gIn = 1.0f, gOut = 1.0f;
    size_t sr = 48000;
    std::atomic<bool> dirty { true };
};

std::unique_ptr<Module> createParaEqModule(const ModuleSpec& s) { return std::make_unique<ParaEqModule>(s); }

} // namespace plugsalot
