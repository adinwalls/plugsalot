"""Extract parameters from Dusk Audio DAF descriptor tables.

Dusk's DAF plugins (TapeMachine 2, 4K EQ, Multi-Q, Tape Echo 2) declare every
parameter in one constexpr table, e.g.

  { "inputGain", "Input Gain", 'f', -12.f, 12.f, 0.f, "dB", nullptr, 0 },
  { "tapeSpeed", "Tape Speed", 'c', 0.f, 3.f, 1.f, "", tmparams::kTapeSpeed, ... },

kind: 'f' linear float, 'g' skewed float, 'c' choice, 'b' bool/bypass, 'o' output meter.
Choice labels are resolved from `static constexpr const char* kName[] = {...}` arrays.
"""

from __future__ import annotations

import re
from pathlib import Path

from .common import all_strings, as_number, balanced, split_top_level, strip_comments

ARRAY_RE = re.compile(r"static\s+constexpr\s+const\s+char\s*\*\s*(\w+)\s*\[\s*\]\s*=\s*\{")


def _choice_arrays(src: str) -> dict[str, list[str]]:
    arrays = {}
    for m in ARRAY_RE.finditer(src):
        inner, _ = balanced(src, m.end() - 1, "{", "}")
        arrays[m.group(1)] = all_strings(inner)
    return arrays


def extract(root: Path, cfg: dict) -> dict:
    src = strip_comments((root / cfg["file"]).read_text(encoding="utf-8", errors="replace"))
    table = cfg.get("table", "kTmParams")
    m = re.search(rf"\b{re.escape(table)}\s*\[[^\]]*\]\s*=\s*\{{", src)
    if not m:
        raise ValueError(f"descriptor table {table!r} not found")
    body, _ = balanced(src, m.end() - 1, "{", "}")
    arrays = _choice_arrays(src)
    params = []
    for row in split_top_level(body):
        if not row.startswith("{"):
            continue
        f = split_top_level(row.strip()[1:-1])
        if len(f) < 7:
            continue
        pid = f[0].strip().strip('"')
        name = f[1].strip().strip('"')
        kind = f[2].strip().strip("'")
        lo, hi, dv = as_number(f[3]), as_number(f[4]), as_number(f[5])
        unit = f[6].strip().strip('"')
        choice_ref = f[7].strip().split("::")[-1] if len(f) > 7 else "nullptr"
        p: dict = {"upstream_id": pid, "name": name}
        if kind == "o":
            p.update({"type": "float", "role": "meter", "min": lo, "max": hi, "default": dv, "unit": unit})
        elif kind in ("f", "g"):
            p.update({"type": "float", "min": lo, "max": hi, "default": dv, "unit": unit})
            if kind == "g":
                p["scale"] = "log"
        elif kind == "b":
            p.update({"type": "bool", "default": bool(dv)})
        elif kind == "c":
            opts = arrays.get(choice_ref)
            if opts and dv is not None and 0 <= int(dv) < len(opts):
                p.update({"type": "enum", "options": opts, "default": opts[int(dv)]})
                if opts == ["Off", "On"]:
                    p = {"upstream_id": pid, "name": name, "type": "bool", "default": bool(dv), "convert_hint": "enum_index"}
            else:
                p.update({"type": "enum", "unresolved": True, "missing": ["options"], "raw": row[:200]})
        if pid.lower() == "bypass":
            p["role"] = "bypass"
        params.append(p)
    return {"params": params}
