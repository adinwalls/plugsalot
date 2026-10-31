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
