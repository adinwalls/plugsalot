// Plugsalot - GPL-3.0-or-later
// Compiled as C++14 inside plugsalot_dragonfly (see plugin/CMakeLists.txt).
#include "DragonflyBridge.h"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginInfo.h"
#include "DSP.hpp"

namespace plugsalot
{

struct DragonflyPlate::Impl
{
    explicit Impl(double sr) : dsp(sr) {}
    DragonflyReverbDSP dsp;
};

DragonflyPlate::DragonflyPlate(double sampleRate) : impl(new Impl(sampleRate)) {}
DragonflyPlate::~DragonflyPlate() = default;
int DragonflyPlate::numParams() { return paramCount; }
void DragonflyPlate::setParameterValue(int index, float value) { impl->dsp.setParameterValue((uint32_t) index, value); }
void DragonflyPlate::run(const float** inputs, float** outputs, uint32_t frames) { impl->dsp.run(inputs, outputs, frames); }
void DragonflyPlate::mute() { impl->dsp.mute(); }

} // namespace plugsalot
