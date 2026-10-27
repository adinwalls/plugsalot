// Plugsalot - GPL-3.0-or-later
// Copies the in-place chain buffer into scratch so upstream cores that read and
// write separate buffers never see aliased input and output.
#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

namespace plugsalot
{

struct StereoScratch
{
    juce::AudioBuffer<float> buffer;

    void prepare(int maxBlockSize) { buffer.setSize(2, juce::jmax(1, maxBlockSize), false, true, true); }

    // Returns input pointers (copies of channels) valid until the next call.
    const float* const* copyFrom(float* const* channels, int numSamples) noexcept
    {
        const int n = juce::jmin(numSamples, buffer.getNumSamples());
        buffer.copyFrom(0, 0, channels[0], n);
        buffer.copyFrom(1, 0, channels[1], n);
        ptrs[0] = buffer.getReadPointer(0);
        ptrs[1] = buffer.getReadPointer(1);
        return ptrs;
    }

    const float* ptrs[2] {};
};

} // namespace plugsalot
