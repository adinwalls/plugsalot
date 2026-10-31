// Plugsalot - GPL-3.0-or-later
// Guitarix amps and pedals (GxPlugins.lv2 by Hermann Meyer / brummer10 and the Guitarix project,
// GPL-2.0-or-later): Supersonic, BlueAmp, Plexi, Guvnor and UltraCab. Knobs, not capture files.
// Every parameter is 0..1 and passes straight through to the Faust plug-in's port.
#include "dsp/Module.h"
#include "GxAmp.h"

namespace plugsalot
{

namespace
{
struct GxDef
{
    std::unique_ptr<gx::GxAmp> (*create)();
    std::vector<const char*> names; // upstream ids, in the order the wrapper connects the ports
};
}

class GuitarixModule final : public Module
{
public:
    GuitarixModule(const ModuleSpec& s, const GxDef& d) : Module(s), amp(d.create()), names(d.names) {}

    void prepare(double sampleRate, int maxBlockSize) override { amp->prepare(sampleRate, maxBlockSize); }

    void reset() override {}

    void process(float* const* ch, int n) noexcept override { amp->process(ch[0], ch[1], ch[0], ch[1], n); }

    int resolve(const juce::String& id) const override
    {
        for (size_t i = 0; i < names.size(); ++i)
            if (id == names[i])
                return (int) i;
        return -1;
    }

    void setNative(int index, float v) noexcept override { amp->setParam(index, v); }

private:
    std::unique_ptr<gx::GxAmp> amp;
    std::vector<const char*> names;
};

std::unique_ptr<Module> createGxSupersonicModule(const ModuleSpec& s) { return std::make_unique<GuitarixModule>(s, GxDef { gx::createSupersonic, { "GAIN", "BASS", "TREBLE", "VOLUME" } }); }
std::unique_ptr<Module> createGxBlueAmpModule(const ModuleSpec& s)    { return std::make_unique<GuitarixModule>(s, GxDef { gx::createBlueAmp, { "MASTER", "TONE", "VOLUME" } }); }
std::unique_ptr<Module> createGxGuvnorModule(const ModuleSpec& s)     { return std::make_unique<GuitarixModule>(s, GxDef { gx::createGuvnor, { "GAIN", "BASS", "MID", "TREBLE", "LEVEL" } }); }
std::unique_ptr<Module> createGxUltraCabModule(const ModuleSpec& s)   { return std::make_unique<GuitarixModule>(s, GxDef { gx::createUltraCab, { "GAIN", "MIDS", "PUNCH", "RESONANCE", "SIZE_", "TOP" } }); }
std::unique_ptr<Module> createGxPlexiModule(const ModuleSpec& s)      { return std::make_unique<GuitarixModule>(s, GxDef { gx::createPlexi, { "VOLUME", "MASTER", "BASS", "MID", "TREBLE", "PRESENSE" } }); }

} // namespace plugsalot
