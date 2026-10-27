// Plugsalot shim replacing DPF's extra/ScopedDenormalDisable.hpp.
// Flushes denormals to zero for the lifetime of the object (x86 SSE and ARM64).
// GPL-3.0-or-later (Plugsalot).
#pragma once

#include <cstdint>

#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
 #include <xmmintrin.h>
#endif

class ScopedDenormalDisable
{
public:
    ScopedDenormalDisable() noexcept
    {
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
        saved = _mm_getcsr();
        _mm_setcsr(saved | 0x8040); // FTZ | DAZ
#elif defined(__aarch64__) && !defined(_MSC_VER)
        uint64_t fpcr;
        asm volatile("mrs %0, fpcr" : "=r"(fpcr));
        saved = fpcr;
        asm volatile("msr fpcr, %0" ::"r"(fpcr | (1ULL << 24)));
#endif
    }

    ~ScopedDenormalDisable() noexcept
    {
#if defined(__SSE__) || defined(_M_X64) || defined(_M_IX86) || defined(__x86_64__)
        _mm_setcsr(static_cast<unsigned int>(saved));
#elif defined(__aarch64__) && !defined(_MSC_VER)
        asm volatile("msr fpcr, %0" ::"r"(saved));
#endif
    }

private:
    uint64_t saved = 0;
};
