# Handover notes (written 2026-10-31, after v0.3.0 UI port)

## State
- `main` at the vintage-rack UI commit (6c22b95, version 0.3.0). CI is green on Linux, Windows and macOS. The user creates release tags themselves (tag pushes are blocked from the build environment): `v0.3.0` is ready to tag, as a **pre-release / alpha** (not "public beta"; it goes to a few musicians).
- 14 runnable modules: console_eq, multicomp, deess, tape, plate, limiter, nam_amp, cab_ir, delay, tape_echo, buttercomp2, console8, srsly2, sweetwide.
- The user records vocals on Linux (Ardour, Pop!_OS) today and will test the electric guitar features.
- TONE3000: no reply yet. The publishable key goes in the GitHub repository variable `TONE3000_CLIENT_ID` (never the secret `t3k_cs_...` key). The user did not want to capture amps: only use and approve existing licensed captures.

## Next job: Guitarix amps instead of NAM (0.3.1)
Decision: use the GxPlugins.lv2 amps/pedals (brummer10, https://github.com/brummer10/GxPlugins.lv2, one repo per plugin) as the guitar path. Knobs not files, no TONE3000, tiny size. Keep the NAM code but stop the AI offering it (guidelines + catalogue) and hide the TONE3000 button until needed.

Spike results (standalone harnesses, clean output, no NaN; not yet listened to):
| Plugin | Ports (all 0..1) | CPU | Notes |
|---|---|---|---|
| GxSupersonic | GAIN .15, BASS .5, TREBLE .5, VOLUME .25 | ~0.6% | mono, namespace `supersonic` |
| GxBlueAmp | MASTER .5, TONE .5, VOLUME .5 | ~0.3% | needs `mydsp_faustpower2_f/3_f` helper templates |
| GxGuvnor | BASS, GAIN, LEVEL, MID, TREBLE (all .5) | ~1% | overdrive pedal, no bypass port |
| GxUltraCab | GAIN, MIDS, PUNCH, RESONANCE, SIZE_, TOP (all .5) | ~0.6% | **stereo** (`stereo_audio`), built-in cab, no IR file |
| GxPlexi | BASS .5, MASTER .4, MID .5, PRESENSE .15, TREBLE .5, VOLUME .15 | not run | needs **zita-resampler** (not vendored; GPL-3.0-or-later per Arch package; vendor it with the licence header and credit) |

How the DSP is used (same idea as the Airwindows wrappers):
- Each `dsp/<name>.cc` is Faust-generated: `class Dsp : public PluginLV2` inside `namespace <name>`, created by `<name>::plugin()`; set ports with `connect_ports(portIndex, float*, p)`, then `set_samplerate(sr, p)`, optional `activate_plugin(true, p)`, then `mono_audio(n, in, out, p)` (or `stereo_audio(n, inL, inR, outL, outR, p)` for UltraCab).
- The wrapper TU must define before including the `.cc`: `FAUSTFLOAT float`, `__rt_func`/`__rt_data` (empty), `always_inline`, `N_(x)`, `faustpower<N>` templates, the per-plugin `PortIndex` enum (copy from the plugin's `plugin/gx_<name>.h`) at **global** scope, and `gx_pluginlv2.h`. `min`/`max` macros clash with the standard library: include all std headers first and avoid `std::min/max` in that TU.
- Each plugin needs its own translation unit (same-named symbols/namespaces); tables are `*_table.h` headers beside the `.cc`.
- Guitar is mono: sum stereo to mono, process, copy to both channels (as the NAM module does), then UltraCab stereo.
- Licences: source headers say GPL-2.0-or-later (repo LICENSE files are GPL-3); compatible with Plugsalot's GPL-3.0-or-later. Credit in THIRD_PARTY.md.

To do:
1. Add the five repos to `third_party/sources.json` (pin commits), `tools/fetch_sources.py`/`vendor_sources.py`; add zita-resampler.
2. Write module wrappers (`plugin/src/dsp/modules/`), add to `PLUGSALOT_MODULES` and engine sources in `plugin/CMakeLists.txt`, factory in `ModuleFactory.cpp`.
3. Curation JSON in `catalogue/curation/` (with `maker`, `tape`, `knobs` colours), run `tools/build_catalogue.py` and `tools/validate.py`.
4. Update `plugin/resources/guidelines.md`: Guitarix amps are the guitar path (BlueAmp clean/blues, Plexi classic rock, Supersonic high gain, Guvnor overdrive pedal before an amp, UltraCab after the amp); NAM no longer offered.
5. Tests in `plugin/tests/TestMain.cpp` (module count, render sanity, CPU), README module table, docs/TESTING.md.
6. Commit and push once, check CI once, tell the user to create the tag.

## Working notes
- Build/test: `cmake -S . -B build && cmake --build build -j8`; test runner `PlugsalotTests`.
- Be economical: one commit per job, check CI once, avoid polling and screenshots unless the UI changes.
- The Design prototype artifact for the UI is https://claude.ai/artifact/X8z1XUBd44vtk4WxhyJXJB.
