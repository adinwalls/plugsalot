// Plugsalot shim: the minimum of DPF that Dragonfly's DSP.cpp uses, so the
// upstream file compiles unmodified without the DISTRHO Plugin Framework.
// GPL-3.0-or-later (Plugsalot).
#pragma once

#include <cmath>
#include <cstdint>
#include <limits>

template <typename T>
static inline bool d_isNotEqual(const T v1, const T v2)
{
    return std::abs(v1 - v2) >= std::numeric_limits<T>::epsilon();
}
