# Handover notes (written 2026-10-31, after v0.3.0 UI port)

## State
- `main` at the vintage-rack UI commit (6c22b95, version 0.3.0). CI is green on Linux, Windows and macOS. The user creates release tags themselves (tag pushes are blocked from the build environment): `v0.3.0` is ready to tag, as a **pre-release / alpha** (not "public beta"; it goes to a few musicians).
- 14 runnable modules: console_eq, multicomp, deess, tape, plate, limiter, nam_amp, cab_ir, delay, tape_echo, buttercomp2, console8, srsly2, sweetwide.
- The user records vocals on Linux (Ardour, Pop!_OS) today and will test the electric guitar features.
- TONE3000: no reply yet. The publishable key goes in the GitHub repository variable `TONE3000_CLIENT_ID` (never the secret `t3k_cs_...` key). The user did not want to capture amps: only use and approve existing licensed captures.

## Done in 0.3.1: Guitarix amps instead of NAM
Five GxPlugins.lv2 modules are in: `gx_supersonic`, `gx_blueamp`, `gx_plexi`, `gx_guvnor`, `gx_ultracab` (pinned in `third_party/sources.json`, vendored under `plugin/vendor/gx-*`, wrappers in `plugin/src/dsp/modules/Gx*Dsp.cpp` + `GuitarixModule.cpp`). Plexi turned out not to need zita-resampler, so nothing extra is vendored. NAM and cab_ir are marked `"offered": false` in curation (still run, hidden from the AI prompt), the Library line is gone from the prompt, the TONE3000 button handler is commented out in `PluginEditor.cpp`, guidelines point guitar at the Guitarix chain.
Tests pass locally (19 runnable modules; all five render, all under 2% CPU). NOT yet listened to: the user should try each amp with real guitar, especially output levels (volume/master defaults are the plug-in's own) and the UltraCab pairing.
Next: user tags `v0.3.1` (pre-release), listens, then tune default levels in curation JSON and the guidelines.

## Working notes
- Build/test: `cmake -S . -B build && cmake --build build -j8`; test runner `PlugsalotTests`.
- Be economical: one commit per job, check CI once, avoid polling and screenshots unless the UI changes.
- The Design prototype artifact for the UI is https://claude.ai/artifact/X8z1XUBd44vtk4WxhyJXJB.

## 2026-11-12: variety batch
Seven modules added (tube2, density, air2, channel9, chorus_ensemble from Airwindows; hall and room from Dragonfly): 26 runnable modules, tests pass on Linux. See `docs/PLUGIN_REVIEW.md` section 6. `tools/extractors/dpf_param_table.py` now reads Dragonfly preset banks. Hall and Room each build as their own small library because every Dragonfly plug-in uses the same class and header names. Not yet listened to. TuneBrew (Adin's own pitch corrector, `adinwalls/adin-brew` main, header-only engine) is in as `tunebrew`: 27 runnable modules. Next: DuskVerb (framework-free core is in the pinned Dusk repo), and wiring TuneBrew's key detection into the input analysis.

## 2026-11-19: Corrective EQ (para_eq)
The LSP Parametric Equalizer is in as `para_eq` ("Corrective EQ (8-band)"), the clean cleanup EQ that goes before `console_eq` (character). It runs LSP's own `dspu::Filter` per band, per channel, in IIR mode only (zero latency; FIR/FFT/SPM modes, filter-mode and width are hidden). Vendored under `plugin/vendor/lsp-dsp-units`, `lsp-common-lib` and `lsp-dsp-lib` (headers + Filter/FilterBank + generic kernels only; `src/dsp/modules/LspKernels.cpp` binds the few kernels at compile time instead of upstream's CPU dispatch). Wrapper: `ParaEqModule.cpp`; AI guidance: the "Two EQs, two jobs" line in `guidelines.md`; tests: `testParaEq` (gain at the centre of each filter type at 44.1/48/96 kHz, notch depth, eight-band CPU about 0.4% of a core). 28 runnable modules. Not listened to: check the default Q feel (LSP's Q is a damping factor, so Q 0.4 is the flat Hi-pass) and that the AI actually reaches for it on cleanup.
