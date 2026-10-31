// Plugsalot - GPL-3.0-or-later
// Headless tests: catalogue, validator parity with tools/chain_validator.py,
// every module on test audio (finite output, latency, CPU cost), chain engine
// swaps, and AI reply parsing. Exit code 0 = all passed.
//
// Optional live AI test: set PLUGSALOT_GEMINI_KEY or PLUGSALOT_CLAUDE_KEY.
#include "ai/AiSession.h"
#include "core/ChainValidator.h"
#include "core/Library.h"
#include "core/Tone3000.h"
#include "dsp/ChainEngine.h"
#include <juce_events/juce_events.h>
#include <chrono>
#include <iostream>

using namespace plugsalot;

static int failures = 0;
static void check(bool cond, const juce::String& what)
{
    std::cout << (cond ? "  PASS " : "  FAIL ") << what << "\n";
    if (! cond)
        ++failures;
}

static void fillTestSignal(juce::AudioBuffer<float>& b, int offset, double sr)
{
    // Vocal-ish test signal: 220 Hz tone with harmonics, a 6 kHz "ess" burst, noise.
    juce::Random rng(1234 + offset);
    for (int i = 0; i < b.getNumSamples(); ++i)
    {
        const double t = (offset + i) / sr;
        double s = 0.25 * std::sin(2 * juce::MathConstants<double>::pi * 220 * t)
                 + 0.08 * std::sin(2 * juce::MathConstants<double>::pi * 660 * t)
                 + 0.05 * (rng.nextFloat() * 2 - 1);
        if (std::fmod(t, 1.0) > 0.8)
            s += 0.15 * std::sin(2 * juce::MathConstants<double>::pi * 6000 * t);
        b.setSample(0, i, (float) s);
        b.setSample(1, i, (float) (s * 0.9));
    }
}

static void testCatalogue(const Catalogue& cat)
{
    std::cout << "Catalogue\n";
    check(cat.modules().size() == 19, "nineteen runnable modules embedded (" + juce::String((int) cat.modules().size()) + ")");
    for (auto& m : cat.modules())
    {
        bool defaultsOk = true;
        for (auto& p : m.params)
            if (p.check(p.defaultValue).isNotEmpty())
                defaultsOk = false;
        check(defaultsOk && ! m.params.empty(), m.id + ": params and raw defaults valid");
    }
    auto prompt = PromptBuilder::systemPrompt(cat);
    check(prompt.contains("Hard limits") && prompt.contains("\"multicomp\""), "system prompt has guidelines and catalogue ("
          + juce::String(prompt.length()) + " chars)");
}

static void testValidatorParity(const Catalogue& cat, const juce::File& examplesDir)
{
    std::cout << "Validator parity with catalogue/examples\n";
    ChainValidator v(cat);
    for (auto& f : examplesDir.findChildFiles(juce::File::findFiles, false, "*.json"))
    {
        auto c = juce::JSON::parse(f);
        auto r = v.validateChain(c.getProperty("reply", {}), c.getProperty("cpu_mode", "normal").toString());
        const bool expectValid = c.getProperty("expect", {}).toString() == "valid";
        auto needle = c.getProperty("expect_error_contains", {}).toString();
        bool ok = expectValid ? r.ok() : (! r.ok() && (needle.isEmpty() || r.errors.joinIntoString("\n").contains(needle)));
        // Python's jsonschema says "is too long"; the C++ validator says "too long" too.
        check(ok, f.getFileNameWithoutExtension() + (r.ok() ? "" : " -> " + r.errors[0].substring(0, 90)));
    }
}

static void testModules(const Catalogue& cat)
{
    std::cout << "Modules on test audio (48 kHz, 256-sample blocks, 3 s)\n";
    const double sr = 48000.0;
    const int block = 256, blocks = (int) (3 * sr / block);
    for (auto& spec : cat.modules())
    {
        auto m = createModule(spec);
        if (m == nullptr) { check(false, spec.id + ": created"); continue; }
        m->prepare(sr, block);
        m->reset();
        // A setting that makes the module audibly work.
        if (spec.id == "multicomp") { m->setParam("mode", "FET"); m->setParam("fet_threshold", -24.0); }
        if (spec.id == "deess") m->setParam("intensity", 0.6);
        if (spec.id == "tape") m->setParam("drive", 6.0);
        if (spec.id == "console_eq") { m->setParam("hpf_enabled", true); m->setParam("hpf_hz", 100.0); m->setParam("hm_gain", 4.0); }
        if (spec.id == "plate") m->setParam("wet", 30.0);
        if (spec.id == "limiter") m->setParam("input_gain", 6.0);

        juce::AudioBuffer<float> buf(2, block);
        double inEnergy = 0, outEnergy = 0, seconds = 0;
        float maxAbs = 0;
        bool finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            fillTestSignal(buf, b * block, sr);
            inEnergy += buf.getRMSLevel(0, 0, block);
            float* ch[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
            auto t0 = std::chrono::high_resolution_clock::now();
            m->process(ch, block);
            seconds += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
            for (int c = 0; c < 2; ++c)
                for (int i = 0; i < block; ++i)
                {
                    const float s = buf.getSample(c, i);
                    finite &= std::isfinite(s);
                    maxAbs = juce::jmax(maxAbs, std::abs(s));
                }
            outEnergy += buf.getRMSLevel(0, 0, block);
        }
        const double cpuPercent = 100.0 * seconds / 3.0;
        check(finite && maxAbs > 1.0e-4f && maxAbs < 8.0f,
              spec.id + ": finite, audible output (peak " + juce::String(juce::Decibels::gainToDecibels(maxAbs), 1)
              + " dBFS, level change " + juce::String(juce::Decibels::gainToDecibels(outEnergy / juce::jmax(1e-9, inEnergy)), 1)
              + " dB, latency " + juce::String(m->latencySamples()) + " samples, CPU " + juce::String(cpuPercent, 2) + "% of one core)");
        if (spec.id == "limiter")
            check(maxAbs <= juce::Decibels::decibelsToGain(-0.9f), "limiter holds peaks under -1 dBFS ceiling (+0.1 dB tolerance)");
    }
}

static void testTrackingLatency(const Catalogue& cat)
{
    std::cout << "Tracking-stage latency (tracking overrides applied, 48 kHz)\n";
    for (auto& spec : cat.modules())
    {
        if (! spec.trackingSafe)
            continue;
        auto m = createModule(spec);
        for (auto& o : spec.trackingOverrides)
            m->setParam(o.name.toString(), o.value);
        m->prepare(48000.0, 256);
        juce::AudioBuffer<float> buf(2, 256);
        for (int b = 0; b < 20; ++b)
        {
            fillTestSignal(buf, b * 256, 48000.0);
            float* ch[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
            m->process(ch, 256);
        }
        const int lat = m->latencySamples();
        check(lat <= 96, spec.id + ": " + juce::String(lat) + " samples (" + juce::String(lat / 48.0, 2) + " ms) when tracking");
    }
}

static void testEngine(const Catalogue& cat)
{
    std::cout << "Chain engine\n";
    ChainValidator v(cat);
    auto reply = juce::JSON::parse(R"({"schema_version":1,"source":"lead vocal","stage":"mixing","chain":[
        {"module":"console_eq","params":{"hpf_enabled":true,"hpf_hz":90}},
        {"module":"multicomp","params":{"mode":"Opto","opto_peak_reduction":45}},
        {"module":"deess","params":{"intensity":0.4}},
        {"module":"plate","params":{"wet":15}}],"explanation":"test"})");
    auto r = v.validateChain(reply, "normal");
    check(r.ok(), "vocal chain validates");

    ChainEngine engine;
    engine.prepare(48000.0, 512);
    engine.setChain(r.chain);
    juce::AudioBuffer<float> buf(2, 512);
    bool finite = true;
    for (int b = 0; b < 200; ++b)
    {
        fillTestSignal(buf, b * 512, 48000.0);
        engine.process(buf);
        finite &= std::isfinite(buf.getMagnitude(0, 512));
        if (b == 50) engine.setParam(1, "opto_peak_reduction", 70.0);
        if (b == 100)
        {
            // Change list: same instances survive, one module inserted.
            auto changes = juce::JSON::parse(R"({"schema_version":1,"changes":[{"op":"set","module_index":0,"param":"lm_gain","value":-3},
                {"op":"insert","at":4,"module":"limiter","params":{"ceiling":-1}}],"explanation":"x"})");
            auto r2 = v.applyChanges(changes, r.chain, "normal");
            check(r2.ok() && r2.chain.slots.size() == 5, "change list applies");
            check(r2.chain.slots[0].instanceId == r.chain.slots[0].instanceId, "existing module keeps its identity");
            engine.setChain(r2.chain);
        }
        if (b == 150) engine.setBypass(3, true);
        if (b % 20 == 0) engine.collectGarbage();
    }
    check(finite, "200 blocks through swaps, parameter changes and bypass stay finite");
    check(engine.numModules() == 5, "engine runs 5 modules after the change list");
    std::cout << "    chain latency: " << engine.latencySamples() << " samples\n";
}

// A 16-bit mono WAV holding a short decaying-noise "cabinet".
static juce::File writeTestIr(const juce::File& dir)
{
    const int n = 512, rate = 48000;
    juce::MemoryBlock mb;
    juce::MemoryOutputStream o(mb, false);
    o.write("RIFF", 4); o.writeInt(36 + n * 2); o.write("WAVEfmt ", 8);
    o.writeInt(16); o.writeShort(1); o.writeShort(1); o.writeInt(rate); o.writeInt(rate * 2); o.writeShort(2); o.writeShort(16);
    o.write("data", 4); o.writeInt(n * 2);
    juce::Random rnd(7);
    for (int i = 0; i < n; ++i)
        o.writeShort((short) (rnd.nextFloat() * 2.0f - 1.0f) * 0 + (short) ((rnd.nextFloat() * 2.0f - 1.0f) * 16000.0f * std::exp(-i / 60.0f)));
    auto f = dir.getChildFile("test cab.wav");
    f.replaceWithData(mb.getData(), mb.getSize());
    return f;
}

static void testFileModules(const Catalogue& cat, const juce::File& vendorModels)
{
    std::cout << "NAM Amp and Cabinet IR (captures from the library folder)\n";
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("plugsalot-test-lib");
    tmp.deleteRecursively();
    tmp.createDirectory();
    juce::File(tmp).getChildFile("lib").createDirectory();
#if JUCE_WINDOWS
    _putenv_s("PLUGSALOT_LIBRARY", tmp.getChildFile("lib").getFullPathName().toRawUTF8());
#else
    setenv("PLUGSALOT_LIBRARY", tmp.getChildFile("lib").getFullPathName().toRawUTF8(), 1);
#endif
    const auto a2 = Library::import("nam", vendorModels.getChildFile("A2.nam"));
    const auto a1 = Library::import("nam", vendorModels.getChildFile("wavenet_a1_standard.nam"));
    const auto ir = Library::import("ir", writeTestIr(tmp));
    check(a2 == "A2.nam" && a1 == "wavenet_a1_standard.nam" && ir == "test cab.wav", "files import into the library");
    check(Library::list("nam").size() == 2 && Library::list("ir").size() == 1, "library lists what was imported");
    check(! Library::resolve("nam", "../secret.nam").existsAsFile() && Library::resolve("nam", "../x.nam") == juce::File(), "library refuses path tricks");

    const double sr = 48000.0;
    const int block = 256, blocks = (int) (3 * sr / block);
    for (const char* name : { "A2.nam", "wavenet_a1_standard.nam" })
    {
        auto m = createModule(*cat.find("nam_amp"));
        m->prepare(sr, block);
        const auto msg = m->loadFile(Library::resolve("nam", name));
        check(msg.isEmpty() || msg.contains("trained at"), juce::String(name) + ": loads (" + (msg.isEmpty() ? juce::String("no warning") : msg) + ")");
        m->setParam("input_gain", 6.0);
        juce::AudioBuffer<float> buf(2, block), dry(2, block);
        double seconds = 0, diff = 0;
        float peak = 0;
        bool finite = true;
        for (int b = 0; b < blocks; ++b)
        {
            fillTestSignal(buf, b * block, sr);
            dry.makeCopyOf(buf);
            float* ch[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
            auto t0 = std::chrono::high_resolution_clock::now();
            m->process(ch, block);
            seconds += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < block; ++i)
            {
                finite &= std::isfinite(buf.getSample(0, i));
                peak = juce::jmax(peak, std::abs(buf.getSample(0, i)));
                diff += std::abs(buf.getSample(0, i) - dry.getSample(0, i));
            }
        }
        check(finite && diff > 1.0 && peak < 8.0f && peak > 1.0e-3f,
              juce::String(name) + ": capture changes the sound, finite (peak " + juce::String(juce::Decibels::gainToDecibels(peak), 1)
              + " dBFS, CPU " + juce::String(100.0 * seconds / 3.0, 2) + "% of one core)");
    }

    {
        auto m = createModule(*cat.find("nam_amp"));
        m->prepare(sr, block);
        const auto msg = m->loadFile(Library::resolve("nam", "missing.nam"));
        juce::AudioBuffer<float> buf(2, block);
        fillTestSignal(buf, 0, sr);
        juce::AudioBuffer<float> before;
        before.makeCopyOf(buf);
        float* ch[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
        m->process(ch, block);
        check(msg.isNotEmpty() && buf.getMagnitude(0, block) == before.getMagnitude(0, block), "missing capture reports a message and passes audio through");
    }

    {
        auto m = createModule(*cat.find("cab_ir"));
        m->prepare(sr, block);
        const auto msg = m->loadFile(Library::resolve("ir", "test cab.wav"));
        check(msg.isEmpty(), "impulse response loads" + (msg.isEmpty() ? juce::String() : " (" + msg + ")"));
        juce::AudioBuffer<float> buf(2, block), dry(2, block);
        double diff = 0;
        bool finite = true;
        for (int b = 0; b < 200; ++b)
        {
            fillTestSignal(buf, b * block, sr);
            dry.makeCopyOf(buf);
            float* ch[2] = { buf.getWritePointer(0), buf.getWritePointer(1) };
            m->process(ch, block);
            for (int i = 0; i < block; ++i)
            {
                finite &= std::isfinite(buf.getSample(1, i));
                diff += std::abs(buf.getSample(1, i) - dry.getSample(1, i));
            }
        }
        check(finite && diff > 1.0, "cabinet impulse response colours the signal, finite");
    }

    // Through the validator and engine: file names travel in the chain, bad ones are refused.
    ChainValidator v(cat);
    auto good = juce::JSON::parse(R"({"schema_version":1,"source":"electric guitar","stage":"mixing","chain":[
        {"module":"nam_amp","params":{"input_gain":3,"gate_threshold":-65},"file":"A2.nam"},
        {"module":"cab_ir","params":{"low_cut":90},"file":"test cab.wav"},
        {"module":"plate","params":{"wet":10}}],"explanation":"test"})");
    auto r = v.validateChain(good, "normal");
    check(r.ok() && r.chain.slots.size() == 3 && r.chain.slots[0].file == "A2.nam",
          "chain with capture and cabinet files validates" + (r.ok() ? juce::String() : " -> " + r.errors.joinIntoString("; ")));
    if (! r.ok() || r.chain.slots.size() < 3) { tmp.deleteRecursively(); return; }
    auto bad = juce::JSON::parse(R"({"schema_version":1,"source":"electric guitar","stage":"mixing","chain":[
        {"module":"nam_amp","params":{},"file":"../../etc/passwd.nam"}],"explanation":"x"})");
    check(! v.validateChain(bad, "normal").ok(), "path tricks in 'file' are rejected");
    auto wrong = juce::JSON::parse(R"({"schema_version":1,"source":"lead vocal","stage":"mixing","chain":[
        {"module":"plate","params":{},"file":"A2.nam"}],"explanation":"x"})");
    check(! v.validateChain(wrong, "normal").ok(), "'file' on a module that loads none is rejected");

    ChainEngine engine;
    juce::String note;
    engine.onMessage = [&](const juce::String& s) { note = s; };
    engine.prepare(sr, block);
    engine.setChain(r.chain);
    juce::AudioBuffer<float> buf(2, block);
    bool finite = true;
    for (int b = 0; b < 150; ++b)
    {
        fillTestSignal(buf, b * block, sr);
        engine.process(buf);
        finite &= std::isfinite(buf.getMagnitude(0, block));
        if (b == 50)
        {   // swap the capture mid-stream
            auto next = r.chain;
            next.slots[0].file = "wavenet_a1_standard.nam";
            engine.setChain(next);
        }
        if (b % 20 == 0) engine.collectGarbage();
    }
    check(finite && engine.numModules() == 3, "engine swaps a capture mid-stream without glitches or crashes");
    tmp.deleteRecursively();
}

static void testDelay(const Catalogue& cat)
{
    std::cout << "Clean Delay\n";
    const double sr = 48000.0;
    const int total = (int) sr; // one second
    auto run = [&](const char* mode, double feedback, double duck, std::vector<float>& L, std::vector<float>& R, bool loudInput = false)
    {
        auto m = createModule(*cat.find("delay"));
        m->prepare(sr, 512);
        m->setParam("time_ms", 100.0);
        m->setParam("mode", mode);
        m->setParam("feedback", feedback);
        m->setParam("duck", duck);
        m->setParam("mix", 50.0);
        m->setParam("low_cut", 20.0);
        m->setParam("high_cut", 20000.0);
        // let the parameter smoothing settle before the test impulse
        std::vector<float> a(512, 0.0f), b(512, 0.0f);
        for (int i = 0; i < 300; ++i) { float* c[2] = { a.data(), b.data() }; m->process(c, 512); }
        L.assign((size_t) total, 0.0f);
        R.assign((size_t) total, 0.0f);
        if (loudInput)
            for (int i = 0; i < total; ++i) L[(size_t) i] = R[(size_t) i] = 0.5f * std::sin(0.05f * (float) i);
        else
            L[0] = R[0] = 1.0f;
        for (int s = 0; s < total; s += 512)
        {
            const int n = juce::jmin(512, total - s);
            float* c[2] = { L.data() + s, R.data() + s };
            m->process(c, n);
        }
    };
    const int t = (int) (0.1 * sr);
    std::vector<float> L, R;
    run("Stereo", 50.0, 0.0, L, R);
    check(std::abs(L[0] - 1.0f) < 1.0e-4f, "dry impulse passes at unity");
    check(std::abs(L[(size_t) t] - 0.5f) < 0.02f, "first echo lands at 100 ms (" + juce::String(L[(size_t) t], 3) + ")");
    check(std::abs(L[(size_t) (2 * t)] - 0.25f) < 0.05f, "second echo is about half of the first at 50% feedback (the feedback filters smear a single-sample click (" + juce::String(L[(size_t) (2 * t)], 3) + ")");
    check(std::abs(R[(size_t) t] - 0.5f) < 0.02f, "stereo mode repeats on the same side");

    run("Ping-pong", 50.0, 0.0, L, R);
    check(std::abs(L[(size_t) t] - 0.5f * 1.0f) < 0.03f && std::abs(R[(size_t) t]) < 0.02f, "ping-pong first echo is on the left only");
    check(std::abs(R[(size_t) (2 * t)]) > 0.1f && std::abs(L[(size_t) (2 * t)]) < 0.02f, "ping-pong second echo is on the right only");

    run("Stereo", 90.0, 0.0, L, R, true);
    bool finite = true; float peak = 0;
    for (float x : L) { finite &= std::isfinite(x); peak = juce::jmax(peak, std::abs(x)); }
    check(finite && peak < 6.0f, "high feedback on a sustained tone stays bounded (peak " + juce::String(peak, 2) + ")");

    std::vector<float> L0, R0, L1, R1;
    run("Stereo", 40.0, 0.0, L0, R0, true);
    run("Stereo", 40.0, 100.0, L1, R1, true);
    double e0 = 0, e1 = 0;
    for (int i = total / 2; i < total; ++i) { e0 += std::abs(L0[(size_t) i]); e1 += std::abs(L1[(size_t) i]); }
    double dry = 0;
    for (int i = total / 2; i < total; ++i) dry += std::abs(0.5f * std::sin(0.05f * (float) i));
    check((e1 - dry) < 0.6 * (e0 - dry), "ducking cuts the echoes while the input plays");
}

static void testTapeEcho(const Catalogue& cat)
{
    std::cout << "Tape Echo\n";
    const double sr = 48000.0;
    const int block = 256, total = (int) sr * 2;
    auto run = [&](double mix, double intensity, std::vector<float>& out, double& cpuPct, bool burst)
    {
        auto m = createModule(*cat.find("tape_echo"));
        m->prepare(sr, block);
        m->setParam("mode", "Head 1");
        m->setParam("repeat_rate", 100.0);
        m->setParam("intensity", intensity);
        m->setParam("echo_level", 100.0);
        m->setParam("mix", mix);
        out.assign((size_t) total, 0.0f);
        double seconds = 0;
        for (int s = 0; s < total; s += block)
        {
            std::vector<float> l((size_t) block, 0.0f), r((size_t) block, 0.0f);
            for (int i = 0; i < block; ++i)
            {
                const int n = s + i;
                if (burst && n < 2400) l[(size_t) i] = r[(size_t) i] = 0.4f * std::sin(0.08f * (float) n);
                else if (! burst) l[(size_t) i] = r[(size_t) i] = 0.3f * std::sin(0.03f * (float) n);
            }
            float* c[2] = { l.data(), r.data() };
            auto t0 = std::chrono::high_resolution_clock::now();
            m->process(c, block);
            seconds += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < block; ++i) out[(size_t) (s + i)] = l[(size_t) i];
        }
        cpuPct = 100.0 * seconds / 2.0;
    };
    std::vector<float> dry, wet;
    double cpu0 = 0, cpu1 = 0;
    run(0.0, 30.0, dry, cpu0, true);
    float dryPeak = 0, tailDry = 0;
    for (int i = 0; i < total; ++i) { dryPeak = juce::jmax(dryPeak, std::abs(dry[(size_t) i])); if (i > 12000) tailDry = juce::jmax(tailDry, std::abs(dry[(size_t) i])); }
    check(dryPeak > 0.3f && tailDry < 1.0e-3f, "mix 0 passes only the dry signal (peak " + juce::String(dryPeak, 2) + ")");
    run(50.0, 30.0, wet, cpu1, true);
    float tail = 0;
    bool finite = true;
    for (int i = 0; i < total; ++i) { finite &= std::isfinite(wet[(size_t) i]); if (i > 4000 && i < 24000) tail = juce::jmax(tail, std::abs(wet[(size_t) i])); }
    check(finite && tail > 0.02f, "echoes appear after the burst (peak in tail " + juce::String(tail, 3) + ")");
    std::vector<float> loud; double cpu2 = 0;
    run(50.0, 100.0, loud, cpu2, false);
    float peak = 0; bool fin2 = true;
    for (float x : loud) { fin2 &= std::isfinite(x); peak = juce::jmax(peak, std::abs(x)); }
    check(fin2 && peak < 20.0f, "maximum intensity stays finite (peak " + juce::String(peak, 2) + ")");
    std::cout << "  Tape Echo CPU: " << cpu1 << "% of one core at 48 kHz\n";
}

static void testAirwindowsAdditions(const Catalogue& cat)
{
    std::cout << "ButterComp2 and Console8\n";
    const double sr = 48000.0;
    const int block = 256, total = (int) sr;
    struct Result { double rms = 0, peak = 0, cpu = 0; bool finite = true; };
    auto run = [&](const char* id, const std::vector<std::pair<const char*, juce::var>>& params, float amp) {
        auto m = createModule(*cat.find(id));
        m->prepare(sr, block);
        for (auto& p : params) m->setParam(p.first, p.second);
        Result r;
        double sum = 0; int count = 0, secondHalf = total / 2;
        double seconds = 0;
        for (int s = 0; s < total; s += block)
        {
            std::vector<float> l((size_t) block), rr((size_t) block);
            for (int i = 0; i < block; ++i) l[(size_t) i] = rr[(size_t) i] = amp * std::sin(0.0587f * (float) (s + i));
            float* c[2] = { l.data(), rr.data() };
            auto t0 = std::chrono::high_resolution_clock::now();
            m->process(c, block);
            seconds += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < block; ++i)
            {
                r.finite &= std::isfinite(l[(size_t) i]);
                r.peak = juce::jmax(r.peak, (double) std::abs(l[(size_t) i]));
                if (s + i >= secondHalf) { sum += (double) l[(size_t) i] * l[(size_t) i]; ++count; }
            }
        }
        r.rms = std::sqrt(sum / juce::jmax(1, count));
        r.cpu = 100.0 * seconds;
        return r;
    };
    const double inRms = 0.3 / std::sqrt(2.0);
    auto clean = run("console8", { { "drive", 0.0 }, { "tone", "Solid" } }, 0.3f);
    check(clean.finite && std::abs(juce::Decibels::gainToDecibels(clean.rms / inRms)) < 1.5,
          "Console8 at zero drive stays near unity (" + juce::String(juce::Decibels::gainToDecibels(clean.rms / inRms), 2) + " dB)");
    auto driven = run("console8", { { "drive", 100.0 }, { "tone", "Solid" } }, 0.3f);
    check(driven.finite && driven.peak < 1.5 && driven.rms > clean.rms, "Console8 drive adds level and stays bounded (peak " + juce::String(driven.peak, 2) + ")");
    auto hype = run("console8", { { "drive", 0.0 }, { "tone", "Hype" } }, 0.3f);
    check(hype.finite && std::abs(hype.rms - clean.rms) > 1.0e-6, "Hype tone differs from Solid");
    std::cout << "  Console8 CPU: " << clean.cpu << "% of one core at 48 kHz\n";

    auto off = run("buttercomp2", { { "compress", 0.0 }, { "output", 0.5 }, { "dry_wet", 1.0 } }, 0.5f);
    auto comp = run("buttercomp2", { { "compress", 0.7 }, { "output", 0.5 }, { "dry_wet", 1.0 } }, 0.5f);
    check(off.finite && comp.finite && std::abs(juce::Decibels::gainToDecibels(off.rms / (0.5 / std::sqrt(2.0)))) < 1.0,
          "ButterComp2 with Compress 0 passes the signal unchanged");
    check(comp.finite && std::abs(comp.rms - off.rms) > 1.0e-3, "ButterComp2 changes the level when Compress is raised (" + juce::String(juce::Decibels::gainToDecibels(comp.rms / off.rms), 2) + " dB)");
    std::cout << "  ButterComp2 CPU: " << comp.cpu << "% of one core at 48 kHz\n";
}

static void testWideners(const Catalogue& cat)
{
    std::cout << "Stereo wideners\n";
    const double sr = 48000.0;
    const int block = 256, total = (int) sr;
    // Mostly-centre material with a little different content on each side.
    auto sideOverMid = [&](const char* id, const std::vector<std::pair<const char*, juce::var>>& params, double& cpu, bool& finite) {
        auto m = createModule(*cat.find(id));
        m->prepare(sr, block);
        for (auto& p : params) m->setParam(p.first, p.second);
        double mid = 0, side = 0, seconds = 0;
        for (int s = 0; s < total; s += block)
        {
            std::vector<float> l((size_t) block), r((size_t) block);
            for (int i = 0; i < block; ++i)
            {
                const float n = (float) (s + i);
                const float c = 0.3f * std::sin(0.0587f * n) + 0.1f * std::sin(0.31f * n);
                l[(size_t) i] = c + 0.06f * std::sin(0.77f * n);
                r[(size_t) i] = c + 0.06f * std::sin(1.13f * n);
            }
            float* ch[2] = { l.data(), r.data() };
            auto t0 = std::chrono::high_resolution_clock::now();
            m->process(ch, block);
            seconds += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < block; ++i)
            {
                finite &= std::isfinite(l[(size_t) i]) && std::isfinite(r[(size_t) i]);
                if (s >= total / 2) { const double M = 0.5 * (l[(size_t) i] + r[(size_t) i]), S = 0.5 * (l[(size_t) i] - r[(size_t) i]); mid += M * M; side += S * S; }
            }
        }
        cpu = 100.0 * seconds;
        return std::sqrt(side / juce::jmax(1.0e-12, mid));
    };
    double cpu = 0; bool finite = true;
    const double base = sideOverMid("srsly2", { { "space", 0.0 }, { "center", 0.0 }, { "dry_wet", 0.0 } }, cpu, finite);
    const double wide = sideOverMid("srsly2", { { "space", 0.9 }, { "center", 0.5 }, { "dry_wet", 1.0 } }, cpu, finite);
    check(finite && wide > base * 1.3, "Srsly2 widens (side/mid " + juce::String(base, 3) + " -> " + juce::String(wide, 3) + ")");
    std::cout << "  Srsly2 CPU: " << cpu << "% of one core at 48 kHz\n";
    const double sw = sideOverMid("sweetwide", { { "un_wide", 0.5 }, { "soar", 0.0 } }, cpu, finite);
    const double swWide = sideOverMid("sweetwide", { { "un_wide", 0.9 }, { "soar", 0.3 } }, cpu, finite);
    const double swNarrow = sideOverMid("sweetwide", { { "un_wide", 0.1 }, { "soar", 0.0 } }, cpu, finite);
    check(finite && swWide > swNarrow, "SweetWide widens and narrows (narrow " + juce::String(swNarrow, 3) + ", neutral " + juce::String(sw, 3) + ", wide " + juce::String(swWide, 3) + ")");
    std::cout << "  SweetWide CPU: " << cpu << "% of one core at 48 kHz\n";
}

static void testGuitarix(const Catalogue& cat)
{
    std::cout << "Guitarix amps\n";
    const double sr = 48000.0;
    const int block = 256, total = (int) sr;
    struct Result { double rms = 0, peak = 0, cpu = 0; bool finite = true; };
    // Guitar-like DI: a plucked low E chord-ish mix with a decaying envelope, same on both channels.
    auto run = [&](const char* id, const std::vector<std::pair<const char*, juce::var>>& params) {
        Result r;
        auto m = createModule(*cat.find(id));
        m->prepare(sr, block);
        for (auto& p : params) m->setParam(p.first, p.second);
        double sum = 0, seconds = 0;
        for (int s = 0; s < total; s += block)
        {
            std::vector<float> l((size_t) block), rr((size_t) block);
            for (int i = 0; i < block; ++i)
            {
                const double t = (double) (s + i) / sr;
                const float env = (float) std::exp(-3.0 * std::fmod(t, 0.5));
                const float x = env * (0.25f * std::sin(6.2831853f * 82.4f * (float) t) + 0.12f * std::sin(6.2831853f * 164.8f * (float) t) + 0.08f * std::sin(6.2831853f * 247.2f * (float) t));
                l[(size_t) i] = rr[(size_t) i] = x;
            }
            float* ch[2] = { l.data(), rr.data() };
            auto t0 = std::chrono::high_resolution_clock::now();
            m->process(ch, block);
            seconds += std::chrono::duration<double>(std::chrono::high_resolution_clock::now() - t0).count();
            for (int i = 0; i < block; ++i)
            {
                r.finite &= std::isfinite(l[(size_t) i]) && std::isfinite(rr[(size_t) i]);
                r.peak = juce::jmax(r.peak, (double) std::abs(l[(size_t) i]), (double) std::abs(rr[(size_t) i]));
                if (s >= total / 2) sum += (double) l[(size_t) i] * l[(size_t) i];
            }
        }
        r.rms = std::sqrt(sum / (total / 2));
        r.cpu = 100.0 * seconds;
        return r;
    };
    for (const char* id : { "gx_supersonic", "gx_blueamp", "gx_plexi", "gx_guvnor", "gx_ultracab" })
    {
        const auto r = run(id, {});
        check(r.finite && r.rms > 1.0e-4 && r.peak < 8.0, juce::String(id) + " renders audio at defaults (rms " + juce::String(r.rms, 4) + ", peak " + juce::String(r.peak, 3) + ")");
        std::cout << "  " << id << " CPU: " << r.cpu << "% of one core at 48 kHz\n";
    }
    const auto sLow = run("gx_supersonic", { { "gain", 0.1 } }), sHigh = run("gx_supersonic", { { "gain", 0.8 } });
    check(sLow.finite && sHigh.finite && std::abs(sHigh.rms - sLow.rms) > 1.0e-3, "Supersonic gain changes the sound (rms " + juce::String(sLow.rms, 4) + " -> " + juce::String(sHigh.rms, 4) + ")");
    const auto pLow = run("gx_plexi", { { "volume", 0.1 } }), pHigh = run("gx_plexi", { { "volume", 0.7 } });
    check(pLow.finite && pHigh.finite && pHigh.rms > pLow.rms, "Plexi volume raises the level (rms " + juce::String(pLow.rms, 4) + " -> " + juce::String(pHigh.rms, 4) + ")");
    const auto cNar = run("gx_ultracab", { { "size", 0.1 } }), cWide = run("gx_ultracab", { { "size", 0.9 } });
    check(cNar.finite && cWide.finite && std::abs(cNar.rms - cWide.rms) > 1.0e-4, "UltraCab size changes the sound");

    // The AI is told about the Guitarix amps, not the Neural Amp Modeler or the cabinet IR loader.
    const auto prompt = cat.toPromptJson();
    check(prompt.contains("\"gx_plexi\"") && ! prompt.contains("\"nam_amp\"") && ! prompt.contains("\"cab_ir\""),
          "prompt offers gx_plexi and leaves out nam_amp and cab_ir");
    check(cat.find("nam_amp") != nullptr && cat.find("cab_ir") != nullptr, "nam_amp and cab_ir remain runnable");
}

static void testReplyParsing(const Catalogue& cat)
{
    std::cout << "AI reply parsing\n";
    AiSession s(cat);
    ChainState none;
    auto fenced = s.interpret("```json\n{\"schema_version\":1,\"source\":\"bass\",\"stage\":\"mixing\",\"chain\":[{\"module\":\"tape\",\"params\":{\"drive\":3}}],\"explanation\":\"Tape for weight.\"}\n```", none, "normal");
    check(fenced.kind == AiOutcome::Kind::Chain, "fenced JSON chain accepted");
    auto q = s.interpret("{\"schema_version\":1,\"question\":\"Is this a DI or an amp recording?\"}", none, "normal");
    check(q.kind == AiOutcome::Kind::Question, "clarifying question recognised");
    auto bad = s.interpret("{\"schema_version\":1,\"source\":\"x\",\"stage\":\"mixing\",\"chain\":[{\"module\":\"tape\",\"params\":{\"drive\":40}}],\"explanation\":\"\"}", none, "normal");
    check(bad.kind == AiOutcome::Kind::Error && bad.message.contains("outside"), "out-of-range value rejected with a usable message");
    auto prose = s.interpret("Sure! Here you go: {\"schema_version\":1,\"source\":\"x\",\"stage\":\"tracking\",\"chain\":[{\"module\":\"limiter\",\"params\":{}}],\"explanation\":\"\"} hope that helps", none, "normal");
    check(prose.kind == AiOutcome::Kind::Error && prose.message.contains("tracking"), "JSON found inside prose, tracking rule enforced");
}

// Needs tests/mock_tone3000.py running and PLUGSALOT_TONE3000_BASE pointing at it.
static void testTone3000(const Catalogue& cat)
{
    if (juce::SystemStats::getEnvironmentVariable("PLUGSALOT_TONE3000_BASE", {}).isEmpty())
    {
        std::cout << "TONE3000 test skipped (start plugin/tests/mock_tone3000.py and set PLUGSALOT_TONE3000_BASE)\n";
        return;
    }
    std::cout << "TONE3000 client (against the stand-in server)\n";
    auto tmp = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("plugsalot-t3k-lib");
    tmp.deleteRecursively();
    tmp.createDirectory();
#if JUCE_WINDOWS
    _putenv_s("PLUGSALOT_LIBRARY", tmp.getFullPathName().toRawUTF8());
    _putenv_s("PLUGSALOT_TONE3000_CLIENT_ID", "pk_test");
#else
    setenv("PLUGSALOT_LIBRARY", tmp.getFullPathName().toRawUTF8(), 1);
    setenv("PLUGSALOT_TONE3000_CLIENT_ID", "pk_test", 1);
#endif
    check(Tone3000::configured(), "client id is picked up");

    Tone3000 t3k;
    int tokenEvents = 0;
    t3k.onTokensChanged = [&](const Tone3000::Tokens&) { ++tokenEvents; };
    std::vector<Tone3000::Tone> tones;
    int pages = 0;
    check(! t3k.list("trending", {}, {}, 1, tones, pages).ok, "lists need a sign-in first");

    Tone3000::DeviceCode code;
    auto r = t3k.requestDeviceCode(code);
    check(r.ok && code.userCode == "ABCD-1234" && code.verificationUriComplete.contains("activate"), "device code requested: " + code.userCode);
    std::atomic<bool> cancel { false };
    r = t3k.waitForApproval(code, cancel);
    check(r.ok && t3k.signedIn() && tokenEvents == 1, "sign-in completes after the user approves (pending is polled through)");

    r = t3k.list("trending", {}, {}, 1, tones, pages);
    check(r.ok && tones.size() == 2 && tones[0].title == "Plexi Crunch" && tones[0].creator == "tonesmith"
              && tones[0].avatarUrl.isNotEmpty() && tones[0].license == "cc-by" && tones[1].creator == "Pedal Head",
          "trending list parsed with creator name, avatar and licence");
    r = t3k.list("favorited", {}, "plexi", 1, tones, pages);
    check(r.ok && tones.size() == 1, "favorites search by title");
    r = t3k.list("downloaded", "pedal", {}, 1, tones, pages);
    check(r.ok && tones.size() == 1 && tones[0].gear == "pedal", "gear filter");

    std::vector<Tone3000::Model> models;
    r = t3k.models("101", models);
    check(r.ok && models.size() == 2 && models[0].architecture == "A2" && models[1].architecture == "A1", "models listed with architectures");

    auto tk = t3k.tokens();
    tk.expiresAtMs = 0;
    Tone3000 expired(tk);
    expired.onTokensChanged = [&](const Tone3000::Tokens&) { ++tokenEvents; };
    r = expired.list("latest", {}, {}, 1, tones, pages);
    check(r.ok && tokenEvents == 2, "an expired access token is refreshed automatically");

    auto dest = Library::newFileFor("nam", "Plexi Crunch - standard (tonesmith)");
    r = t3k.download(models[0], dest);
    check(r.ok && dest.existsAsFile() && dest.getSize() > 100, "model downloads into the library");
    check(Library::list("nam").contains(dest.getFileName()), "downloaded model shows in the library list");
    check(Library::newFileFor("nam", "../../evil/..\\x").getParentDirectory() == Library::folder("nam"), "download names cannot leave the library folder");

    auto m = createModule(*cat.find("nam_amp"));
    m->prepare(48000.0, 256);
    const auto msg = m->loadFile(dest);
    check(msg.isEmpty() || msg.contains("trained at"), "the downloaded capture loads in NAM Amp");

    Tone3000 bad(Tone3000::Tokens{ "old", "wrong-refresh", 0 });
    r = bad.list("trending", {}, {}, 1, tones, pages);
    check(! r.ok && ! bad.signedIn() && r.error.contains("sign in"), "a dead refresh token signs the user out with a clear message");

    t3k.signOut();
    check(! t3k.signedIn(), "sign-out clears the tokens");
    tmp.deleteRecursively();
}

static void testLive(const Catalogue& cat)
{
    ProviderSettings ps;
    auto gemini = juce::SystemStats::getEnvironmentVariable("PLUGSALOT_GEMINI_KEY", {});
    auto claude = juce::SystemStats::getEnvironmentVariable("PLUGSALOT_CLAUDE_KEY", {});
    if (gemini.isEmpty() && claude.isEmpty())
    {
        std::cout << "Live AI test skipped (set PLUGSALOT_GEMINI_KEY or PLUGSALOT_CLAUDE_KEY)\n";
        return;
    }
    std::cout << "Live AI test\n";
    ps.provider = gemini.isNotEmpty() ? ProviderSettings::Provider::Gemini : ProviderSettings::Provider::Claude;
    ps.apiKey = gemini.isNotEmpty() ? gemini : claude;
    AiSession s(cat);
    std::vector<ChatTurn> turns;
    SessionContext ctx;
    auto out = s.runExchange(ps, ctx, {}, {}, "Warm, intimate pop vocal recorded on a condenser mic.", turns);
    check(out.kind == AiOutcome::Kind::Chain, "live request returns a valid chain (model " + out.model + (out.retried ? ", after retry" : "") + ")");
    std::cout << "    " << out.message << "\n";
    if (out.kind == AiOutcome::Kind::Chain)
        std::cout << out.chain.toJson() << "\n";
}

int main(int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;  // message manager for AiSession
    const auto& cat = Catalogue::embedded();
    juce::File examples(argc > 1 ? juce::String(argv[1]) : juce::String(PLUGSALOT_SOURCE_DIR "/catalogue/examples"));

    testCatalogue(cat);
    testValidatorParity(cat, examples);
    testModules(cat);
    testTrackingLatency(cat);
    testEngine(cat);
    testFileModules(cat, juce::File(PLUGSALOT_SOURCE_DIR "/plugin/vendor/nam-core/example_models"));
    testReplyParsing(cat);
    testDelay(cat);
    testTapeEcho(cat);
    testAirwindowsAdditions(cat);
    testWideners(cat);
    testGuitarix(cat);
    testTone3000(cat);
    testLive(cat);

    std::cout << (failures == 0 ? "\nAll tests passed\n" : "\n" + std::to_string(failures) + " test(s) failed\n");
    return failures == 0 ? 0 : 1;
}
