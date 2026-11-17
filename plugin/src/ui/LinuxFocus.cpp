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
    using SyncFn = int (*)(Display*, int);
    using ErrFn = int (*)(Display*, XErrorEvent*);
    using SetErrFn = ErrFn (*)(ErrFn);

    X11()
    {
        lib = dlopen("libX11.so.6", RTLD_LAZY | RTLD_LOCAL);
        if (lib == nullptr)
            return;
        open  = (OpenFn)  dlsym(lib, "XOpenDisplay");
        focus = (FocusFn) dlsym(lib, "XSetInputFocus");
        flush = (FlushFn) dlsym(lib, "XFlush");
        sync  = (SyncFn)  dlsym(lib, "XSync");
        setErr = (SetErrFn) dlsym(lib, "XSetErrorHandler");
        if (open != nullptr && focus != nullptr && flush != nullptr && sync != nullptr && setErr != nullptr)
            display = open(nullptr);
    }

    void* lib = nullptr;
    OpenFn open = nullptr;
    FocusFn focus = nullptr;
    FlushFn flush = nullptr;
    SyncFn sync = nullptr;
    SetErrFn setErr = nullptr;
    Display* display = nullptr;
};

// Xlib's default error handler calls exit(), which takes the whole host down. A focus
// request can legitimately fail (BadMatch when the window isn't viewable, BadWindow
// when the host just destroyed it), so errors from it are swallowed.
int ignoreXError(Display*, XErrorEvent*) { return 0; }
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
    // Errors arrive asynchronously, so sync inside the guard. The handler is process-wide,
    // so the host's own handler is put back straight afterwards.
    auto previous = x.setErr(ignoreXError);
    x.focus(x.display, window, RevertToParent, CurrentTime);
    x.sync(x.display, 0);
    x.setErr(previous);
    return true;
   #else
    juce::ignoreUnused(c);
    return false;
   #endif
}

} // namespace plugsalot
