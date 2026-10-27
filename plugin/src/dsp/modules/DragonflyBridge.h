// Plugsalot - GPL-3.0-or-later
// Thin bridge to Dragonfly's DSP so its C++14-only headers (freeverb3 uses
// dynamic exception specifications) never reach the C++17 plugin code.
// Implemented in DragonflyBridge.cpp, compiled in the plugsalot_dragonfly library.
#pragma once

#include <cstdint>
#include <memory>

namespace plugsalot
{

class DragonflyPlate
{
public:
    explicit DragonflyPlate(double sampleRate);
    ~DragonflyPlate();
    static int numParams();
    void setParameterValue(int index, float value);
    void run(const float** inputs, float** outputs, uint32_t frames);
    void mute();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace plugsalot
