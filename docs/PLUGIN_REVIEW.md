# Plugin review: what we have and what could add variety

Written 2026-11-12 from `catalogue/index.json`, `THIRD_PARTY.md` and `docs/HANDOVER.md` at commit 7f150b8. Nothing had been added at that point; this was a menu to choose from. See the status section at the end for what has been added since.

## 1. What the app uses today

21 catalogue entries: 20 effects and the drum sampler. Of these, 14 can be used on vocals (marked V).

| Module | Upstream (licence) | Role in chain | Vocals |
| --- | --- | --- | --- |
| `console_eq` | Dusk 4K EQ 2 (GPL-3) | Character EQ: HPF/LPF, 4 bands | V |
| `para_eq` | LSP Parametric EQ x8 (LGPL-3) | Corrective EQ, cuts, HPF | V |
| `deess` | Airwindows DeBess (MIT) | De-esser | V |
| `multicomp` | Dusk Multi-Comp (GPL-3) | Compressor, 7 modes (opto, FET, VCA, bus, studio FET/VCA, digital) | V |
| `buttercomp2` | Airwindows ButterComp2 (MIT) | Gentle glue compressor | V |
| `console8` | Airwindows Console8 (MIT) | Console colour, "secret sauce" before reverb | V |
| `tape` | Dusk TapeMachine 2 (GPL-3) | Tape saturation, head bump, wow/flutter | V (lead) |
| `delay` | Plugsalot Clean Delay (GPL-3) | Clean stereo/ping-pong echo, slapback, throws | V |
| `tape_echo` | Dusk Tape Echo 2 (GPL-3) | Space Echo style delay plus spring reverb | V |
| `plate` | Dragonfly Plate (GPL-3) | Plate reverb | V |
| `srsly2` | Airwindows Srsly2 (MIT) | Stereo widener | backing vocal only |
| `sweetwide` | Airwindows SweetWide (MIT) | Grungy widener | not tagged for vocal |
| `limiter` | x42 DPL (GPL-3) | Final peak limiter | bus/master only |
| `gx_blueamp`, `gx_plexi`, `gx_supersonic`, `gx_guvnor`, `gx_ultracab` | Guitarix (GPL-2+) | Amps, overdrive, cabinet | guitar |
| `nam_amp`, `cab_ir` | NAM Core, AudioDSPTools (MIT) | Hidden from the AI for now | guitar |
| `drum_sampler` | sfizz (BSD-2) | Software drums source | drums |

Typical vocal path today: EQ (cleanup) -> de-ess -> compressor (multicomp and/or buttercomp2) -> colour (console8 / tape) -> EQ (tone) -> delay / tape echo -> plate. The variety within each slot is thin: one de-esser, one reverb type, one saturation family, no modulation, no gate, no pitch tools.

## 2. Gaps for vocals

1. **No second reverb type.** Plate only. Rooms, halls and springs are the usual alternatives.
2. **No doubler / chorus / modulation.** Nothing for ADT-style doubling, vocal thickening or widening that keeps the vocal centred.
3. **Saturation is narrow.** Console8 and tape cover subtle and warm. There is no tube, transformer or harder "grit" option.
4. **No exciter / air.** Top-end sparkle currently has to come from EQ.
5. **No gate / expander** for breath and bleed cleanup before compression.
6. **No pitch correction.**
7. **Only one compressor family choice per slot** beyond Multi-Comp's modes. A fast "vocal rider" style or a different colour would help.

## 3. Candidates

"Effort" assumes the existing pipeline in `docs/DEVELOPING.md`: pin the upstream, write a curation JSON, extract, vendor, wrap in a module, add an example chain. Names of individual Airwindows effects should be checked against the pinned commit (`dfa54018e7`) when curating.

### A. Easy: more Airwindows (MIT, same pinned repo, `airwindows` extractor already exists)

Five modules already use this source, so no new upstream or licence work. Each is a small wrapper plus curation file.

| Candidate | What it does | Why for vocals |
| --- | --- | --- |
| **Tube2** (or Tube) | Tube-style saturation, even harmonics | Warmth and presence without the tape character; a clear alternative to Console8 |
| **Density** | Soft saturation / "tape-ish" thickness | Adds body to thin takes; gentler than a distortion |
| **Air** | Airy top-end enhancement | Exciter-style sparkle that EQ cannot give |
| **Channel9** | Console channel with transformer drive | A more coloured console option than Console8 |
| **ChorusEnsemble** / **PitchNasty** | Chorus and pitch-offset thickening | Doubling and thickening for backing vocals |
| **Pop** or **Pressure4** | Fast/punchy compressors | Different compression character from ButterComp2 |

Effort: low. Probably the best first batch because it fills the saturation, exciter and chorus gaps at once.

### B. Easy: more from sources already vendored

| Candidate | Upstream | What it does | Effort |
| --- | --- | --- | --- |
| **Dragonfly Hall / Room / Early Reflections** | Same repo as `plate`, GPL-3, `dpf_param_table` extractor | Hall and room reverbs, short ambience | Low. Closes the biggest reverb gap |
| **Other Dusk Audio plugins** (e.g. Multi-Q EQ, a spring reverb) | Same repo as Multi-Comp, GPL-3, `dusk_daf` extractor already lists Multi-Q | More EQ and reverb flavours | Low to medium. Check which are on the pinned commit and framework-free |

### C. Medium: new upstream, new extractor

| Candidate | Upstream | What it does | Why / catch |
| --- | --- | --- | --- |
| **LSP Gate / Expander** | LSP Plugins (LGPL-3), same family as `para_eq`, `lsp_para_eq` extractor exists | Noise gate, expander | Breath and bleed cleanup. Needs a new port-macro extractor per plugin |
| **LSP Chorus / Flanger / Phaser / Slap Delay** | LSP Plugins (LGPL-3) | Modulation and doubling | Gives a proper doubler and a modulation family. Same extractor work |
| **x42 Auto-Tune (fat1.lv2)** | x42 / Fons Adriaensen (GPL-2+) | Scale-based pitch correction | Only real pitch tool candidate. Needs key/scale parameters in the prompt, adds latency, and the AI must be told when not to use it. Uses the `lv2_ttl` extractor |

### D. Hard / probably later

| Candidate | Upstream | Note |
| --- | --- | --- |
| **Calf Exciter, Saturator, Deesser, Vocoder** | Calf Studio Gear (LGPL-2.1+) | Real and licence-compatible, but it is a large plugin framework with its own GUI/DSP split, so extracting DSP is harder than Airwindows |
| **Formant/harmony, vocoder, creative pitch** | various | Few small, permissively licensed options; would need a deliberate search |

## 4. Suggested order

1. Batch 1 (low effort): Tube2, Density, Air, ChorusEnsemble from Airwindows, plus Dragonfly Hall and Room. This adds saturation variety, an exciter, doubling and a second reverb type with almost no new licence or pipeline work.
2. Batch 2: LSP Gate and Chorus, if breath/bleed cleanup and a better doubler matter after listening to batch 1.
3. Batch 3: x42 Auto-Tune, only if pitch correction is something users will actually ask for.

Also worth doing regardless: tag `sweetwide`, `limiter` and `srsly2` for the vocal roles that suit them, so the AI can reach for them. That is a curation edit only.

## 5. Caveats

- Descriptions of candidate plugins come from general knowledge of those projects, not from reading their pinned source. Each should be confirmed (name, licence file, parameter list) when its curation file is written.
- The licence column assumes the project rule that only GPL-3-compatible licences are accepted (`docs/DEVELOPING.md`).

## 6. Status (updated 2026-11-12)

Adin approved batch 1. Added and passing the headless tests (26 runnable modules):

| Module | Upstream | Fills |
| --- | --- | --- |
| `tube2` | Airwindows Tube2 | Tube saturation |
| `density` | Airwindows Density | Fatten / thin saturation |
| `air2` | Airwindows Air2 | Exciter / air. Air2 was used instead of Air because Air only works properly at 44.1 kHz |
| `channel9` | Airwindows Channel9 | Console flavours (Neve, API, SSL, Teac, Mackie) |
| `chorus_ensemble` | Airwindows ChorusEnsemble | Chorus, thickening, light doubling |
| `hall` | Dragonfly Hall Reverb | Big, spacious reverb |
| `room` | Dragonfly Room Reverb | Small natural spaces |

The AI guidelines (`plugin/resources/guidelines.md`) now say when to pick each colour stage and each reverb.

Not yet listened to on real vocals: default levels and the hint ranges are from the upstream documentation and source, so they may need tuning after a listening pass.

Still to do:

- **DuskVerb** (Dusk Audio, same pinned repo as Multi-Comp). It has a framework-free core (`plugins/DuskVerb/core`) with many engines (halls, chambers, plates, spring, shimmer, non-linear, reverse). It is a bigger port than the others because its factory presets carry engine configuration, so it is its own piece of work.
- **TuneBrew** as the built-in pitch corrector, replacing the x42 Auto-Tune idea in section 3C. Needs access to its source.
- LSP Gate and the remaining candidates, if still wanted.
