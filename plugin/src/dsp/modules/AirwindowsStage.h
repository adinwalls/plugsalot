// Plugsalot - GPL-3.0-or-later
// A tiny interface over one Airwindows VST2-style effect. Each Airwindows effect defines the same
// global names (kNumParameters, kParamA ...), so each one is compiled in its own translation unit
// and reached through this interface.
#pragma once

#include <memory>

namespace plugsalot
{

struct AwStage
{
    virtual ~AwStage() = default;
    virtual void prepare(double sampleRate) = 0;
    virtual void process(float** in, float** out, int numSamples) noexcept = 0;
    virtual void set(int index, float value) noexcept { (void) index; (void) value; }
};

std::unique_ptr<AwStage> createConsole8InStage();
std::unique_ptr<AwStage> createConsole8HypeStage();
std::unique_ptr<AwStage> createConsole8OutStage();

} // namespace plugsalot
