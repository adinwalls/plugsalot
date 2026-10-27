"""Shared helpers for the parameter extractors.

Extractors read upstream C/C++/Turtle source as plain text. They never compile or
execute upstream code. Anything an extractor cannot resolve from literals is kept
with "unresolved": true so a human can fill it in via the curation file.
"""

from __future__ import annotations

import re
from typing import Iterator

_NUM = r"[-+]?(?:\d+\.?\d*|\.\d+)(?:[eE][-+]?\d+)?"
NUM_RE = re.compile(_NUM)
STRING_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def strip_comments(src: str) -> str:
    """Remove // and /* */ comments while leaving string literals intact."""
    out, i, n = [], 0, len(src)
    while i < n:
        c = src[i]
        if c == '"':
            j = i + 1
            while j < n and src[j] != '"':
                j += 2 if src[j] == "\\" else 1
            out.append(src[i : j + 1])
            i = j + 1
        elif c == "'" and i + 2 < n:
            j = i + 1
            while j < n and src[j] != "'":
                j += 2 if src[j] == "\\" else 1
            out.append(src[i : j + 1])
            i = j + 1
        elif src.startswith("//", i):
            j = src.find("\n", i)
            i = n if j == -1 else j
        elif src.startswith("/*", i):
            j = src.find("*/", i + 2)
            i = n if j == -1 else j + 2
        else:
            out.append(c)
            i += 1
    return "".join(out)


def balanced(src: str, start: int, open_ch: str = "(", close_ch: str = ")") -> tuple[str, int]:
    """Return (inner_text, end_index) for the bracket group opening at src[start]."""
    assert src[start] == open_ch, f"expected {open_ch!r} at {start}"
    depth, i, n = 0, start, len(src)
    in_str = False
    while i < n:
        c = src[i]
        if in_str:
            if c == "\\":
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
        elif c == open_ch:
            depth += 1
        elif c == close_ch:
            depth -= 1
            if depth == 0:
                return src[start + 1 : i], i
        i += 1
    raise ValueError("unbalanced brackets")


def split_top_level(args: str, sep: str = ",") -> list[str]:
    """Split on separators that are not nested inside (), {}, [], <> or strings."""
    parts, depth, cur, in_str, i = [], 0, [], False, 0
    pairs = {"(": ")", "{": "}", "[": "]"}
    closers = set(pairs.values())
    while i < len(args):
        c = args[i]
        if in_str:
            cur.append(c)
            if c == "\\" and i + 1 < len(args):
                cur.append(args[i + 1])
                i += 2
                continue
            if c == '"':
                in_str = False
        elif c == '"':
            in_str = True
            cur.append(c)
        elif c in pairs:
            depth += 1
            cur.append(c)
        elif c in closers:
            depth -= 1
            cur.append(c)
        elif c == sep and depth == 0:
            parts.append("".join(cur).strip())
            cur = []
        else:
            cur.append(c)
        i += 1
    tail = "".join(cur).strip()
    if tail:
        parts.append(tail)
    return parts


def first_string(text: str) -> str | None:
    m = STRING_RE.search(text)
    return m.group(1) if m else None


def all_strings(text: str) -> list[str]:
    return [m.group(1) for m in STRING_RE.finditer(text)]


def as_number(text: str) -> float | None:
    """Parse a C numeric literal like '-12.0f' or '20'. None if not a plain literal."""
    t = text.strip().rstrip("fF")
    if re.fullmatch(_NUM, t):
        v = float(t)
        return int(v) if v.is_integer() and "." not in t and "e" not in t.lower() else v
    return None


def find_calls(src: str, pattern: str) -> Iterator[tuple[re.Match, str]]:
    """Yield (match, argument_text) for each `pattern(` ... `)` call in src."""
    for m in re.finditer(pattern, src):
        j = src.find("(", m.end() - 1)
        if j == -1:
            continue
        try:
            inner, _ = balanced(src, j)
        except ValueError:
            continue
        yield m, inner


def snake(name: str) -> str:
    s = re.sub(r"([a-z0-9])([A-Z])", r"\1_\2", name)
    s = re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_").lower()
    return s or "param"
