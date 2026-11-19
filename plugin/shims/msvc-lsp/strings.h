// Plugsalot - GPL-3.0-or-later
// MSVC stand-in for <strings.h> (LSP's stdlib/string.h includes it).
#pragma once
#include <string.h>
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
