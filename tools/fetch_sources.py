#!/usr/bin/env python3
"""Fetch every upstream source in third_party/sources.json at its pinned commit.

Sources land in .cache/upstream/<source_key>/ (git-ignored). Re-running is cheap:
a source already at the right commit is skipped.

    python3 tools/fetch_sources.py            # all sources
    python3 tools/fetch_sources.py dusk-audio  # just one
"""

from __future__ import annotations

import argparse
import json
import subprocess
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent


def git(*args: str, cwd: Path | None = None) -> str:
    return subprocess.run(["git", *args], cwd=cwd, check=True, capture_output=True, text=True).stdout.strip()


def fetch(key: str, spec: dict, cache: Path) -> None:
    dest = cache / key
    if (dest / ".git").exists():
        try:
            if git("rev-parse", "HEAD", cwd=dest) == spec["commit"]:
                print(f"  {key}: up to date")
                return
        except subprocess.CalledProcessError:
            pass
    dest.mkdir(parents=True, exist_ok=True)
    if not (dest / ".git").exists():
        git("init", "-q", cwd=dest)
        git("remote", "add", "origin", spec["repo"], cwd=dest)
    sparse = spec.get("sparse_paths")
    if sparse:
        git("config", "core.sparseCheckout", "true", cwd=dest)
        (dest / ".git" / "info").mkdir(exist_ok=True)
        (dest / ".git" / "info" / "sparse-checkout").write_text("\n".join(sparse) + "\n")
        git("fetch", "-q", "--depth", "1", "--filter=blob:none", "origin", spec["commit"], cwd=dest)
    else:
        git("fetch", "-q", "--depth", "1", "origin", spec["commit"], cwd=dest)
    git("checkout", "-q", "FETCH_HEAD", cwd=dest)
    print(f"  {key}: {spec['commit'][:10]}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("keys", nargs="*", help="source keys to fetch (default: all)")
    ap.add_argument("--cache-dir", type=Path, default=REPO / ".cache" / "upstream")
    args = ap.parse_args()
    sources = json.loads((REPO / "third_party" / "sources.json").read_text())["sources"]
    keys = args.keys or list(sources)
    unknown = [k for k in keys if k not in sources]
    if unknown:
        print(f"unknown source key(s): {', '.join(unknown)}", file=sys.stderr)
        return 2
    print(f"Fetching {len(keys)} source(s) into {args.cache_dir}")
    for k in keys:
        fetch(k, sources[k], args.cache_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
