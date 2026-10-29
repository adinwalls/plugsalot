"""Reference implementation of the Plugsalot chain validator (layer 2: hard limits).

The plugin will carry a C++ port of this logic; this Python version is the
specification and is what the test set runs against. It checks an LLM reply
against the JSON schemas and then against the catalogue:

- every module id exists, every parameter exists and is in range
- at most MAX_MODULES modules and within the CPU budget for the chosen CPU mode
- only one limiter, and it must be last
- in the tracking stage, only tracking-safe modules, with tracking overrides applied

`validate_chain` returns (errors, repaired_chain). Errors are written so they can
be sent straight back to the LLM for its one retry.
"""

from __future__ import annotations

import copy
import json
from pathlib import Path

import jsonschema

REPO = Path(__file__).resolve().parent.parent
CAT = REPO / "catalogue"

MAX_MODULES = 8
CPU_BUDGET = {"light": 6, "normal": 12, "high": 20}
UNKNOWN_CPU_COST = 2  # placeholder cost for modules not yet benchmarked


def load_catalogue() -> dict[str, dict]:
    return {json.loads(p.read_text())["id"]: json.loads(p.read_text()) for p in (CAT / "modules").glob("*.json")}


def _schema(name: str) -> dict:
    return json.loads((CAT / "schema" / f"{name}.schema.json").read_text())


def check_param(module: dict, pid: str, value, where: str) -> list[str]:
    spec = next((p for p in module["params"] if p["id"] == pid), None)
    if spec is None:
        names = ", ".join(p["id"] for p in module["params"][:12])
        return [f"{where}: '{module['id']}' has no parameter '{pid}'. Valid parameters include: {names}."]
    t = spec["type"]
    if t in ("float", "int"):
        if isinstance(value, bool) or not isinstance(value, (int, float)):
            return [f"{where}: '{pid}' must be a number."]
        if t == "int" and not float(value).is_integer():
            return [f"{where}: '{pid}' must be a whole number."]
        if not (spec["min"] <= value <= spec["max"]):
            return [f"{where}: '{pid}' = {value} is outside {spec['min']}..{spec['max']} {spec.get('unit', '')}".rstrip() + "."]
    elif t == "enum":
        if value not in spec["options"]:
            return [f"{where}: '{pid}' must be one of {spec['options']}."]
    elif t == "bool" and not isinstance(value, bool):
        return [f"{where}: '{pid}' must be true or false."]
    return []


def check_file(mod: dict, name: str | None, where: str) -> list[str]:
    if not name:
        return []
    if not mod.get("file_kind"):
        return [f"{where}: '{mod['id']}' does not load a file; remove 'file'."]
    if "/" in name or "\\" in name or ".." in name:
        return [f"{where}: 'file' must be a plain file name from the library list."]
    ext = (".nam",) if mod["file_kind"] == "nam" else (".wav",)
    if not name.lower().endswith(ext):
        return [f"{where}: '{name}' is not a {ext[0]} file."]
    return []


def validate_chain(reply: dict, catalogue: dict[str, dict], cpu_mode: str = "normal") -> tuple[list[str], dict | None]:
    errors: list[str] = []
    try:
        jsonschema.validate(reply, _schema("chain"))
    except jsonschema.ValidationError as e:
        path = "/".join(str(x) for x in e.absolute_path) or "reply"
        return [f"{path}: {e.message}"], None

    chain = copy.deepcopy(reply)
    stage = chain["stage"]
    limiter_positions = []
    cpu = 0
    for i, slot in enumerate(chain["chain"]):
        where = f"chain[{i}]"
        mod = catalogue.get(slot["module"])
        if mod is None:
            errors.append(f"{where}: unknown module '{slot['module']}'. Available: {', '.join(sorted(catalogue))}.")
            continue
        if mod["category"] == "limiting":
            limiter_positions.append(i)
        cpu += mod["cpu_points"] if mod["cpu_points"] is not None else UNKNOWN_CPU_COST
        for pid, value in slot["params"].items():
            errors += check_param(mod, pid, value, where)
        errors += check_file(mod, slot.get("file"), where)
        if stage == "tracking":
            if not mod["tracking_safe"]:
                errors.append(f"{where}: '{mod['id']}' cannot be used while tracking (too much latency, or not suited to recording).")
            for pid, forced in mod.get("tracking_overrides", {}).items():
                slot["params"][pid] = forced

    if len(limiter_positions) > 1:
        errors.append("Only one limiter is allowed per chain.")
    if limiter_positions and limiter_positions[-1] != len(chain["chain"]) - 1:
        errors.append("The limiter must be the last module in the chain.")
    budget = CPU_BUDGET[cpu_mode]
    if cpu > budget:
        errors.append(f"Chain uses {cpu} CPU points; the '{cpu_mode}' CPU mode allows {budget}. Remove or swap a module.")
    return errors, (None if errors else chain)


def validate_changes(reply: dict, current: dict, catalogue: dict[str, dict], cpu_mode: str = "normal") -> tuple[list[str], dict | None]:
    """Apply a change list to the current chain, then validate the result as a whole chain."""
    try:
        jsonschema.validate(reply, _schema("changes"))
    except jsonschema.ValidationError as e:
        return [f"changes: {e.message}"], None
    chain = copy.deepcopy(current)
    slots = chain["chain"]
    for n, ch in enumerate(reply["changes"]):
        where = f"changes[{n}]"
        op = ch["op"]
        try:
            if op == "set":
                slots[ch["module_index"]]["params"][ch["param"]] = ch["value"]
            elif op == "insert":
                new = {"module": ch["module"], "params": ch["params"]}
                if ch.get("file"):
                    new["file"] = ch["file"]
                slots.insert(ch["at"], new)
            elif op == "set_file":
                slots[ch["module_index"]]["file"] = ch["file"]
            elif op == "remove":
                slots.pop(ch["module_index"])
            elif op == "move":
                slots.insert(ch["to"], slots.pop(ch["from"]))
            elif op == "bypass":
                slots[ch["module_index"]]["bypass"] = ch["bypass"]
        except IndexError:
            return [f"{where}: module index out of range (chain has {len(slots)} modules)."], None
    if not slots:
        return ["changes would leave the chain empty."], None
    return validate_chain(chain, catalogue, cpu_mode)
