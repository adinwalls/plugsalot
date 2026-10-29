// Plugsalot - GPL-3.0-or-later
// The user's library of amp captures (.nam) and cabinet impulse responses (.wav).
// Everything lives in one folder per kind so the AI can be told exactly which
// file names exist and never has to invent a path.
#pragma once

#include <juce_core/juce_core.h>

namespace plugsalot
{

struct Library
{
    // "nam" or "ir". Created on demand. PLUGSALOT_LIBRARY overrides the root (tests).
    static juce::File folder(const juce::String& kind);

    // File names in the library, sorted.
    static juce::StringArray list(const juce::String& kind);

    // Library file for a name from a chain slot; invalid names give a non-existent File.
    static juce::File resolve(const juce::String& kind, const juce::String& name);

    // Copy a user-chosen file into the library (keeps its name, adds " 2" etc. on clashes).
    // Returns the library file name, or empty on failure.
    static juce::String import(const juce::String& kind, const juce::File& source);

    static juce::String extensionFor(const juce::String& kind) { return kind == "nam" ? ".nam" : ".wav"; }
};

} // namespace plugsalot
