"""Extract control ports from LV2 Turtle (.ttl / .ttl.in) port lists.

Works on templated .ttl.in files (with @PLACEHOLDERS@) by scanning `[ ... ]` port
blocks as text rather than using an RDF parser. Input control ports become
parameters; an output port with lv2:reportsLatency marks the module as having
variable latency.
"""

from __future__ import annotations

import re
from pathlib import Path

from .common import as_number, balanced

UNIT_MAP = {"db": "dB", "hz": "Hz", "ms": "ms", "s": "s", "pc": "%", "frame": "samples", "khz": "kHz", "bpm": "BPM"}


def _blocks(src: str):
    i = 0
    while True:
        j = src.find("[", i)
        if j == -1:
            return
        try:
            inner, end = balanced(src, j, "[", "]")
        except ValueError:
            return
        if "lv2:index" in inner:
            yield inner
        i = end + 1


def _prop(block: str, key: str) -> str | None:
    m = re.search(rf"{re.escape(key)}\s+([^;\]]+?)\s*[;\]]", block + ";")
    return m.group(1).strip() if m else None


def extract(root: Path, cfg: dict) -> dict:
    src = (root / cfg["file"]).read_text(encoding="utf-8", errors="replace")
    params, latency_port = [], None
    for b in _blocks(src):
        is_input = "lv2:InputPort" in b
        is_control = "lv2:ControlPort" in b
        symbol = (_prop(b, "lv2:symbol") or "").strip('"')
        if not is_control:
            continue
        if not is_input:
            if "lv2:reportsLatency" in b:
                latency_port = symbol
            continue
        name = (_prop(b, "lv2:name") or symbol).strip('"')
        lo = as_number(_prop(b, "lv2:minimum") or "")
        hi = as_number(_prop(b, "lv2:maximum") or "")
        dv = as_number(_prop(b, "lv2:default") or "")
        unit_tok = _prop(b, "units:unit") or ""
        unit = UNIT_MAP.get(unit_tok.split(":")[-1], unit_tok.split(":")[-1])
        p: dict = {"upstream_id": symbol, "name": name}
        if "lv2:toggled" in b:
            p.update({"type": "bool", "default": bool(dv)})
        elif "lv2:enumeration" in b:
            labels = re.findall(r'rdfs:label\s+"([^"]+)"\s*;\s*rdf:value\s+([-\d.]+)', b)
            p.update({"type": "enum", "options": [l for l, _ in labels]})
            for l, v in labels:
                if dv is not None and float(v) == float(dv):
                    p["default"] = l
        elif "lv2:integer" in b:
            p.update({"type": "int", "min": lo, "max": hi, "default": dv, "unit": unit})
        else:
            p.update({"type": "float", "min": lo, "max": hi, "default": dv, "unit": unit})
            if "pprop:logarithmic" in b:
                p["scale"] = "log"
        if "lv2:designation lv2:enabled" in b:
            p["role"] = "bypass_inverted"
        params.append(p)
    return {"params": params, "latency_port": latency_port}
