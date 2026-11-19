// Plugsalot - GPL-3.0-or-later
// Listens to the plugin input and works out the song's key: a small YIN pitch tracker
// (on a low-passed, decimated mono sum) feeds TuneBrew's KeyDetector, which histograms
// held notes and correlates them against the 24 major/minor key profiles.
//
// Meant for a single melodic voice or instrument. On a full mix there is no single
// pitch to follow, so confidence simply stays low.
//
// push() runs on the audio thread; everything else may be called from any thread.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <vector>

// The vendored header uses M_PI, which MSVC only defines with _USE_MATH_DEFINES.
#ifndef M_PI
 #define M_PI 3.14159265358979323846
#endif
#include "KeyDetect.hpp" // plugin/vendor/adin-brew/src, compiled unmodified

namespace plugsalot
{

class KeyTracker
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        dec = std::max(1, (int) std::floor(sr / 22050.0 + 1.0e-4));
        dsr = sr / dec;
        tMax = (int) std::ceil(dsr / 70.0);               // lowest pitch followed: 70 Hz
        tMin = std::max(2, (int) std::floor(dsr / 800.0)); // highest: 800 Hz
        hop = std::max(32, (int) std::round(dsr * 0.012)); // one pitch estimate every ~12 ms
        hopSeconds = (double) hop / dsr;
        ringSize = 1;
        while (ringSize < 4 * tMax)
            ringSize <<= 1;
        ring.assign((size_t) ringSize, 0.0f);
        diff.assign((size_t) tMax + 2, 0.0f);
        cmnd.assign((size_t) tMax + 2, 0.0f);
        lpA = (float) std::exp(-2.0 * 3.14159265358979 * 1000.0 / sr);
        rmsA = (float) std::exp(-1.0 / (0.03 * sr));
        reset();
    }

    void reset()
    {
        detector.reset();
        std::fill(ring.begin(), ring.end(), 0.0f);
        lp1 = lp2 = ms = decAcc = 0.0f;
        decCount = hopCount = evalCount = steady = 0;
        written = 0;
        lastMidi = -1000.0;
        key.store(-1); conf.store(0.0f); seconds.store(0.0f);
    }

    // Audio thread: `ch` is the first input channel (a second one is averaged in when present).
    void push(const float* left, const float* right, int n) noexcept
    {
        if (locked.load(std::memory_order_relaxed) || ring.empty())
            return;
        if (relearn.exchange(false))
            reset();
        for (int i = 0; i < n; ++i)
        {
            const float x = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
            ms = rmsA * ms + (1.0f - rmsA) * x * x;
            lp1 = lpA * lp1 + (1.0f - lpA) * x;
            lp2 = lpA * lp2 + (1.0f - lpA) * lp1;
            decAcc += lp2;
            if (++decCount < dec)
                continue;
            ring[(size_t) (written & (ringSize - 1))] = decAcc / (float) dec;
            decAcc = 0.0f; decCount = 0;
            ++written;
            if (++hopCount >= hop && written >= 2 * tMax)
            {
                hopCount = 0;
                analyseHop();
            }
        }
    }

    // Message thread: commit to the key shown now, so it stops re-detecting. False when there is no key yet.
    bool lock()
    {
        const int k = key.load();
        if (k < 0)
            return false;
        lockedKey.store(k);
        locked.store(true);
        return true;
    }

    // Message thread: forget the lock and learn the key afresh.
    void unlock()
    {
        relearn.store(true);
        locked.store(false);
        key.store(-1); conf.store(0.0f); seconds.store(0.0f);
    }

    bool isLocked() const { return locked.load(); }
    // 0..11 major, 12..23 minor (tonic = value % 12), -1 when nothing is known yet.
    int keyIndex() const { return locked.load() ? lockedKey.load() : key.load(); }
    float confidence() const { return locked.load() ? 1.0f : conf.load(); }
    float secondsHeard() const { return seconds.load(); }

private:
    void analyseHop() noexcept
    {
        // ~ -50 dBFS gate: silence and room noise carry no pitch.
        if (ms < 1.0e-5f)
        {
            steady = 0;
            return;
        }
        const double midi = estimateMidi();
        if (midi < 0.0)
        {
            steady = 0;
            lastMidi = -1000.0;
            return;
        }
        // Only count notes that hold still for ~3 hops (36 ms): glides and onsets say little about the key.
        steady = std::abs(midi - lastMidi) < 0.6 ? steady + 1 : 1;
        lastMidi = midi;
        if (steady >= 3)
            detector.add(midi, hopSeconds);

        if (++evalCount >= 40) // about twice a second
        {
            evalCount = 0;
            int k, alt;
            float c, off;
            detector.evaluate(k, c, alt, off);
            key.store(k);
            conf.store(c);
            seconds.store((float) detector.seconds());
        }
    }

    // YIN on the most recent window. Returns a MIDI note (fractional), or -1 when unvoiced.
    double estimateMidi() noexcept
    {
        const int w = tMax; // integration window
        const int mask = ringSize - 1;
        const int64_t start = written - (int64_t) (w + tMax);
        auto at = [&](int64_t i) { return ring[(size_t) (i & mask)]; };

        diff[0] = 0.0f;
        for (int tau = 1; tau <= tMax; ++tau)
        {
            float d = 0.0f;
            for (int j = 0; j < w; ++j)
            {
                const float e = at(start + j) - at(start + j + tau);
                d += e * e;
            }
            diff[(size_t) tau] = d;
        }
        cmnd[0] = 1.0f;
        float run = 0.0f;
        for (int tau = 1; tau <= tMax; ++tau)
        {
            run += diff[(size_t) tau];
            cmnd[(size_t) tau] = run > 0.0f ? diff[(size_t) tau] * (float) tau / run : 1.0f;
        }
        int best = -1;
        for (int tau = tMin; tau < tMax; ++tau)
        {
            if (cmnd[(size_t) tau] < 0.15f)
            {
                while (tau + 1 < tMax && cmnd[(size_t) tau + 1] < cmnd[(size_t) tau])
                    ++tau;
                best = tau;
                break;
            }
        }
        if (best < 0)
            return -1.0;
        double period = best;
        const float a = cmnd[(size_t) best - 1], b = cmnd[(size_t) best], c = cmnd[(size_t) best + 1];
        const float denom = a - 2.0f * b + c;
        if (std::abs(denom) > 1.0e-9f)
            period += std::clamp((double) (0.5f * (a - c) / denom), -0.5, 0.5);
        const double hz = dsr / period;
        return 69.0 + 12.0 * std::log2(hz / 440.0);
    }

    double sr = 44100.0, dsr = 22050.0, hopSeconds = 0.012, lastMidi = -1000.0;
    int dec = 2, tMax = 315, tMin = 27, hop = 265, ringSize = 1024;
    int decCount = 0, hopCount = 0, evalCount = 0, steady = 0;
    int64_t written = 0;
    float lpA = 0, rmsA = 0, lp1 = 0, lp2 = 0, ms = 0, decAcc = 0;
    std::vector<float> ring, diff, cmnd;
    tunebrew::KeyDetector detector;

    std::atomic<bool> locked { false }, relearn { false };
    std::atomic<int> key { -1 }, lockedKey { -1 };
    std::atomic<float> conf { 0.0f }, seconds { 0.0f };
};

} // namespace plugsalot
