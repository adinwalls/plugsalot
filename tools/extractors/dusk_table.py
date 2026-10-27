"""Generic extractor for Dusk Audio DAF descriptor arrays.

Dusk's DAF plugins describe parameters in constexpr arrays whose row layout
varies by plugin, so the curation file names the fields in order:

  Multi-Comp: {"mode","Mode","",0,7,0,CoreParameter::Mode,true}
    fields = ["id","name","unit","min","max","default","core","integer","interval","skew","midpoint"]
  4K EQ:      { "lf_gain", -15.f, 15.f, 0.f }
    fields = ["id","min","max","default"]

Identifiers used as numbers (duskaudio::kOptoGainUnityKnob, vcaLaw::kThresholdMinDb)
are resolved from `constant_files`. Choice labels come from named const char*
arrays mapped per parameter in `labels`; parameters listed in `booleans` are
Off/On switches; `inert` lists parameters the DSP ignores, which are dropped.
"""

from __future__ import annotations

import re
from pathlib import Path

from .common import all_strings, as_number, balanced, split_top_level, strip_comments

LABEL_ARRAY_RE = r"\b{name}\s*\[[^\]]*\]\s*=\s*\{{"


def _constants(texts: list[str]) -> dict[str, float]:
    consts = {}
    for t in texts:
        for m in re.finditer(r"constexpr\s+(?:float|double|int)\s+(\w+)\s*=\s*([-+]?[\d.]+(?:[eE][-+]?\d+)?)f?\s*;", t):
            consts[m.group(1)] = as_number(m.group(2))
    return consts


def _value(token: str, consts: dict[str, float]):
    token = token.strip()
    n = as_number(token)
    if n is not None:
        return n
    if token in ("true", "false"):
        return token == "true"
    name = token.split("::")[-1]
    if name in consts:
        return consts[name]
    if token.startswith('"') and token.endswith('"'):
        return token[1:-1]
    return token  # unresolved identifier (e.g. CoreParameter::Mode) kept as text


def extract(root: Path, cfg: dict) -> dict:
    path = root / cfg["file"]
    src = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    texts = [src] + [strip_comments((root / f).read_text(errors="replace")) for f in cfg.get("constant_files", [])]
    consts = _constants(texts)

    m = re.search(rf"\b{re.escape(cfg['table'])}\s*(?:\[[^\]]*\])?\s*=\s*\{{\s*\{{?", src)
    if not m:
        raise ValueError(f"table {cfg['table']!r} not found")
    start = src.index("{", m.start())
    body, _ = balanced(src, start, "{", "}")
    if body.strip().startswith("{") and body.strip().count("{") > 1 and cfg.get("double_braced", True):
        inner = body.strip()
        if inner.startswith("{") and balanced(inner, 0, "{", "}")[1] == len(inner) - 1:
            body = inner[1:-1]  # std::array<...> = {{ ... }}

    fields = cfg["fields"]
    labels_map = cfg.get("labels", {})
    booleans = set(cfg.get("booleans", []))
    inert = set(cfg.get("inert", []))
    meters = set(cfg.get("meters", []))

    def label_array(name: str) -> list[str]:
        am = re.search(LABEL_ARRAY_RE.format(name=re.escape(name)), src)
        return all_strings(balanced(src, am.end() - 1, "{", "}")[0]) if am else []

    params = []
    for row in split_top_level(body):
        row = row.strip()
        if not row.startswith("{"):
            continue
        vals = [_value(v, consts) for v in split_top_level(row[1:-1])]
        rec = dict(zip(fields, vals))
        pid = rec.get("id")
        if not isinstance(pid, str) or pid in inert:
            continue
        p: dict = {"upstream_id": pid, "name": rec.get("name") or pid}
        if "core" in rec and isinstance(rec["core"], str):
            p["core"] = rec["core"].split("::")[-1]
        lo, hi, dv = rec.get("min"), rec.get("max"), rec.get("default")
        unresolved = [k for k in ("min", "max", "default") if not isinstance(rec.get(k), (int, float))]
        if unresolved:
            p.update({"type": "float", "unit": rec.get("unit", "") or "", "unresolved": True, "missing": unresolved, "raw": row[:200]})
            p.update({k: rec[k] for k in ("min", "max", "default") if isinstance(rec.get(k), (int, float))})
        elif pid in labels_map:
            opts = label_array(labels_map[pid])
            if len(opts) == int(hi - lo) + 1:
                p.update({"type": "enum", "options": opts, "default": opts[int(dv - lo)], "convert": "enum_index"})
            else:
                p.update({"type": "enum", "unresolved": True, "missing": ["options"], "raw": f"{labels_map[pid]} has {len(opts)} labels"})
        elif pid in booleans:
            p.update({"type": "bool", "default": bool(dv)})
        else:
            p.update({"type": "float", "min": lo, "max": hi, "default": dv, "unit": rec.get("unit", "") or ""})
            if isinstance(rec.get("skew"), (int, float)) and rec["skew"] not in (0, 1):
                p["scale"] = "log"
        if pid in meters:
            p["role"] = "meter"
        if pid == "bypass":
            p["role"] = "bypass"
        params.append(p)
    return {"params": params}
