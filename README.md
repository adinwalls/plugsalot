# Plugsalot

*An AI plugin chain for your DAW, built from open-source plugins, for people who would rather be making music.*

## A note from me

Hello! I'm Adin, and I'm a songwriter. Like a lot of home recording artists, I just want to get the music out of my head and heart and into the world. But I'm busy, and on a good week I get an hour or two to actually record something. I don't want to spend that hour reading about plugins.

Plugins are a headache. I recently wrote a 14 page guide to getting Mixbus working on Linux, and it was a lot to keep in my head when I had real work to do. That is why I made Plugsalot.

I don't actually think plugins suck. I *love* them, and this project is a dream come true for me. The open-source plugin world is full of unsung heroes, and I've gathered a lot of my favourites into one simple window. Tell Plugsalot what you're recording (a dropdown or two, plus a chat), and it sets up the plugins you need. It tells you what each one is, what it's doing, and what licence it runs on. You can use any AI you like, so as the models get smarter, Plugsalot gets better too.

Please try it with your own recordings and tell me what you think. Come and find me on the [Discord](https://discord.gg/jzdu3uyGYq), which has a channel just for Plugsalot. If you'd like to support the project, there's [Patreon](https://www.patreon.com/cw/adinwalls) and a once-off tip on [Ko-fi](https://ko-fi.com/adinwalls). More of my writing is at [adinwalls.com](https://www.adinwalls.com).

> Plugsalot is an early **alpha**. Expect rough edges, and please tell me about them.

## Thank you

Plugsalot is nothing without the people who build and give away incredible plugins. Thank you, truly, for all of your work. Each one is used under its own open-source licence; the full details and exact versions are in [THIRD_PARTY.md](THIRD_PARTY.md).

| Project | Developer(s) | Plugins used in Plugsalot |
| --- | --- | --- |
| [Airwindows](https://www.airwindows.com/) | Chris Johnson | DeBess (de-esser), ButterComp2 (compressor), Console8 (console warmth), Srsly2 and SweetWide (stereo wideners) |
| [Dusk Audio](https://github.com/dusk-audio/dusk-audio-plugins) | Dusk Audio | 4K EQ 2 (console EQ), Multi-Comp (compressor), TapeMachine 2 (tape), Tape Echo 2 (tape delay) |
| [Guitarix](https://guitarix.org/) | Hermann Meyer, James Warden, Andreas Degert, Pete Shorthose | BlueAmp (clean/blues amp), Plexi (classic rock amp), Supersonic (high-gain amp), Guvnor (overdrive pedal), UltraCab (speaker cabinet) |
| [Dragonfly Reverb](https://github.com/michaelwillis/dragonfly-reverb) | Michael Willis and contributors (with freeverb3 by Teru Kamogashira) | Plate Reverb |
| [x42 plugins](https://github.com/x42/dpl.lv2) | Robin Gareus | Digital Peak Limiter |
| [Neural Amp Modeler](https://github.com/sdatkinson/NeuralAmpModelerCore) and AudioDSPTools | Steven Atkinson | NAM Amp and Cabinet IR (still in the plugin, but the AI doesn't offer them for now) |
| [LSP Plugins](https://github.com/lsp-plugins) | Vladimir Sadovnikov and the Linux Studio Plugins Project | Parametric EQ (coming soon) |
| [sfizz](https://github.com/sfztools/sfizz) | sfizz contributors | Drum sampler (coming soon) |

Plugsalot also includes a simple Clean Delay that I wrote for this project, and uses the [Barlow](https://github.com/jpt/barlow) and [Permanent Marker](https://fonts.google.com/specimen/Permanent+Marker) fonts. Plugsalot itself is free software, **GPL-3.0-or-later**, and is built with [JUCE](https://juce.com/).

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

## Setting it up

You need three things: the plugin installed, an AI key, and a few minutes. It takes about the same on every system.

### Step 1: Download Plugsalot

1. Go to the [Releases page](https://github.com/adinwalls/plugsalot/releases) and open the newest release.
2. Under **Assets**, download the zip for your system: `Linux`, `Windows` or `macOS`.
3. Unzip it. Inside is the VST3 plugin, a standalone app, and (Linux) an LV2 plugin.

### Step 2: Install it

**Linux** (tested on Pop!_OS with Ardour)
1. Copy the `Plugsalot.vst3` folder into `~/.vst3/` (create the folder if it isn't there). For LV2, copy `Plugsalot.lv2` into `~/.lv2/`.
2. To use the standalone app, make it runnable (`chmod +x Plugsalot`) and start it.
3. Open your DAW and rescan plugins.

**Windows**
1. Copy the `Plugsalot.vst3` folder into `C:\Program Files\Common Files\VST3\`.
2. The standalone app is `Plugsalot.exe`. Windows may warn that it's from an unknown publisher (builds aren't signed yet): choose **More info**, then **Run anyway**.
3. Open your DAW and rescan plugins.

**macOS**
1. Copy `Plugsalot.vst3` into `~/Library/Audio/Plug-Ins/VST3/`. For the AU version, copy `Plugsalot.component` into `~/Library/Audio/Plug-Ins/Components/`.
2. Builds aren't notarized yet, so the first time, right-click the app (or plugin) and choose **Open**.
3. Open your DAW and rescan plugins.

Not sure your DAW works with it? Try the **standalone app** first. Pick your audio interface under *Options > Audio settings*. It starts with the input muted to avoid feedback, so put headphones on, click **Settings...** in the yellow bar, and untick the mute.

### Step 3: Give Plugsalot a brain (an AI key)

Plugsalot works with the AI service of your choice. You paste in your own key, and nobody else sees your chats. The first time you open it, a setup card appears.

**Option A: Google Gemini (free)**
1. Click **Open Google AI Studio** and sign in with a Google account. If the browser doesn't open, the link is already copied, so paste it into your browser.
2. Click **Create API key** and copy it.
3. Paste it into Plugsalot (Ctrl+V, or right-click and Paste) and click **Connect**. A green tick means it worked. Leave the model on **Automatic**.

The free tier is sometimes busy for a few seconds. Plugsalot automatically retries and tries other models, so just give it a moment.

**Option B: Claude (paid)**
1. Make an account at the [Claude Console](https://console.anthropic.com) and add a key. This is billed to you by Anthropic, and is separate from a claude.ai chat subscription.
2. In Plugsalot, open **Settings**, choose **Claude: your own API key**, and paste the key.
3. Automatic uses a fast, low-cost model. Choose a larger one from the Model list if you want more detailed advice.

Your key is saved in a settings file in your user folder (the system keychain is on the to-do list), and it's shared by every Plugsalot instance.

### Step 4: Make your first chain

1. Pick a **Source** (for example Lead vocal), a **Stage** (Tracking, Mixing...) and a **Style**.
2. Play or sing for a few seconds so Plugsalot can hear your level and tone.
3. Press **Send** with the chat box empty, or describe the sound: *"warm, intimate, a little vintage"*.
4. Tweak with the quick buttons (**Brighter**, **Less muddy**, **More space**) or by chat. **Undo** reverts any change.

For guitar, set Source to Electric guitar and ask for "a crunchy rock rhythm tone". It is still being tested and may not work well on Windows and macOS yet.

Typing in a Linux DAW: if your DAW keeps the keyboard (Ardour does), click the chat box first, or use **Can't type? Open a typing window**.

More things to try, and what to look out for, are in [docs/TESTING.md](docs/TESTING.md). Developers: see [docs/DEVELOPING.md](docs/DEVELOPING.md).

## Not done yet

Plugsalot is an alpha, so there's plenty still to do:

- The LSP Parametric EQ and the sfizz drum sampler are catalogued but not in the plugin yet.
- The guitar amps still need lots of listening with real guitar, especially output levels, and Windows/macOS testing.
- Hosted AI for Patreon supporters, so you won't need your own key.
- API keys stored in the system keychain.
- Reverb on a send (it's an insert for now), and latency-matched bypass.
- Factory presets for TapeMachine 2 and Multi-Comp, and Multi-Comp's multiband mode.
- Signed builds, so Windows and macOS stop warning you on first launch.
- The interface is a first pass: the real design is still to come.
- Amp captures from TONE3000 (put on hold).

Got an idea or hit a bug? Tell me on the [Discord](https://discord.gg/jzdu3uyGYq).
