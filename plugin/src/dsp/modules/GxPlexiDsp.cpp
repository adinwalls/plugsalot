// Plugsalot - GPL-3.0-or-later
// Guitarix Plexi (brummer10/GxPlexi.lv2, GPL-2.0-or-later), Faust-generated DSP compiled unmodified.
#include "GxPrelude.h"

// PortIndex as in the upstream plugin/gx_plexi.h, at global scope as the generated code expects.
typedef enum
{
   EFFECTS_OUTPUT,
   EFFECTS_INPUT,
   BYPASS,
   BASS,
   MASTER,
   MID,
   PRESENSE,
   TREBLE,
   VOLUME,
} PortIndex;

#include "gx_pluginlv2.h"
#include "plexi.cc"
#undef min
#undef max
#include "GxWrap.h"

namespace plugsalot::gx
{
std::unique_ptr<GxAmp> createPlexi()
{
    return std::make_unique<GxPlugin>(plexi::plugin(), std::vector<uint32_t> { VOLUME, MASTER, BASS, MID, TREBLE, PRESENSE }, std::vector<float> { .15f, .4f, .5f, .5f, .5f, .15f });
}
} // namespace plugsalot::gx
