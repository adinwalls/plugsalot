"""Extract parameters from DPF plugins that use a static PARAMS[] table (Dragonfly Reverb).

  static const Param PARAMS[paramCount] = {
    {paramDecay, "Decay", "decay", 0.1f, 10.0f, "s"},
  ...
Defaults come from presets[DEFAULT_PRESET]; all presets are exported too, so the
AI can start from a named factory sound instead of raw numbers.
"""

from __future__ import annotations

import re
from pathlib import Path

from .common import all_strings, as_number, balanced, split_top_level, strip_comments


def _enum_values(src: str, enum_name: str) -> dict[str, int]:
    m = re.search(rf"enum\s+{enum_name}\s*\{{", src)
    if not m:
        return {}
    body, _ = balanced(src, m.end() - 1, "{", "}")
    values, nxt = {}, 0
    for item in split_top_level(body):
        if "=" in item:
            k, v = (s.strip() for s in item.split("=", 1))
            nxt = int(as_number(v))
        else:
            k = item.strip()
        if k:
            values[k] = nxt
            nxt += 1
    return values


def extract(root: Path, cfg: dict) -> dict:
    src = strip_comments((root / cfg["file"]).read_text(encoding="utf-8", errors="replace"))
    m = re.search(r"\bPARAMS\s*\[[^\]]*\]\s*=\s*\{", src)
    if not m:
        raise ValueError("PARAMS[] table not found")
    body, _ = balanced(src, m.end() - 1, "{", "}")
    rows = []
    for row in split_top_level(body):
        f = split_top_level(row.strip()[1:-1])
        rows.append({
            "enum": f[0].strip(),
            "name": f[1].strip().strip('"'),
            "symbol": f[2].strip().strip('"'),
            "min": as_number(f[3]),
            "max": as_number(f[4]),
            "unit": f[5].strip().strip('"'),
        })

    # enum-valued parameters (e.g. algorithm) declared via an enum + names array
    enum_params = {}
    for pname, (enum_name, names_array) in cfg.get("enum_params", {}).items():
        consts = _enum_values(src, enum_name)
        am = re.search(rf"\b{names_array}\s*\[[^\]]*\]\s*=\s*\{{", src)
        names = all_strings(balanced(src, am.end() - 1, "{", "}")[0]) if am else []
        enum_params[pname] = (consts, names)

    def to_value(sym: str, token: str):
        token = token.strip()
        if sym in enum_params:
            consts, names = enum_params[sym]
            idx = consts.get(token, as_number(token))
            return names[int(idx)] if idx is not None and int(idx) < len(names) else None
        return as_number(token)

    presets = []
    pm = re.search(r"\bpresets\s*\[[^\]]*\]\s*=\s*\{", src)
    if pm:
        pbody, _ = balanced(src, pm.end() - 1, "{", "}")
        for prow in split_top_level(pbody):
            name = all_strings(prow)[0]
            vals_text = prow[prow.index("{", 1) :]
            vals = split_top_level(balanced(vals_text, 0, "{", "}")[0])
            presets.append({"name": name, "params": {r["symbol"]: to_value(r["symbol"], v) for r, v in zip(rows, vals)}})
    dm = re.search(r"DEFAULT_PRESET\s*=\s*(\d+)", src)
    default_preset = presets[int(dm.group(1))] if dm and presets else None

    params = []
    for r in rows:
        p: dict = {"upstream_id": r["symbol"], "name": r["name"]}
        if r["symbol"] in enum_params:
            p.update({"type": "enum", "options": enum_params[r["symbol"]][1]})
        else:
            p.update({"type": "float", "min": r["min"], "max": r["max"], "unit": r["unit"]})
        if default_preset:
            p["default"] = default_preset["params"][r["symbol"]]
        params.append(p)
    return {"params": params, "presets": presets, "default_preset": default_preset["name"] if default_preset else None}
