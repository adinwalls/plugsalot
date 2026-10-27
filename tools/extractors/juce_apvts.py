"""Extract parameters from JUCE AudioProcessorValueTreeState layouts.

Handles the common literal forms:
  AudioParameterFloat("id", "Name", NormalisableRange<float>(min, max, step, skew), default, "unit")
  AudioParameterFloat(ParameterID{"id", 1}, "Name", ..., AudioParameterFloatAttributes().withLabel("dB"))
  AudioParameterChoice("id", "Name", StringArray{"A", "B"}, defaultIndex)
  AudioParameterBool("id", "Name", false)
  AudioParameterInt("id", "Name", min, max, default)

Declarations built in loops or from variables are emitted with "unresolved": true
and the raw argument text, for a human to complete in the curation file.
"""

from __future__ import annotations

import re
from pathlib import Path

from .common import all_strings, as_number, balanced, find_calls, first_string, split_top_level, strip_comments

CALL_RE = r"AudioParameter(Float|Choice|Bool|Int)\s*>\s*\("
RANGE_RE = re.compile(r"NormalisableRange\s*<\s*float\s*>\s*\(([^()]*)\)")
LABEL_RE = re.compile(r'withLabel\s*\(\s*"([^"]*)"\s*\)')


def _id_and_name(args: list[str]) -> tuple[str | None, str | None]:
    pid = first_string(args[0]) if args else None
    # Ids built at runtime ("mb_" + name + "_x", ParamIDs::bandFreq(n)) are not literals.
    if args and ("+" in args[0] or "ParamIDs::" in args[0]):
        pid = None
    name = None
    if len(args) > 1:
        a = args[1].strip()
        if a.startswith('"') and a.endswith('"') and a.count('"') == 2:
            name = a[1:-1]
    return pid, name


def _parse_float(args: list[str]) -> dict:
    out: dict = {"type": "float"}
    joined = ",".join(args)
    m = RANGE_RE.search(joined)
    if m:
        nums = [as_number(x) for x in split_top_level(m.group(1))]
        if len(nums) >= 2 and nums[0] is not None and nums[1] is not None:
            out["min"], out["max"] = nums[0], nums[1]
            if len(nums) >= 4 and nums[3] is not None and nums[3] != 1:
                out["scale"] = "log"
                out["skew"] = nums[3]
    # default = first plain numeric argument after the range argument
    range_idx = next((i for i, a in enumerate(args) if "NormalisableRange" in a), None)
    if range_idx is not None and range_idx + 1 < len(args):
        d = as_number(args[range_idx + 1])
        if d is not None:
            out["default"] = d
        for a in args[range_idx + 2 :]:
            lab = LABEL_RE.search(a)
            if lab:
                out["unit"] = lab.group(1)
                break
            s = a.strip()
            if s.startswith('"') and s.endswith('"'):
                out["unit"] = s[1:-1]
                break
    return out


def _parse_choice(args: list[str]) -> dict:
    out: dict = {"type": "enum"}
    for i, a in enumerate(args[2:], start=2):
        if "StringArray" in a:
            opts = all_strings(a)
            if len(opts) >= 2:
                out["options"] = opts
            if i + 1 < len(args):
                d = as_number(args[i + 1])
                if d is not None and "options" in out and 0 <= int(d) < len(out["options"]):
                    out["default"] = out["options"][int(d)]
            break
    return out


def _parse_bool(args: list[str]) -> dict:
    out: dict = {"type": "bool"}
    if len(args) > 2:
        v = args[2].strip()
        if v in ("true", "false"):
            out["default"] = v == "true"
    return out


def _parse_int(args: list[str]) -> dict:
    out: dict = {"type": "int"}
    nums = [as_number(a) for a in args[2:5]]
    if len(nums) == 3 and all(n is not None for n in nums):
        out["min"], out["max"], out["default"] = nums
    return out


PARSERS = {"Float": _parse_float, "Choice": _parse_choice, "Bool": _parse_bool, "Int": _parse_int}
REQUIRED = {
    "float": ("min", "max", "default"),
    "enum": ("options", "default"),
    "bool": ("default",),
    "int": ("min", "max", "default"),
}


def extract(root: Path, cfg: dict) -> dict:
    path = root / cfg["file"]
    src = strip_comments(path.read_text(encoding="utf-8", errors="replace"))
    if "function" in cfg:
        start = src.find(cfg["function"])
        if start == -1:
            raise ValueError(f"function {cfg['function']!r} not found in {cfg['file']}")
        brace = src.find("{", start)
        body, _ = balanced(src, brace, "{", "}")
        src = body
    # Plugins sometimes subclass juce::AudioParameterBool (e.g. Dusk's SnappingBoolParameter).
    bool_classes = cfg.get("bool_classes", [])
    pattern = CALL_RE
    if bool_classes:
        alts = "|".join(re.escape(c) for c in bool_classes)
        pattern = rf"(?:AudioParameter(Float|Choice|Bool|Int)|(?:{alts}))\s*>\s*\("
    params, seen = [], set()
    for m, inner in find_calls(src, pattern):
        kind = m.group(1) or "Bool"
        args = split_top_level(inner)
        pid, name = _id_and_name(args)
        p = {"upstream_id": pid, "name": name}
        p.update(PARSERS[kind](args))
        missing = [k for k in REQUIRED[p["type"]] if k not in p] + [k for k in ("upstream_id", "name") if not p.get(k)]
        if missing:
            p["unresolved"] = True
            p["missing"] = missing
            p["raw"] = re.sub(r"\s+", " ", inner)[:300]
        if pid:
            if pid in seen:
                continue
            seen.add(pid)
        params.append(p)
    return {"params": params}
