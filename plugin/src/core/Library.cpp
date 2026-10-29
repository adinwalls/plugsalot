// Plugsalot - GPL-3.0-or-later
#include "Library.h"

namespace plugsalot
{

juce::File Library::folder(const juce::String& kind)
{
    juce::File root;
    const auto env = juce::SystemStats::getEnvironmentVariable("PLUGSALOT_LIBRARY", {});
    if (env.isNotEmpty())
        root = juce::File(env);
    else
        root = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("Plugsalot");
    auto dir = root.getChildFile(kind == "nam" ? "Captures" : "Cabinets");
    dir.createDirectory();
    return dir;
}

juce::StringArray Library::list(const juce::String& kind)
{
    juce::StringArray names;
    const auto ext = extensionFor(kind);
    for (auto& f : folder(kind).findChildFiles(juce::File::findFiles, false, "*" + ext))
        names.add(f.getFileName());
    names.sort(true);
    return names;
}

juce::File Library::resolve(const juce::String& kind, const juce::String& name)
{
    if (name.isEmpty() || name.containsAnyOf("/\\") || name.contains("..") || ! name.endsWithIgnoreCase(extensionFor(kind)))
        return {};
    return folder(kind).getChildFile(name);
}

juce::String Library::import(const juce::String& kind, const juce::File& source)
{
    if (! source.existsAsFile() || ! source.hasFileExtension(extensionFor(kind).substring(1)))
        return {};
    auto dir = folder(kind);
    auto target = dir.getChildFile(source.getFileName());
    if (target == source)
        return target.getFileName();
    for (int n = 2; target.exists() && n < 1000; ++n)
        target = dir.getChildFile(source.getFileNameWithoutExtension() + " " + juce::String(n) + extensionFor(kind));
    return source.copyFileTo(target) ? target.getFileName() : juce::String();
}

} // namespace plugsalot
