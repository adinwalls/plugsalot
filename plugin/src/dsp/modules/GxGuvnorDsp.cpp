// Plugsalot - GPL-3.0-or-later
// Guitarix Guvnor (brummer10/GxGuvnor.lv2, GPL-2.0-or-later), Faust-generated DSP compiled unmodified.
#include "GxPrelude.h"

// PortIndex as in the upstream plugin/gx_guvnor.h, at global scope as the generated code expects.
typedef enum
{
   EFFECTS_OUTPUT,
   EFFECTS_INPUT,
   BASS,
   GAIN,
   LEVEL,
   MID,
   TREBLE,
} PortIndex;

#include "gx_pluginlv2.h"
#include "guvnor.cc"
#undef min
#undef max
#include "GxWrap.h"

namespace plugsalot::gx
{
std::unique_ptr<GxAmp> createGuvnor()
{
    return std::make_unique<GxPlugin>(guvnor::plugin(), std::vector<uint32_t> { GAIN, BASS, MID, TREBLE, LEVEL }, std::vector<float> { .5f, .5f, .5f, .5f, .5f });
}
} // namespace plugsalot::gx
