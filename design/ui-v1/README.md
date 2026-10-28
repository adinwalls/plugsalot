# UI design v1: vintage rack

Interactive prototype of the new Plugsalot interface, made with Adin on 2026-10-28.
It is a Design Component page (`Main.dc.html`) from the Claude design canvas, kept here as the
reference for porting the look into the JUCE plugin. It is not wired into the build yet.

What it settles:

- Wooden case (teak by default) around a dark blue-grey enamel panel.
- Each module is a 500-series-style card in a six-slot rack, signal flowing left to right.
- Knob colour = function: blue tone, red dynamics, grey level, cream time and space, black filters.
- A masking-tape scribble strip on each module says what it does in plain words.
- Hover a module for 2 seconds to see its catalogue description.
- When the AI builds a chain, old modules unplug one by one (right to left) and new ones
  plug in left to right; knobs then sweep to their settings. Follow-up tweaks only turn knobs.
- Send is a square red-orange pilot lamp with a label plate; the VU meter has a warm backlight.
