// Plugsalot - GPL-3.0-or-later
// Guitarix UltraCab (brummer10/GxUltraCab.lv2, GPL-2.0-or-later), Faust-generated DSP compiled unmodified.
#include "GxPrelude.h"

// PortIndex as in the upstream plugin/gx_ultracab.h, at global scope as the generated code expects.
typedef enum
{
   EFFECTS_OUTPUT,
   EFFECTS_INPUT,
   EFFECTS_OUTPUT1,
   EFFECTS_INPUT1,
   BYPASS,
   GAIN,
   MIDS,
   PUNCH,
   RESONANCE,
   SIZE_,
   TOP,
} PortIndex;

#include "gx_pluginlv2.h"
#include "ultracab.cc"
#undef min
#undef max
#include "GxWrap.h"

namespace plugsalot::gx
{
std::unique_ptr<GxAmp> createUltraCab()
{
    return std::make_unique<GxPlugin>(ultracab::plugin(), std::vector<uint32_t> { GAIN, MIDS, PUNCH, RESONANCE, SIZE_, TOP }, std::vector<float> { .5f, .5f, .5f, .5f, .5f, .5f });
}
} // namespace plugsalot::gx
