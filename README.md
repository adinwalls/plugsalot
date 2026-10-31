# Plugsalot

An open-source, cross-platform audio plugin with a built-in AI chat that builds and tunes processing chains: tell it what you're recording and the sound you want, and it sets up EQ, compression, de-essing, saturation, reverb and limiting for you.

Plugsalot is **GPL-3.0-or-later**. Its processing modules are ported from high-quality open-source projects; see [THIRD_PARTY.md](THIRD_PARTY.md).

> Status: **0.2 prototype.** A working plugin (VST3, AU, LV2, standalone) with the AI chat, eight modules ported from open-source DSP (including Neural Amp Modeler amps and cabinet impulse responses, with TONE3000 browsing), and the module catalogue it reads. The interface is a functional placeholder. To try it, see [docs/TESTING.md](docs/TESTING.md).

## The plugin

```
plugin/
  src/core/        catalogue loader, chain data, validator (C++ port of tools/chain_validator.py)
  src/dsp/         chain engine (lock-free chain swaps), module wrappers, input analyser
  src/ai/          system prompt, Gemini and Claude clients, validated exchange with one retry
  src/ui/          the vintage-rack interface (RackPanel, Vintage painters, chat, settings)
  resources/       guidelines.md: the AI's instructions (layer 3 of the AI design)
  vendor/          upstream DSP copied unmodified by tools/vendor_sources.py
  shims/           tiny stand-ins for the frameworks upstream code expects (DPF, VST2 SDK)
  tests/           headless tests: validator parity, every module on test audio, engine swaps
```

How a request flows: dropdowns + your words + the input analysis -> system prompt (guidelines + catalogue) -> Gemini or Claude -> reply JSON -> validator (hard limits; one retry with the errors) -> chain engine swaps the chain in without stopping audio. Follow-ups come back as small change lists, and every change can be undone.

AI providers: a free Google Gemini key (default; the plugin picks the newest Flash model automatically) or your own Claude API key. The hosted Patreon tier is designed in the AI Service Design doc but not built yet.

### Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
build/plugin/PlugsalotTests_artefacts/Release/PlugsalotTests   # headless tests
```

Linux needs: `libasound2-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libfreetype-dev libfontconfig1-dev libcurl4-openssl-dev libgl1-mesa-dev`. GitHub Actions builds Linux, Windows and macOS on every push (`.github/workflows/build.yml`).

JUCE 8 is used under the AGPLv3, which GPLv3 explicitly allows combining with (GPLv3 section 13).

## How the catalogue works

The AI never guesses at plugin settings. It reads a catalogue describing every module (what it's for, its parameters, their safe ranges and units) and replies with a chain as JSON, which a validator checks before anything touches audio.

```
third_party/sources.json      pinned upstream repos and their licences
        │  tools/fetch_sources.py
        ▼
.cache/upstream/<source>/     upstream code at the pinned commit (git-ignored)
        │  tools/extract_all.py
        ▼
catalogue/raw/<id>.json       everything upstream declares, machine-extracted
        │  + catalogue/curation/<id>.json   (human judgement: purpose, hints, what the AI sees)
        │  tools/build_catalogue.py
        ▼
catalogue/modules/<id>.json   final entries the AI reads
catalogue/index.json          short summary of every module
THIRD_PARTY.md                generated credits and licences
        │  tools/validate.py
        ▼
schema checks + example chains in catalogue/examples/
```

Curation can only **narrow** what upstream allows. If a bumped upstream commit changes a range so a curated limit no longer fits, the build fails rather than shipping a wrong value.

### Parameter extractors

| Extractor | Source format | Used for |
| --- | --- | --- |
| `juce_apvts` | JUCE `AudioProcessorValueTreeState` layouts | Dusk Audio JUCE plugins |
| `dusk_daf` | Dusk DAF descriptor tables | TapeMachine 2, 4K EQ, Multi-Q, Tape Echo 2 |
| `dpf_param_table` | DPF `PARAMS[]` tables + presets | Dragonfly Reverb |
| `lv2_ttl` | LV2 Turtle port lists | x42 and other LV2 plugins |
| `airwindows` | Airwindows effect sources + Airwindopedia | all ~500 Airwindows effects |
| `lsp_para_eq` | LSP port macros | LSP Parametric Equalizer |
| `manual` | written in the curation file | library APIs such as sfizz |

## Quick start

```bash
pip install -r requirements.txt
python3 tools/fetch_sources.py     # clone upstreams at their pinned commits
python3 tools/extract_all.py       # extract raw parameters
python3 tools/build_catalogue.py   # merge with curation
python3 tools/validate.py          # schema + example chains
python3 tools/vendor_sources.py    # refresh the DSP copied into plugin/vendor
```

## Adding a module

1. Add or reuse its upstream in `third_party/sources.json` (repo, commit, licence). Only GPL-3-compatible licences.
2. Create `catalogue/curation/<id>.json`: source, extractor, purpose, `good_for`, and the `expose` list of parameters the AI may set, with hints.
3. Run the four commands above and review `catalogue/raw/<id>.json` for anything marked `unresolved`.
4. Add an example chain using it to `catalogue/examples/`.

## Pilot modules

| Module | Upstream | Licence |
| --- | --- | --- |
| `console_eq` | Dusk Audio 4K EQ 2 | GPL-3.0 |
| `para_eq` | LSP Parametric Equalizer (catalogue only) | LGPL-3.0 |
| `multicomp` | Dusk Audio Multi-Comp | GPL-3.0 |
| `deess` | Airwindows DeBess | MIT |
| `tape` | Dusk Audio TapeMachine 2 | GPL-3.0 |
| `plate` | Dragonfly Plate Reverb | GPL-3.0 |
| `limiter` | x42 Digital Peak Limiter | GPL-3.0 |
| `gx_blueamp`, `gx_plexi`, `gx_supersonic`, `gx_guvnor`, `gx_ultracab` | Guitarix GxPlugins.lv2 (BlueAmp, Plexi, Supersonic, Guvnor, UltraCab; knobs, no files) | GPL-2.0-or-later |
| `nam_amp` | Neural Amp Modeler Core (runs, but the AI no longer offers it) | MIT |
| `cab_ir` | AudioDSPTools ImpulseResponse | MIT |
| `delay` | Plugsalot Clean Delay (original code) | GPL-3.0 |
| `tape_echo` | Dusk Audio Tape Echo 2 | GPL-3.0 |
| `buttercomp2` | Airwindows ButterComp2 | MIT |
| `srsly2` | Airwindows Srsly2 | MIT |
| `sweetwide` | Airwindows SweetWide | MIT |
| `console8` | Airwindows Console8 (channel In/Hype + Out) | MIT |
| `drum_sampler` | sfizz (catalogue only) | BSD-2-Clause |

## Not done yet

- The LSP Parametric EQ and sfizz drum sampler in the plugin (they are catalogued; `console_eq` is the prototype's EQ).
- Preset extraction for TapeMachine 2 and Multi-Comp's factory presets.
- Multiband mode of Multi-Comp as its own entry.
- Hosted AI for Patreon supporters, API keys in the system keychain, latency-matched bypass, reverb on a send.
- The real interface design.
