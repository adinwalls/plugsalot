"""Extract the LSP Parametric Equalizer's band parameters.

LSP describes ports with C macros (EQ_FILTER, AMP_GAIN, ...). Rather than run the
preprocessor, this extractor understands the one macro family the para-equalizer
uses: it reads the option tables and the EQ_FILTER_*(index, total, default_freq)
lines of the chosen port array, and resolves the numeric limits from the LSP
headers (lsp-dsp-units for frequency, the plugin's own metadata for width).

LSP stores gains as linear amplitude. The AI works in dB, so gain parameters are
emitted in dB with convert = "db_to_amp".
"""

from __future__ import annotations

import re
from pathlib import Path

from .common import all_strings, as_number, balanced, strip_comments

NEEDS_ROOTS = True


def _items(src: str, name: str) -> list[str]:
    m = re.search(rf"\b{name}\s*\[\s*\]\s*=\s*\{{", src)
    if not m:
        return []
    body, _ = balanced(src, m.end() - 1, "{", "}")
    body = re.sub(r"#ifdef.*?#endif[^\n]*", "", body, flags=re.S)  # drop experimental entries
    return [s for s in all_strings(body)[0::2]]


def _const(text: str, name: str) -> float:
    m = re.search(rf"{name}\s*=\s*([-\d.]+)f?", text) or re.search(rf"#define\s+{name}\s+([-\d.]+)f?", text)
    if not m:
        raise ValueError(f"constant {name} not found")
    return as_number(m.group(1))


def extract(root: Path, cfg: dict, roots: dict[str, Path]) -> dict:
    meta_cpp = strip_comments((root / "src/main/meta/para_equalizer.cpp").read_text())
    meta_h = (root / "include/private/meta/para_equalizer.h").read_text()
    units_h = (roots["lsp-dsp-units"] / "include/lsp-plug.in/dsp-units/const.h").read_text()

    f_min = _const(units_h, "LSP_DSP_UNITS_SPEC_FREQ_MIN")
    f_max = _const(units_h, "LSP_DSP_UNITS_SPEC_FREQ_MAX")
    w_min, w_max, w_dfl = (_const(meta_h, f"WIDTH_{k}") for k in ("MIN", "MAX", "DFL"))

    types = _items(meta_cpp, "filter_types")
    modes = _items(meta_cpp, "filter_modes")
    slopes = _items(meta_cpp, "filter_slopes")
    eq_modes = _items(meta_cpp, "equalizer_eq_modes")

    arr = cfg["ports_array"]
    m = re.search(rf"\b{arr}\s*\[\s*\]\s*=\s*\{{", meta_cpp)
    if not m:
        raise ValueError(f"port array {arr} not found")
    body, _ = balanced(meta_cpp, m.end() - 1, "{", "}")
    bands = [(int(i), as_number(f)) for i, _, f in re.findall(r"EQ_FILTER_(?:MONO|STEREO)\s*\(\s*(\d+)\s*,\s*(\d+)\s*,\s*([-\d.]+)f?\s*\)", body)]

    params = [
        {"upstream_id": "g_in", "name": "Input gain", "type": "float", "min": -144.0, "max": 20.0, "default": 0.0, "unit": "dB", "convert": "db_to_amp"},
        {"upstream_id": "g_out", "name": "Output gain", "type": "float", "min": -144.0, "max": 20.0, "default": 0.0, "unit": "dB", "convert": "db_to_amp"},
        {"upstream_id": "mode", "name": "Equalizer mode", "type": "enum", "options": eq_modes, "default": eq_modes[0], "convert": "enum_index"},
    ]
    for i, f_dfl in bands:
        params += [
            {"upstream_id": f"ft_{i}", "name": f"Filter type {i}", "type": "enum", "options": types, "default": types[0], "convert": "enum_index"},
            {"upstream_id": f"fm_{i}", "name": f"Filter mode {i}", "type": "enum", "options": modes, "default": modes[0], "convert": "enum_index"},
            {"upstream_id": f"s_{i}", "name": f"Filter slope {i}", "type": "enum", "options": slopes, "default": slopes[0], "convert": "enum_index"},
            {"upstream_id": f"f_{i}", "name": f"Frequency {i}", "type": "float", "min": f_min, "max": f_max, "default": f_dfl, "unit": "Hz", "scale": "log"},
            {"upstream_id": f"g_{i}", "name": f"Gain {i}", "type": "float", "min": -36.0, "max": 36.0, "default": 0.0, "unit": "dB", "convert": "db_to_amp"},
            {"upstream_id": f"q_{i}", "name": f"Quality factor {i}", "type": "float", "min": 0.0, "max": 100.0, "default": 0.0, "unit": ""},
            {"upstream_id": f"w_{i}", "name": f"Filter width {i}", "type": "float", "min": w_min, "max": w_max, "default": w_dfl, "unit": "octaves"},
        ]
    return {"params": params, "bands": len(bands)}
