# Testing Plugsalot 0.2 (prototype)

This build is about whether the idea works: you describe a sound, the AI builds a chain from real open-source DSP, and it sounds right. The interface is deliberately plain; its design is the next step.

## 1. Get the build

GitHub builds every version automatically for Linux, Windows and macOS:

1. Open the repo on GitHub, go to **Actions**, and click the latest green **Build plugin** run.
2. Under **Artifacts**, download `Plugsalot-linux`, `Plugsalot-windows` or `Plugsalot-macos`.

## 2. Install

Each download contains the VST3 plugin, a standalone app, and on Linux an LV2 plugin.

| System | VST3 goes in | Other |
| --- | --- | --- |
| Linux (Pop!_OS) | `~/.vst3/` | LV2: `~/.lv2/`. Standalone: run `Plugsalot` directly (`chmod +x Plugsalot` first). |
| Windows | `C:\Program Files\Common Files\VST3\` | Standalone: `Plugsalot.exe`. Windows may warn that it's from an unknown publisher; choose "More info", then "Run anyway". |
| macOS | `~/Library/Audio/Plug-Ins/VST3/` | AU: `~/Library/Audio/Plug-Ins/Components/`. Not notarized yet: right-click the app and choose Open the first time. |

Then rescan plugins in your DAW. The **standalone app** is the quickest test: pick your audio interface under Options > Audio settings and play or sing into it. The app starts with input muted (a yellow bar says so) to avoid feedback: put headphones on, click **Settings...** in that bar and untick the mute.

To look around without an API key, start the standalone app with `PLUGSALOT_DEMO=1` set (Linux/macOS: `PLUGSALOT_DEMO=1 ./Plugsalot`); it opens with a sample vocal chain.

## 3. Turn on the AI

The first time you open Plugsalot, the setup card appears:

1. Click **Open Google AI Studio** and sign in with a Google account. If your browser doesn't open (some DAWs block it), the link is already copied: paste it into your browser, or copy it from the box next to the button.
2. Click **Create API key**, and copy it.
3. Paste it into Plugsalot (Ctrl+V, or right-click > Paste) and click **Connect**. A green tick shows which model answered.

**Model:** leave it on **Automatic** to use the best free Flash model. Google's free tier is often busy for a few seconds at a time, so if a model answers "busy", Plugsalot tries the other Flash and Flash-Lite models, then waits and goes round again (three rounds over about 15 seconds) before it gives up. A model you pick yourself is tried first, with the others as backup. You can also pick a specific model from the list, which shows the models your key can use. Pro models usually need billing turned on in Google AI Studio.

**Your key saves as soon as you paste it.** Connect only checks that it works, so a busy model or closing the card can't lose it. The key is shared by every Plugsalot instance and the standalone app.

**Typing in a Linux DAW:** click the chat box and type; Enter sends. Some Linux DAWs (Ardour, for one) keep the keyboard for their own shortcuts, so clicking the chat box asks the system to send your typing to Plugsalot instead; click back in the DAW to get its shortcuts again. If typing still doesn't arrive, click **Can't type? Open a typing window** under the chat box.

To compare with Claude, choose **Claude: your own API key** in Settings and paste a key from the Claude Console. Automatic uses `claude-haiku-5-5`; pick Sonnet from the Model list for more detailed advice.

## 4. Things to try

1. **First chain:** Source = Lead vocal, Stage = Mixing, Style = Pop. Play a vocal through it for a few seconds (so the AI gets the level and tone analysis), then press **Send** with the box empty.
2. **Describe a sound:** "warm, intimate, a little vintage" or "big modern pop vocal".
3. **Follow-ups:** press **Brighter**, **Less muddy** or **More space**. These should make small changes, not rebuild the chain.
4. **Check it:** click a module to see its settings, nudge a slider, switch a module off and on, and try **Undo**.
5. **Other sources:** bass DI, full kit / drum bus, acoustic guitar, master.
6. **Limits:** set CPU to Light and ask for "lots of tape character". The chain should stay within budget. Set Stage to Tracking: no limiter should appear.
7. **Save and reload:** save your DAW project, close and reopen it. The chain and chat should come back.

8. **Guitar amps (new in 0.2):** set Source to Electric guitar and ask for "a crunchy rock rhythm tone". Plugsalot only uses amp captures you own, so first add some: click a **NAM Amp** module, then **Add file** (any `.nam` file) or **TONE3000** (see below). Add a cabinet `.wav` impulse response to the **Cabinet IR** module the same way. Your files are kept in `Plugsalot/Captures` and `Plugsalot/Cabinets` in your user folder, and the AI is told their names so it can pick one.
9. **TONE3000 (new in 0.2):** TONE3000 sign-in needs the app key from TONE3000, so it works only once that is built in (or set `PLUGSALOT_TONE3000_CLIENT_ID` in your environment before starting your DAW). Then click **TONE3000** on a NAM Amp module, sign in with the code shown (you approve it in your browser), pick a tone and model, and **Use this capture**. The creator's name and a link are shown under the capture. A2 and "lite" models use much less CPU than standard ones.

10. **Delays (new):** ask for "a slapback on the lead vocal" or "a dubby tape echo on the snare". The AI chooses between **Clean Delay** (exact, transparent, optional ping-pong and ducking) and **Tape Echo** (warm, wobbly, with spring reverb). Tape Echo's Intensity above about 75% self-oscillates on purpose.
11. **Compression and colour (new):** ask for a "polished pop vocal with a fast compressor then a smooth LA-2A-style one" (the AI should use two Multi-Comp stages, or ButterComp2 for glue), and for "some console warmth before the reverb" (Console8). Console8's Tone has Solid and Hype; Drive adds saturation and level.
12. **Stereo width (new):** ask for "make the synth pad wider" or "a huge wide reverb". **Srsly2** is the smooth widener; **SweetWide** is gritty and colours the sound. Check both in mono, because extreme widening can thin out.
13. **New look (0.3.0):** the window is now the vintage rack. Check: (a) resize the window by its corner; the whole picture scales and keeps its proportions; (b) turn a knob by dragging up or down (double-click resets, Shift gives fine control, the value shows in gold while you hover); (c) the **IN** button bypasses a module and its lamp goes dark; (d) click a module's name to open **all** its settings (and the capture / cabinet pickers, TONE3000 and Remove); (e) hover a module for two seconds for its description; (f) ask for a new chain and watch the old modules unplug and the new ones plug in, then ask for a small tweak ("brighter") and check the knobs turn without any unplugging; (g) the VU meter moves with the output level while audio plays; (h) typing in the chat box still works in your DAW (use the typing-window button if not).

## 5. What to look out for

- Amp captures are heavy on the CPU: a standard WaveNet capture can use 30% of a core on a slow machine. If you hear crackles, switch the CPU box to Light or pick a lighter (A2 or lite) model.
- Does each first chain sound like a sensible starting point for that source?
- Do the explanations make sense to you as a musician?
- Any clicks, dropouts or level jumps when the chain changes?
- Any reply where nothing happened and a note appeared instead? Copy the note text.
- How long does a reply take on the free Gemini key?

## Known limits

- Modules: Console EQ (Dusk 4K EQ 2), Multi-Comp (Dusk), DeBess (Airwindows), TapeMachine 2 (Dusk), Plate Reverb (Dragonfly), Peak Limiter (x42), NAM Amp (Neural Amp Modeler), Cabinet IR, Clean Delay (Plugsalot's own) Tape Echo (Dusk Tape Echo 2), ButterComp2, Console8, Srsly2 and SweetWide (all Airwindows). The LSP 8-band EQ and the sfizz drum sampler are in the catalogue but not in the plugin yet.
- Your API key is saved in a plain settings file in your user folder (not yet the system keychain).
- Reverb is an insert with a dry/wet mix, not a send.
- Bypassing a module that has latency briefly shifts timing; a latency-matched bypass is planned.
- TONE3000 browsing is tested against a stand-in server only, not the real service, and the interface uses a text "TONE3000" heading until the official logo files are added.
- Captures are mono and a capture runs at the model's own cost; the amp sums stereo input to mono.
- No hosted AI yet; the Patreon tier comes later.
- Builds are unsigned, so operating systems warn on first launch.
- Multi-Comp's settings panel shows every mode's controls at once; only the chosen mode's controls have an effect.
- Google's API can't be reached from the build machine, so Gemini is tested against a stand-in server that reproduces its replies (including the "high demand" 503). Your own key is the first real test; if anything fails, the note in the chat says why.

## Electric guitar (0.3.1)

Guitar tones now come from Guitarix amps with knobs, so there is nothing to download: ask for "a clean blues guitar" (BlueAmp), "classic rock crunch" (Plexi), "heavy rhythm guitar" (Supersonic), or "push the amp with an overdrive pedal" (Guvnor before the amp). Every amp chain ends in UltraCab, the speaker cabinet. The Neural Amp Modeler and cabinet impulse-response modules still exist but the AI no longer offers them, and the TONE3000 button is hidden.
