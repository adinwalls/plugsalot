#!/usr/bin/env python3
"""Validate the built catalogue: schema, licences, overrides, presets, and the
example chains in catalogue/examples/.

    python3 tools/validate.py
"""

from __future__ import annotations

import json
import sys
from pathlib import Path

import jsonschema

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

from chain_validator import check_param, load_catalogue, validate_chain  # noqa: E402

CAT = REPO / "catalogue"


def main() -> int:
    schema = json.loads((CAT / "schema" / "module.schema.json").read_text())
    sources = json.loads((REPO / "third_party" / "sources.json").read_text())["sources"]
    problems: list[str] = []

    catalogue = load_catalogue()
    for path in sorted((CAT / "modules").glob("*.json")):
        mod = json.loads(path.read_text())
        name = path.stem
        try:
            jsonschema.validate(mod, schema)
        except jsonschema.ValidationError as e:
            problems.append(f"{name}: schema: {'/'.join(map(str, e.absolute_path))}: {e.message}")
            continue
        if mod["id"] != name:
            problems.append(f"{name}: id {mod['id']!r} does not match file name")
        src = sources.get(mod["source"]["source_key"])
        if src is None:
            problems.append(f"{name}: unknown source {mod['source']['source_key']!r}")
        else:
            if mod["source"]["license"] != src["license"]:
                problems.append(f"{name}: licence {mod['source']['license']} differs from sources.json {src['license']}")
            if mod["source"]["commit"] != src["commit"]:
                problems.append(f"{name}: commit differs from sources.json; rebuild")
        for pid, value in mod.get("tracking_overrides", {}).items():
            problems += check_param(mod, pid, value, f"{name}.tracking_overrides")
        for preset in mod.get("presets", []):
            for pid, value in preset["params"].items():
                problems += check_param(mod, pid, value, f"{name}.preset '{preset['name']}'")
        if mod["cpu_points"] is None:
            print(f"  note: {name} has no measured CPU cost yet")

    examples = sorted((CAT / "examples").glob("*.json"))
    for ex in examples:
        case = json.loads(ex.read_text())
        errors, _ = validate_chain(case["reply"], catalogue, case.get("cpu_mode", "normal"))
        expect_ok = case["expect"] == "valid"
        if expect_ok and errors:
            problems.append(f"example {ex.stem}: expected valid, got {errors}")
        if not expect_ok:
            needle = case.get("expect_error_contains", "")
            if not errors:
                problems.append(f"example {ex.stem}: expected an error, chain passed")
            elif needle and not any(needle in e for e in errors):
                problems.append(f"example {ex.stem}: expected error containing {needle!r}, got {errors}")

    for p in problems:
        print(f"  FAIL {p}")
    print(f"Checked {len(catalogue)} module(s) and {len(examples)} example chain(s): {len(problems)} problem(s)")
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
