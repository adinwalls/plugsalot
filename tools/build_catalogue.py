#!/usr/bin/env python3
"""Merge catalogue/raw/ (what upstream declares) with catalogue/curation/ (human
judgement) into catalogue/modules/<id>.json, plus catalogue/index.json and
THIRD_PARTY.md.

Curation can only narrow what upstream allows: a curated range must sit inside the
upstream range and curated options must be a subset of upstream options. Anything
else is a build error, so a bumped upstream commit that changes a range is caught.

    python3 tools/build_catalogue.py
"""

from __future__ import annotations

import copy
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

from extractors.common import snake  # noqa: E402

CAT = REPO / "catalogue"
PARAM_KEYS = ("id", "upstream_id", "name", "type", "min", "max", "default", "unit", "scale", "options", "convert", "ai_hint")
OVERRIDABLE = ("id", "name", "unit", "min", "max", "default", "options", "ai_hint", "convert", "scale")
COPY_KEYS = ("id", "name", "kind", "category", "purpose", "good_for", "avoid_for", "tags", "cpu_points", "latency",
             "tracking_safe", "tracking_overrides", "channels", "file_kind", "notes")


class BuildError(Exception):
    pass


def expand(expose: list[dict]) -> list[dict]:
    out = []
    for item in expose:
        if "repeat" in item:
            for i in item["repeat"]:
                for tmpl in item["items"]:
                    out.append(json.loads(json.dumps(tmpl).replace("{i}", str(i))))
        else:
            out.append(item)
    return out


def merge_param(raw: dict | None, cur: dict, module: str) -> dict:
    uid = cur["upstream_id"]
    if raw is None:
        raise BuildError(f"{module}: upstream parameter {uid!r} not found in raw extraction")
    if raw.get("unresolved"):
        missing = [k for k in raw.get("missing", []) if k not in cur]
        if missing:
            raise BuildError(f"{module}: {uid!r} is unresolved upstream; curation must supply {missing}")
    p = {k: copy.deepcopy(raw[k]) for k in PARAM_KEYS if k in raw}
    p["upstream_id"] = uid
    p.setdefault("id", snake(uid))
    p.setdefault("name", uid)
    if raw.get("convert_hint") and "convert" not in p:
        p["convert"] = raw["convert_hint"]

    for k in OVERRIDABLE:
        if k in cur:
            new = cur[k]
            if k in ("min", "max") and p.get("type") in ("float", "int") and k in raw:
                lo, hi = raw.get("min"), raw.get("max")
                if lo is not None and hi is not None and not (lo <= new <= hi):
                    raise BuildError(f"{module}.{uid}: curated {k}={new} outside upstream range {lo}..{hi}")
            if k == "options" and "options" in raw:
                extra = [o for o in new if o not in raw["options"]]
                if extra:
                    raise BuildError(f"{module}.{uid}: options {extra} not offered upstream")
            p[k] = new

    t = p.get("type")
    if t in ("float", "int"):
        p.setdefault("unit", "")
        if not (p["min"] <= p["default"] <= p["max"]):
            raise BuildError(f"{module}.{uid}: default {p['default']} outside {p['min']}..{p['max']}; set a curated default")
    if t == "enum":
        if p.get("default") not in p["options"]:
            raise BuildError(f"{module}.{uid}: default {p.get('default')!r} not in options; set a curated default")
        p.setdefault("convert", "enum_index")
        # Narrowed option lists must still send upstream's original index.
        if "options" in raw:
            idx = [raw["options"].index(o) for o in p["options"]]
            if idx != list(range(len(idx))):
                p["option_values"] = idx
    return {k: p[k] for k in PARAM_KEYS + ("option_values",) if k in p}


def build_module(cur: dict, raw: dict | None, sources: dict) -> dict:
    module = cur["id"]
    src_key = cur["source"]["source_key"]
    spec = sources[src_key]
    if raw is not None and raw["commit"] != spec["commit"]:
        raise BuildError(f"{module}: raw extracted at {raw['commit'][:10]} but sources.json pins {spec['commit'][:10]}; re-run extract_all.py")

    if cur.get("extractor", {}).get("type") == "manual":
        params = cur["params"]
        hidden = None
    else:
        by_id = {p["upstream_id"]: p for p in raw["params"] if p.get("upstream_id")}
        params = [merge_param(by_id.get(c["upstream_id"]), c, module) for c in expand(cur["expose"])]
        controls = [p for p in raw["params"] if p.get("role") != "meter"]
        hidden = {"count": len(controls) - len(params)}
        if cur.get("hidden_reason"):
            hidden["reason"] = cur["hidden_reason"]

    ids = [p["id"] for p in params]
    dupes = {i for i in ids if ids.count(i) > 1}
    if dupes:
        raise BuildError(f"{module}: duplicate parameter ids {sorted(dupes)}")

    out: dict = {"schema_version": 1}
    for k in COPY_KEYS:
        if k in cur:
            out[k] = cur[k]
    out["cpu_measured"] = bool(cur.get("cpu_measured", False))
    out["source"] = {
        "source_key": src_key,
        "repo": spec["repo"],
        "commit": spec["commit"],
        "path": cur["source"]["path"],
        "upstream_name": cur["source"].get("upstream_name", cur["name"]),
        "license": spec["license"],
        "authors": spec["authors"],
        "extractor": cur.get("extractor", {}).get("type", "manual"),
    }
    out["params"] = params
    if hidden is not None:
        out["hidden_params"] = hidden

    if cur.get("presets") == "from_raw" and raw and raw.get("presets"):
        id_map = {p["upstream_id"]: p["id"] for p in params}
        out["presets"] = [
            {"name": pr["name"], "params": {id_map[k]: v for k, v in pr["params"].items() if k in id_map}}
            for pr in raw["presets"]
        ]
    if "description" in (raw or {}):
        out.setdefault("notes", []).insert(0, f"Upstream description: {raw['description']}")

    order = ["schema_version", "id", "name", "kind", "category", "source", "purpose", "good_for", "avoid_for", "tags",
             "cpu_points", "cpu_measured", "latency", "tracking_safe", "tracking_overrides", "channels", "file_kind", "params",
             "hidden_params", "presets", "notes"]
    return {k: out[k] for k in order if k in out}


def third_party(modules: list[dict], sources: dict) -> str:
    lines = [
        "# Third-party code",
        "",
        "Plugsalot is GPL-3.0-or-later. It includes processing code ported from the open-source projects below,",
        "each used under its own licence. Original copyright notices are kept in every ported file.",
        "",
        "This file is generated by `tools/build_catalogue.py`; do not edit by hand.",
        "",
        "| Module | Upstream | Authors | Licence | Source (pinned commit) |",
        "| --- | --- | --- | --- | --- |",
    ]
    for m in sorted(modules, key=lambda m: m["id"]):
        s = m["source"]
        lines.append(f"| `{m['id']}` | {s['upstream_name']} | {s['authors']} | {s['license']} | [{s['commit'][:10]}]({s['repo']}/tree/{s['commit']}) |")
    used = {m["source"]["source_key"] for m in modules}
    bundled = [k for k in sorted(used) if sources[k].get("includes")]
    if bundled:
        lines += ["", "Bundled libraries inside those upstreams:", ""]
        for k in bundled:
            lines.append(f"- {k}: {sources[k]['includes']}")
    support = [k for k in sources if k not in used and sources[k].get("note")]
    if support:
        lines += ["", "Supporting sources (constants only, not ported code):", ""]
        for k in support:
            s = sources[k]
            lines.append(f"- {k}: {s['repo']} at `{s['commit'][:10]}` ({s['license']}). {s['note']}")
    return "\n".join(lines) + "\n"


def main() -> int:
    sources = json.loads((REPO / "third_party" / "sources.json").read_text())["sources"]
    (CAT / "modules").mkdir(parents=True, exist_ok=True)
    modules, errors = [], []
    for cf in sorted((CAT / "curation").glob("*.json")):
        cur = json.loads(cf.read_text())
        if cur["id"] != cf.stem:
            errors.append(f"{cf.name}: id {cur['id']!r} does not match file name")
            continue
        raw_path = CAT / "raw" / f"{cf.stem}.json"
        raw = json.loads(raw_path.read_text()) if raw_path.exists() else None
        if raw is None and cur.get("extractor", {}).get("type") != "manual":
            errors.append(f"{cf.stem}: no raw extraction; run tools/extract_all.py")
            continue
        try:
            mod = build_module(cur, raw, sources)
        except BuildError as e:
            errors.append(str(e))
            continue
        (CAT / "modules" / f"{mod['id']}.json").write_text(json.dumps(mod, indent=2, ensure_ascii=False) + "\n")
        modules.append(mod)
        print(f"  {mod['id']}: {len(mod['params'])} exposed params")

    index = {
        "schema_version": 1,
        "modules": [
            {k: m[k] for k in ("id", "name", "kind", "category", "purpose", "good_for", "cpu_points", "tracking_safe") if k in m}
            for m in sorted(modules, key=lambda m: (m["category"], m["id"]))
        ],
    }
    (CAT / "index.json").write_text(json.dumps(index, indent=2, ensure_ascii=False) + "\n")
    (REPO / "THIRD_PARTY.md").write_text(third_party(modules, sources))

    for e in errors:
        print(f"  ERROR {e}", file=sys.stderr)
    print(f"Built {len(modules)} module(s), {len(errors)} error(s)")
    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
