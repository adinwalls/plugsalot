// Plugsalot - GPL-3.0-or-later
// Everything a Guitarix GxPlugins.lv2 Faust file (compiled unmodified from plugin/vendor/gx-*)
// expects to find before it is included, as the upstream plugin/gx_<name>.cpp provides it.
// Include this first in a wrapper translation unit, then define the plug-in's PortIndex enum at
// global scope, then gx_pluginlv2.h, then the vendored .cc, then GxWrap.h.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#define __rt_func
#define __rt_data
#define FAUSTFLOAT float
#ifndef N_
#define N_(String) (String)
#endif
#define always_inline inline
// The upstream code uses these macros, which clash with the standard library: keep std headers above this line.
#define max(x, y) (((x) > (y)) ? (x) : (y))
#define min(x, y) (((x) < (y)) ? (x) : (y))

template <int32_t N> inline float faustpower(float x) { return powf(x, N); }
template <int32_t N> inline double faustpower(double x) { return pow(x, N); }
template <int32_t N> inline int32_t faustpower(int32_t x) { return faustpower<N / 2>(x) * faustpower<N - N / 2>(x); }
template <> inline int32_t faustpower<0>(int32_t) { return 1; }
template <> inline int32_t faustpower<1>(int32_t x) { return x; }

template <class T> inline T mydsp_faustpower2_f(T x) { return (x * x); }
template <class T> inline T mydsp_faustpower3_f(T x) { return ((x * x) * x); }
template <class T> inline T mydsp_faustpower4_f(T x) { return (((x * x) * x) * x); }
template <class T> inline T mydsp_faustpower5_f(T x) { return ((((x * x) * x) * x) * x); }
template <class T> inline T mydsp_faustpower6_f(T x) { return (((((x * x) * x) * x) * x) * x); }
