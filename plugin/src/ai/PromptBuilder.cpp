// Plugsalot - GPL-3.0-or-later
#include "PromptBuilder.h"
#include "PlugsalotData.h"
#include "core/Library.h"

namespace plugsalot
{

juce::String AudioAnalysis::describe() const
{
    if (! valid)
        return "No audio analysed yet (play the track through the plugin for a few seconds to include it).";
    auto f = [](float v) { return juce::String(v, 1); };
    return "Measured on the input over the last " + juce::String(seconds, 0) + " s: peak " + f(peakDb) + " dBFS, RMS "
           + f(rmsDb) + " dBFS, crest factor " + f(crestDb) + " dB. Band balance relative to RMS: low (<250 Hz) "
           + f(lowDb - rmsDb) + " dB, mid (250 Hz-4 kHz) " + f(midDb - rmsDb) + " dB, high (>4 kHz) " + f(highDb - rmsDb) + " dB.";
}

juce::String PromptBuilder::systemPrompt(const Catalogue& catalogue)
{
    int size = 0;
    const char* data = PlugsalotData::getNamedResource("guidelines_md", size);
    juce::String guidelines = data != nullptr ? juce::String::fromUTF8(data, size) : juce::String();

    return guidelines
         + "\n\n## Catalogue\n\nThe modules available in this version of Plugsalot, as JSON. "
           "Each parameter lists its type, range or options, default and a hint.\n\n"
         + catalogue.toPromptJson()
         + "\n";
}

juce::String PromptBuilder::userTurn(const SessionContext& ctx, const AudioAnalysis& analysis,
                                     const ChainState* current, const juce::String& userText)
{
    juce::String s;
    s << "Context: source = " << ctx.source << "; stage = " << ctx.stage << "; style = " << ctx.style
      << "; character = " << ctx.character << "; CPU mode = " << ctx.cpuMode << ".\n";
    s << "Audio: " << analysis.describe() << "\n";
    if (current != nullptr && ! current->isEmpty())
        s << "Current chain (module_index counts from 0):\n" << juce::JSON::toString(current->toVar(), true) << "\n";
    else
        s << "Current chain: none yet, so reply with a full chain.\n";
    s << "Request: " << (userText.trim().isNotEmpty() ? userText.trim() : juce::String("Set up a good starting chain for this source."));
    return s;
}

juce::String PromptBuilder::retryTurn(const juce::StringArray& errors)
{
    return "Plugsalot rejected that reply:\n- " + errors.joinIntoString("\n- ")
         + "\nSend a corrected reply as a single JSON object, following the hard limits.";
}

} // namespace plugsalot
