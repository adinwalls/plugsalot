// Plugsalot - GPL-3.0-or-later
// Guitarix BlueAmp (brummer10/GxBlueAmp.lv2, GPL-2.0-or-later), Faust-generated DSP compiled unmodified.
#include "GxPrelude.h"

// PortIndex as in the upstream plugin/gx_blueamp.h, at global scope as the generated code expects.
typedef enum
{
   EFFECTS_OUTPUT,
   EFFECTS_INPUT,
   BYPASS,
   MASTER,
   TONE,
   VOLUME,
} PortIndex;

#include "gx_pluginlv2.h"
#include "blueamp.cc"
#undef min
#undef max
#include "GxWrap.h"

namespace plugsalot::gx
{
std::unique_ptr<GxAmp> createBlueAmp()
{
    return std::make_unique<GxPlugin>(blueamp::plugin(), std::vector<uint32_t> { MASTER, TONE, VOLUME }, std::vector<float> { .5f, .5f, .5f });
}
} // namespace plugsalot::gx
