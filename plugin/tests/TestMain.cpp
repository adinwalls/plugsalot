// Plugsalot - GPL-3.0-or-later
// Headless tests: catalogue, validator parity with tools/chain_validator.py,
// every module on test audio (finite output, latency, CPU cost), chain engine
// swaps, and AI reply parsing. Exit code 0 = all passed.
//
// Optional live AI test: set PLUGSALOT_GEMINI_KEY or PLUGSALOT_CLAUDE_KEY.
#include "ai/AiSession.h"
#include "core/ChainValidator.h"
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
    check(cat.modules().size() == 6, "six runnable modules embedded (" + juce::String((int) cat.modules().size()) + ")");
    for (auto& m : cat.modules())
    {
        bool defaultsOk = true;
        for (auto& p : m.params)
            if (p.check(p.defaultValue).isNotEmpty())
                defaultsOk = false;
        check(defaultsOk && ! m.params.empty() && ! m.rawParams.empty(), m.id + ": params and raw defaults valid");
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
    testReplyParsing(cat);
    testLive(cat);

    std::cout << (failures == 0 ? "\nAll tests passed\n" : "\n" + std::to_string(failures) + " test(s) failed\n");
    return failures == 0 ? 0 : 1;
}
