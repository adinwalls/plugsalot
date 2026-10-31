// Plugsalot - GPL-3.0-or-later
// Guitarix Supersonic (brummer10/GxSupersonic.lv2, GPL-2.0-or-later), Faust-generated DSP compiled unmodified.
#include "GxPrelude.h"

// PortIndex as in the upstream plugin/gx_supersonic.h, at global scope as the generated code expects.
typedef enum
{
   EFFECTS_OUTPUT,
   EFFECTS_INPUT,
   BYPASS,
   GAIN,
   BASS,
   TREBLE,
   VOLUME,
} PortIndex;

#include "gx_pluginlv2.h"
#include "supersonic.cc"
#undef min
#undef max
#include "GxWrap.h"

namespace plugsalot::gx
{
std::unique_ptr<GxAmp> createSupersonic()
{
    return std::make_unique<GxPlugin>(supersonic::plugin(), std::vector<uint32_t> { GAIN, BASS, TREBLE, VOLUME }, std::vector<float> { .15f, .5f, .5f, .25f });
}
} // namespace plugsalot::gx
