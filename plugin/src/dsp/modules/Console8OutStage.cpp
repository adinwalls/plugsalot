// Plugsalot - GPL-3.0-or-later
// Airwindows Console8ChannelOut (MIT), compiled unmodified against the VST2 shim.
#include "AirwindowsStage.h"
#include "Console8ChannelOut.h"

namespace plugsalot
{

namespace
{
struct Stage final : AwStage
{
    Stage() : fx(nullptr) {}
    void prepare(double sr) override { fx.plugsalotSetSampleRate((float) sr); }
    void process(float** in, float** out, int n) noexcept override { fx.processReplacing(in, out, n); }
    void set(int index, float v) noexcept override { if (index >= 0 && index < kNumParameters) fx.setParameter(index, v); }
    Console8ChannelOut fx;
};
}

std::unique_ptr<AwStage> createConsole8OutStage() { return std::make_unique<Stage>(); }

} // namespace plugsalot
