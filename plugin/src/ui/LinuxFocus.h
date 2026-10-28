// Plugsalot - GPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace plugsalot
{

// Linux only: asks the X server to send keystrokes straight to the window that
// holds `c`. Plugin hosts such as Ardour keep the keyboard on their own window
// and JUCE's own request can be ignored, so a text box inside the plugin would
// never receive typing. Returns false where it does nothing (other systems, or
// no X display).
bool takeSystemKeyboardFocus(juce::Component& c);

} // namespace plugsalot
