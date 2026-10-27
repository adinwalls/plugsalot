"""Extract parameters from an Airwindows effect (LinuxVST source layout).

Every Airwindows parameter is a 0..1 value. The plugin's getParameterDisplay()
shows how that maps to what the user sees (e.g. `(B-1.0)*48.0` -> dB), which is
kept as `display_expr` so Plugsalot can present real units later.
The effect's description is taken from Airwindopedia.txt.
"""

from __future__ import annotations

import re
from pathlib import Path

from .common import as_number, strip_comments

LETTERS = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"


def _cases(src: str, func: str) -> dict[str, str]:
    m = re.search(rf"::{func}\s*\(", src)
    if not m:
        return {}
    body = src[m.end() : src.find("\n}", m.end())]
    out = {}
    for cm in re.finditer(r"case\s+kParam([A-Z])\s*:(.*?)(?=case\s+kParam|default\s*:|\Z)", body, re.S):
        out[cm.group(1)] = cm.group(2).strip()
    return out


def describe(pedia: str, effect: str) -> str | None:
    m = re.search(rf"^#+\s*{re.escape(effect)}\s+(is|does|has|was)\b(.*)$", pedia, re.M)
    if not m:
        return None
    return f"{effect} {m.group(1)}{m.group(2)}".strip()


def extract(root: Path, cfg: dict) -> dict:
    effect = cfg["effect"]
    base = root / "plugins" / "LinuxVST" / "src" / effect
    header = strip_comments((base / f"{effect}.h").read_text(errors="replace"))
    src = strip_comments((base / f"{effect}.cpp").read_text(errors="replace"))
    letters = sorted(set(re.findall(r"\bkParam([A-Z])\s*=", header)), key=LETTERS.index)

    ctor = src[src.find(f"{effect}::{effect}(") :]
    ctor = ctor[: ctor.find("\n}")]
    defaults = {L: as_number(v) for L, v in re.findall(r"\b([A-Z])\s*=\s*([-\d.]+)\s*;", ctor)}

    names = {L: (re.search(r'vst_strncpy\s*\(\s*text\s*,\s*"([^"]*)"', t) or [None, None])[1] for L, t in _cases(src, "getParameterName").items()}
    displays = _cases(src, "getParameterDisplay")
    labels = {L: re.findall(r'"([^"]*)"', t) for L, t in _cases(src, "getParameterLabel").items()}

    params = []
    for L in letters:
        disp = displays.get(L, "")
        dm = re.search(r"(float2string|dB2string|int2string)\s*\((.*?),\s*text", disp, re.S)
        p = {
            "upstream_id": L,
            "name": names.get(L) or L,
            "type": "float",
            "min": 0.0,
            "max": 1.0,
            "default": defaults.get(L, 0.5),
            "unit": "",
        }
        if dm:
            p["display_expr"] = f"{dm.group(1)}({dm.group(2).strip()})"
        lab = [x for x in labels.get(L, []) if x]
        if len(lab) == 1:
            p["unit"] = lab[0]
        elif len(lab) > 1:
            p["display_labels"] = lab
        params.append(p)

    out: dict = {"params": params}
    pedia_path = root / "Airwindopedia.txt"
    if pedia_path.exists():
        d = describe(pedia_path.read_text(errors="replace"), effect)
        if d:
            out["description"] = d
    return out
