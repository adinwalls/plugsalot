// Plugsalot - GPL-3.0-or-later
#include "LinuxFocus.h"

#if JUCE_LINUX || JUCE_BSD
 #include <X11/Xlib.h>
 #include <dlfcn.h>
#endif

namespace plugsalot
{

#if JUCE_LINUX || JUCE_BSD
namespace
{
// libX11 is loaded at run time (JUCE does the same) so the plugin needs no extra link step.
struct X11
{
    using OpenFn  = Display* (*)(const char*);
    using FocusFn = int (*)(Display*, Window, int, Time);
    using FlushFn = int (*)(Display*);

    X11()
    {
        lib = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
        if (lib == nullptr)
            return;
        open  = (OpenFn)  dlsym(lib, "XOpenDisplay");
        focus = (FocusFn) dlsym(lib, "XSetInputFocus");
        flush = (FlushFn) dlsym(lib, "XFlush");
        if (open != nullptr && focus != nullptr && flush != nullptr)
            display = open(nullptr);
    }

    void* lib = nullptr;
    OpenFn open = nullptr;
    FocusFn focus = nullptr;
    FlushFn flush = nullptr;
    Display* display = nullptr;
};
} // namespace
#endif

bool takeSystemKeyboardFocus(juce::Component& c)
{
   #if JUCE_LINUX || JUCE_BSD
    static X11 x;
    auto* peer = c.getPeer();
    if (x.display == nullptr || peer == nullptr)
        return false;
    const auto window = (Window) (juce::pointer_sized_uint) peer->getNativeHandle();
    if (window == 0)
        return false;
    // CurrentTime: JUCE passes the window's last user-activity time, which an
    // embedded window often doesn't have, and the server then ignores the request.
    x.focus(x.display, window, RevertToParent, CurrentTime);
    x.flush(x.display);
    return true;
   #else
    juce::ignoreUnused(c);
    return false;
   #endif
}

} // namespace plugsalot
