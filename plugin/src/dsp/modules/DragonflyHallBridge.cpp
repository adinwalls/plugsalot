// Plugsalot - GPL-3.0-or-later
// Compiled as C++14 inside plugsalot_dragonfly (see plugin/CMakeLists.txt).
#include "DragonflyBridge.h"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginInfo.h"
#include "DSP.hpp"

namespace plugsalot
{

struct DragonflyHall::Impl
{
    explicit Impl(double sr) : dsp(sr) {}
    DragonflyReverbDSP dsp;
};

DragonflyHall::DragonflyHall(double sampleRate) : impl(new Impl(sampleRate)) {}
DragonflyHall::~DragonflyHall() = default;
int DragonflyHall::numParams() { return paramCount; }
void DragonflyHall::setParameterValue(int index, float value) { impl->dsp.setParameterValue((uint32_t) index, value); }
void DragonflyHall::run(const float** inputs, float** outputs, uint32_t frames) { impl->dsp.run(inputs, outputs, frames); }
void DragonflyHall::mute() { impl->dsp.mute(); }

} // namespace plugsalot
