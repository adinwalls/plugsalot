#!/usr/bin/env python3
"""Run the parameter extractor for every curation file and write catalogue/raw/.

Each catalogue/curation/<id>.json names its source and extractor. The raw output
is everything the upstream code declares, untouched by human judgement; review
its diff whenever a source commit is bumped.

    python3 tools/extract_all.py                 # every module
    python3 tools/extract_all.py multicomp       # one module
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "tools"))

from extractors import EXTRACTORS  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("ids", nargs="*")
    ap.add_argument("--cache-dir", type=Path, default=REPO / ".cache" / "upstream")
    args = ap.parse_args()

    sources = json.loads((REPO / "third_party" / "sources.json").read_text())["sources"]
    roots = {k: args.cache_dir / k for k in sources}
    out_dir = REPO / "catalogue" / "raw"
    out_dir.mkdir(parents=True, exist_ok=True)

    curation_files = sorted((REPO / "catalogue" / "curation").glob("*.json"))
    if args.ids:
        curation_files = [f for f in curation_files if f.stem in args.ids]

    failures = 0
    for cf in curation_files:
        cur = json.loads(cf.read_text())
        ex = cur.get("extractor")
        if not ex or ex["type"] == "manual":
            print(f"  {cf.stem}: manual (params written in curation file)")
            continue
        key = cur["source"]["source_key"]
        root = roots[key]
        if not root.exists():
            print(f"  {cf.stem}: source {key} not fetched; run tools/fetch_sources.py", file=sys.stderr)
            failures += 1
            continue
        mod = EXTRACTORS[ex["type"]]
        try:
            result = mod.extract(root, ex, roots) if getattr(mod, "NEEDS_ROOTS", False) else mod.extract(root, ex)
        except Exception as e:  # report and keep going so one bad module doesn't block the rest
            print(f"  {cf.stem}: FAILED ({e})", file=sys.stderr)
            failures += 1
            continue
        raw = {
            "module": cf.stem,
            "source_key": key,
            "commit": sources[key]["commit"],
            "extractor": ex,
            **result,
        }
        (out_dir / f"{cf.stem}.json").write_text(json.dumps(raw, indent=2, ensure_ascii=False) + "\n")
        unresolved = sum(1 for p in result["params"] if p.get("unresolved"))
        note = f", {unresolved} unresolved" if unresolved else ""
        print(f"  {cf.stem}: {len(result['params'])} params{note}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
