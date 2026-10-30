// Plugsalot - GPL-3.0-or-later
// Clean Delay: Plugsalot's own stereo / ping-pong delay. Written for this project (no upstream),
// so there is nothing to port. Cubic-interpolated delay lines with a smoothed time, high-pass and
// low-pass filters inside the feedback path, and an optional ducker that pulls the echoes down
// while the dry signal is loud so they fill the gaps instead of masking the performance.
#include "dsp/Module.h"
#include <cmath>
#include <vector>

namespace plugsalot
{

namespace
{
constexpr double kPi = 3.14159265358979323846;

// One-pole filters, 6 dB/oct each; two in series on the feedback path give a gentle 12 dB/oct.
struct OnePole
{
    double a = 0.0, z = 0.0;
    void setLowpass(double fc, double sr) { a = 1.0 - std::exp(-2.0 * kPi * fc / sr); }
    double lowpass(double x) { z += a * (x - z); return z; }
    double highpass(double x) { return x - lowpass(x); }
    void reset() { z = 0.0; }
};

struct Line
{
    std::vector<float> data;
    int mask = 0, write = 0;

    void allocate(int minSize)
    {
        int n = 1;
        while (n < minSize) n <<= 1;
        data.assign((size_t) n, 0.0f);
        mask = n - 1;
        write = 0;
    }
    void clear() { std::fill(data.begin(), data.end(), 0.0f); write = 0; }
    void push(float x) { data[(size_t) write] = x; write = (write + 1) & mask; }

    // Read `delay` samples behind the most recent write (Catmull-Rom interpolation; delay >= 2 samples).
    float read(double delay) const noexcept
    {
        const int whole = (int) delay;
        const float f = (float) (delay - whole);
        auto at = [this](int d) { return data[(size_t) ((write - d) & mask)]; };   // at(1) = the latest sample
        const float y0 = at(whole - 1), y1 = at(whole), y2 = at(whole + 1), y3 = at(whole + 2);
        return y1 + 0.5f * f * (y2 - y0 + f * (2.0f * y0 - 5.0f * y1 + 4.0f * y2 - y3 + f * (3.0f * (y1 - y2) + y3 - y0)));
    }
};
}

class DelayModule final : public Module
{
public:
    using Module::Module;

    void prepare(double sr, int) override
    {
        sampleRate = sr;
        for (auto& l : line)
            l.allocate((int) (2.05 * sr) + 8);
        timeSmooth = 1.0 - std::exp(-1.0 / (0.08 * sr));   // ~80 ms glide when the time changes
        paramSmooth = 1.0 - std::exp(-1.0 / (0.02 * sr));
        envAttack = 1.0 - std::exp(-1.0 / (0.005 * sr));
        envRelease = 1.0 - std::exp(-1.0 / (0.25 * sr));
        updateFilters();
        reset();
        curTime = targetTime();
        curFeedback = feedback * 0.01;
        curMix = mix * 0.01;
    }

    void reset() override
    {
        for (auto& l : line) l.clear();
        for (auto& f : hp) f.reset();
        for (auto& f : lp1) f.reset();
        for (auto& f : lp2) f.reset();
        env = 0.0;
    }

    void process(float* const* ch, int n) noexcept override
    {
        const bool pingPong = mode >= 0.5f;
        const double duckAmt = duck * 0.01;
        for (int i = 0; i < n; ++i)
        {
            curTime += timeSmooth * (targetTime() - curTime);
            curFeedback += paramSmooth * (feedback * 0.01 - curFeedback);
            curMix += paramSmooth * (mix * 0.01 - curMix);

            const float inL = ch[0][i], inR = ch[1][i];
            const double tapL = line[0].read(curTime), tapR = line[1].read(curTime);

            // Feedback path: high-pass then two low-passes, then a soft limit so a long
            // feedback tail can never run away.
            auto feed = [this](int c, double x)
            {
                x = hp[c].highpass(x);
                x = lp2[c].lowpass(lp1[c].lowpass(x));
                return std::fabs(x) > 1.0 ? std::copysign(1.0 + std::tanh(std::fabs(x) - 1.0) * 0.5, x) : x;
            };

            if (pingPong)
            {
                const float mono = 0.5f * (inL + inR);
                line[0].push(mono + (float) (curFeedback * feed(0, tapR)));
                line[1].push((float) (curFeedback * feed(1, tapL)));
            }
            else
            {
                line[0].push(inL + (float) (curFeedback * feed(0, tapL)));
                line[1].push(inR + (float) (curFeedback * feed(1, tapR)));
            }

            // Ducking follows the loudness of the dry input.
            const double level = std::fabs((double) inL) + std::fabs((double) inR);
            env += (level > env ? envAttack : envRelease) * (level - env);
            const double duckGain = 1.0 - duckAmt * juce::jmin(1.0, env * 4.0);

            const double wet = curMix * duckGain;
            ch[0][i] = (float) (inL + wet * tapL);
            ch[1][i] = (float) (inR + wet * tapR);
        }
    }

    int resolve(const juce::String& id) const override { return indexIn(names, 7, id); }

    void setNative(int index, float v) noexcept override
    {
        switch (index)
        {
            case 0: timeMs = v; break;
            case 1: mode = v; break;
            case 2: feedback = v; break;
            case 3: lowCut = v;  updateFilters(); break;
            case 4: highCut = v; updateFilters(); break;
            case 5: mix = v; break;
            case 6: duck = v; break;
            default: break;
        }
    }

private:
    double targetTime() const noexcept
    {
        return juce::jlimit(2.0, 2.0 * sampleRate, (double) timeMs * 0.001 * sampleRate);
    }

    void updateFilters() noexcept
    {
        const double hi = juce::jmin((double) highCut, sampleRate * 0.45);
        for (int c = 0; c < 2; ++c)
        {
            hp[c].setLowpass(juce::jmin((double) lowCut, sampleRate * 0.45), sampleRate);
            lp1[c].setLowpass(hi, sampleRate);
            lp2[c].setLowpass(hi, sampleRate);
        }
    }

    static constexpr const char* names[] = { "time_ms", "mode", "feedback", "low_cut", "high_cut", "mix", "duck" };

    double sampleRate = 48000.0;
    Line line[2];
    OnePole hp[2], lp1[2], lp2[2];
    double timeSmooth = 0.0, paramSmooth = 0.0, envAttack = 0.0, envRelease = 0.0;
    double curTime = 1.0, curFeedback = 0.0, curMix = 0.0, env = 0.0;
    float timeMs = 350.0f, mode = 0.0f, feedback = 30.0f, lowCut = 150.0f, highCut = 6000.0f, mix = 20.0f, duck = 0.0f;
};

std::unique_ptr<Module> createDelayModule(const ModuleSpec& s) { return std::make_unique<DelayModule>(s); }

} // namespace plugsalot
