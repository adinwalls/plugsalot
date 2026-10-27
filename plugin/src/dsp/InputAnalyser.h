// Plugsalot - GPL-3.0-or-later
// Measures the input the AI is asked about: peak, RMS, crest factor and a
// three-band balance, averaged over roughly the last ten seconds of signal.
#pragma once

#include "ai/PromptBuilder.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>

namespace plugsalot
{

class InputAnalyser
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        lowCoef = std::exp(-2.0 * juce::MathConstants<double>::pi * 250.0 / sr);
        highCoef = std::exp(-2.0 * juce::MathConstants<double>::pi * 4000.0 / sr);
        reset();
    }

    void reset()
    {
        lowState = highState = 0.0;
        meanSq = lowSq = midSq = highSq = 0.0;
        peak = 0.0f;
        activeSeconds.store(0.0);
    }

    // Audio thread.
    void push(const juce::AudioBuffer<float>& b) noexcept
    {
        const int n = b.getNumSamples();
        if (n == 0) return;
        double sq = 0, lo = 0, hi = 0, mid = 0;
        float blockPeak = 0;
        for (int i = 0; i < n; ++i)
        {
            float x = b.getSample(0, i);
            if (b.getNumChannels() > 1)
                x = 0.5f * (x + b.getSample(1, i));
            blockPeak = juce::jmax(blockPeak, std::abs(x));
            lowState = x + lowCoef * (lowState - x);       // one-pole low-pass at 250 Hz
            highState = x + highCoef * (highState - x);    // one-pole low-pass at 4 kHz
            const double l = lowState, h = x - highState, m = x - l - h;
            sq += (double) x * x; lo += l * l; hi += h * h; mid += m * m;
        }
        const double blockRmsSq = sq / n;
        if (blockRmsSq < 1.0e-6) // below -60 dBFS: silence, don't let it dilute the picture
            return;
        const double blockSeconds = n / sr;
        const double a = std::exp(-blockSeconds / 10.0); // ~10 s averaging
        meanSq = a * meanSq + (1 - a) * blockRmsSq;
        lowSq = a * lowSq + (1 - a) * lo / n;
        midSq = a * midSq + (1 - a) * mid / n;
        highSq = a * highSq + (1 - a) * hi / n;
        peak = juce::jmax(peak * (float) std::pow(a, 0.1), blockPeak);
        activeSeconds.store(activeSeconds.load() + blockSeconds);
        publish();
    }

    // Any thread.
    AudioAnalysis snapshot() const
    {
        AudioAnalysis a;
        a.seconds = activeSeconds.load();
        a.valid = a.seconds >= 2.0;
        a.peakDb = peakDb.load(); a.rmsDb = rmsDb.load(); a.crestDb = a.peakDb - a.rmsDb;
        a.lowDb = lowDb.load(); a.midDb = midDb.load(); a.highDb = highDb.load();
        return a;
    }

private:
    void publish() noexcept
    {
        auto db = [](double meanSquare) { return (float) juce::Decibels::gainToDecibels(std::sqrt(meanSquare), -120.0); };
        rmsDb.store(db(meanSq));
        lowDb.store(db(lowSq)); midDb.store(db(midSq)); highDb.store(db(highSq));
        peakDb.store(juce::Decibels::gainToDecibels(peak, -120.0f));
    }

    double sr = 44100.0, lowCoef = 0, highCoef = 0, lowState = 0, highState = 0;
    double meanSq = 0, lowSq = 0, midSq = 0, highSq = 0;
    float peak = 0;
    std::atomic<double> activeSeconds { 0 };
    std::atomic<float> peakDb { -120 }, rmsDb { -120 }, lowDb { -120 }, midDb { -120 }, highDb { -120 };
};

} // namespace plugsalot
