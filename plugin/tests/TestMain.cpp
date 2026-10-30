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
    check(cat.modules().size() == 9, "nine runnable modules embedded (" + juce::String((int) cat.modules().size()) + ")");
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
    testTone3000(cat);
    testLive(cat);

    std::cout << (failures == 0 ? "\nAll tests passed\n" : "\n" + std::to_string(failures) + " test(s) failed\n");
    return failures == 0 ? 0 : 1;
}
