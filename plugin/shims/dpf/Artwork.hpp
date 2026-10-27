// Plugsalot shim: Dragonfly's DistrhoPluginInfo.h includes its UI artwork header
// only for window-size macros. Plugsalot has its own UI, so these are placeholders.
// GPL-3.0-or-later (Plugsalot).
#pragma once

namespace Artwork
{
static constexpr unsigned int backgroundWidth = 1;
static constexpr unsigned int backgroundHeight = 1;
}
