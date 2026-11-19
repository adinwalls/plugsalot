// Plugsalot - GPL-3.0-or-later
// Force-included when MSVC builds the vendored LSP filter code, which is written for GCC/Clang.
// It supplies the compiler macros the LSP headers test, and drops GNU attributes (inlining and
// alignment hints only; nothing here changes the maths).
#pragma once
#define __attribute__(x)
#define __ORDER_LITTLE_ENDIAN__ 1234
#define __ORDER_BIG_ENDIAN__ 4321
#define __BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__
#define __SIZE_WIDTH__ 64
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
