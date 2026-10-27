// Plugsalot - GPL-3.0-or-later
// Dusk Audio Multi-Comp: the framework-free MultiCompDSP core (GPL-3.0-or-later).
// Parameter bindings come from the core enum names recorded in catalogue/raw/multicomp.json.
#include "dsp/Module.h"
#include "StereoScratch.h"
#include "MultiCompDSP.hpp"
#include <unordered_map>

namespace plugsalot
{

class MultiCompModule final : public Module
{
public:
    using P = duskaudio::MultiCompDSP::Parameter;

    explicit MultiCompModule(const ModuleSpec& s) : Module(s)
    {
        static const std::unordered_map<std::string, P> byName {
#include "MultiCompParamNames.inc"
        };
        for (auto& p : spec.rawParams)
        {
            auto it = byName.find(p.core.toStdString());
            if (it != byName.end())
            {
                ids.add(p.upstreamId);
                bindings.push_back(it->second);
            }
        }
    }

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

    int resolve(const juce::String& id) const override { return ids.indexOf(id); }

    void setNative(int index, float v) noexcept override
    {
        if (index >= 0 && index < (int) bindings.size())
            dsp.setParameter(bindings[(size_t) index], v);
    }

    int latencySamples() const noexcept override { return dsp.getLatencySamples(); }

private:
    duskaudio::MultiCompDSP dsp;
    StereoScratch scratch;
    juce::StringArray ids;
    std::vector<P> bindings;
};

std::unique_ptr<Module> createMultiCompModule(const ModuleSpec& s) { return std::make_unique<MultiCompModule>(s); }

} // namespace plugsalot
