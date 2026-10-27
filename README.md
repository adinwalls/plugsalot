# Plugsalot

An open-source, cross-platform audio plugin with a built-in AI chat that builds and tunes processing chains: tell it what you're recording and the sound you want, and it sets up EQ, compression, de-essing, saturation, reverb and limiting for you.

Plugsalot is **GPL-3.0-or-later**. Its processing modules are ported from high-quality open-source projects; see [THIRD_PARTY.md](THIRD_PARTY.md).

> Status: early. This repo currently holds the **module catalogue**, the database the AI reads, and the tools that build it. The plugin itself comes next.

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
```

## Adding a module

1. Add or reuse its upstream in `third_party/sources.json` (repo, commit, licence). Only GPL-3-compatible licences.
2. Create `catalogue/curation/<id>.json`: source, extractor, purpose, `good_for`, and the `expose` list of parameters the AI may set, with hints.
3. Run the four commands above and review `catalogue/raw/<id>.json` for anything marked `unresolved`.
4. Add an example chain using it to `catalogue/examples/`.

## Pilot modules

| Module | Upstream | Licence |
| --- | --- | --- |
| `para_eq` | LSP Parametric Equalizer | LGPL-3.0 |
| `multicomp` | Dusk Audio Multi-Comp | GPL-3.0 |
| `deess` | Airwindows DeBess | MIT |
| `tape` | Dusk Audio TapeMachine 2 | GPL-3.0 |
| `plate` | Dragonfly Plate Reverb | GPL-3.0 |
| `limiter` | x42 Digital Peak Limiter | GPL-3.0 |
| `drum_sampler` | sfizz | BSD-2-Clause |

## Not done yet

- CPU cost per module (`cpu_points` is `null` until a benchmark harness measures it; the validator assumes 2 points each meanwhile).
- Latency measurement for TapeMachine 2.
- Preset extraction for TapeMachine 2 and Multi-Comp's factory presets.
- Multiband mode of Multi-Comp as its own entry.
- The plugin, the AI chat, and the C++ port of `tools/chain_validator.py`.
