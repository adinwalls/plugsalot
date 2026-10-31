// Plugsalot - GPL-3.0-or-later
// A Guitarix (GxPlugins.lv2) Faust plug-in behind a small interface. Each plug-in lives in its own
// translation unit (the generated files reuse symbol names), so the module code only sees GxAmp.
#pragma once

#include <memory>

namespace plugsalot::gx
{

class GxAmp
{
public:
    virtual ~GxAmp() = default;
    virtual void prepare(double sampleRate, int maxBlock) = 0;
    virtual void setParam(int index, float value) = 0; // index into the knob list of the module
    // Inputs and outputs may alias; the wrapper copies as needed.
    virtual void process(const float* inL, const float* inR, float* outL, float* outR, int n) = 0;
};

std::unique_ptr<GxAmp> createSupersonic();
std::unique_ptr<GxAmp> createBlueAmp();
std::unique_ptr<GxAmp> createGuvnor();
std::unique_ptr<GxAmp> createUltraCab();
std::unique_ptr<GxAmp> createPlexi();

} // namespace plugsalot::gx
