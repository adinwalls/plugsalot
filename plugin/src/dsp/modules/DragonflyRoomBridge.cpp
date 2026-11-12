// Plugsalot - GPL-3.0-or-later
// Compiled as C++14 inside plugsalot_dragonfly (see plugin/CMakeLists.txt).
#include "DragonflyBridge.h"
#include "DistrhoPlugin.hpp"
#include "DistrhoPluginInfo.h"
#include "DSP.hpp"

namespace plugsalot
{

struct DragonflyRoom::Impl
{
    explicit Impl(double sr) : dsp(sr) {}
    DragonflyReverbDSP dsp;
};

DragonflyRoom::DragonflyRoom(double sampleRate) : impl(new Impl(sampleRate)) {}
DragonflyRoom::~DragonflyRoom() = default;
int DragonflyRoom::numParams() { return paramCount; }
void DragonflyRoom::setParameterValue(int index, float value) { impl->dsp.setParameterValue((uint32_t) index, value); }
void DragonflyRoom::run(const float** inputs, float** outputs, uint32_t frames) { impl->dsp.run(inputs, outputs, frames); }
void DragonflyRoom::mute() { impl->dsp.mute(); }

} // namespace plugsalot
