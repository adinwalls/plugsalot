// Plugsalot - GPL-3.0-or-later
// Include after gx_pluginlv2.h and the vendored Faust .cc (which define PluginLV2): drives a
// PluginLV2 as a GxAmp. Ports are the upstream PortIndex values of the knobs, in knob order.
#pragma once

#include "GxAmp.h"
#include <vector>

namespace plugsalot::gx
{

class GxPlugin final : public GxAmp
{
public:
    GxPlugin(PluginLV2* plugin, std::vector<uint32_t> knobPorts, std::vector<float> defaults)
        : p(plugin), values(std::move(defaults))
    {
        for (size_t i = 0; i < knobPorts.size(); ++i)
            p->connect_ports(knobPorts[i], &values[i], p);
    }

    ~GxPlugin() override { p->delete_instance(p); }

    void prepare(double sr, int maxBlock) override
    {
        p->set_samplerate((uint32_t) sr, p);
        if (p->activate_plugin != nullptr)
            p->activate_plugin(true, p);
        if (p->clear_state != nullptr)
            p->clear_state(p);
        for (auto* v : { &a, &b, &c, &d })
            v->assign((size_t) std::max(1, maxBlock), 0.f);
    }

    void setParam(int i, float v) override
    {
        if (i >= 0 && i < (int) values.size())
            values[(size_t) i] = v;
    }

    void process(const float* inL, const float* inR, float* outL, float* outR, int n) override
    {
        n = std::min(n, (int) a.size());
        if (p->stereo_audio != nullptr)
        {
            std::copy(inL, inL + n, a.begin());
            std::copy(inR, inR + n, b.begin());
            p->stereo_audio(n, a.data(), b.data(), c.data(), d.data(), p);
            std::copy(c.begin(), c.begin() + n, outL);
            std::copy(d.begin(), d.begin() + n, outR);
        }
        else
        {
            // Guitar is mono: sum the two channels, process once, send to both sides.
            for (int i = 0; i < n; ++i)
                a[(size_t) i] = 0.5f * (inL[i] + inR[i]);
            p->mono_audio(n, a.data(), c.data(), p);
            std::copy(c.begin(), c.begin() + n, outL);
            std::copy(c.begin(), c.begin() + n, outR);
        }
    }

private:
    PluginLV2* p;
    std::vector<float> values; // addresses handed to the plug-in: never resized after construction
    std::vector<float> a, b, c, d;
};

} // namespace plugsalot::gx
