#!/usr/bin/env python3
"""Check that Xapiand declares only the Kronuz libraries it includes, each at the head all pin.

usage: pins_check.py [--deps DIR] [--strict]

DIR is the FetchContent base dir of a configured build (default .deps; its <name>-src trees are
the resolved libraries). Prints every Kronuz declaration in CMakeLists.txt with its pin and an
include site, then the pins of every resolved library for the same names.

  A1  each declaration names a library a source in src/ or tests/ includes directly
  A2  no comment in CMakeLists.txt says a declaration is there to win over another pin
  A3  Xapiand's pin of each library is the newest any resolved library pins (the others are
      listed as stale pins in their libraries; --strict fails on those too)

Exit 0 when A1-A3 hold on Xapiand's side (and, with --strict, in every library).
"""

from __future__ import annotations

import argparse
import collections
import re
import subprocess
import sys
from pathlib import Path

DECLARE = re.compile(r"FetchContent_Declare\(\s*(\w+)((?:[^()]|\([^()]*\))*?)\)", re.DOTALL)
INCLUDE = re.compile(r'\s*#\s*include\s*[<"]([^>"]+)[>"]')
OVERRIDE_WORDS = re.compile(r"declared first|first[- ]declaration|\bwins\b|pins that count", re.IGNORECASE)
HEADERS = (".h", ".hh", ".hpp")
SOURCES = (".h", ".hh", ".hpp", ".c", ".cc", ".cpp")
SKIP_DIRS = {".git", "tests", "test", "build", "bench", "benchmarks", "examples", "tools", "doc", "docs"}


def declarations(path: Path) -> dict[str, tuple[str, str | None]]:
    text = re.sub(r"(?m)^\s*#.*$", "", path.read_text(errors="replace"))
    found: dict[str, tuple[str, str | None]] = {}
    for m in DECLARE.finditer(text):
        repo = re.search(r"GIT_REPOSITORY\s+(\S+)", m.group(2))
        tag = re.search(r"GIT_TAG\s+(\S+)", m.group(2))
        if repo and "github.com/Kronuz/" in repo.group(1):
            found.setdefault(m.group(1), (repo.group(1), tag.group(1) if tag else None))
    return found


def walk(root: Path, suffixes: tuple[str, ...]) -> list[Path]:
    return [
        p
        for p in root.rglob("*")
        if p.is_file() and p.suffix in suffixes and not SKIP_DIRS & set(p.relative_to(root).parts[:-1])
    ]


def include_sites(top: Path) -> dict[str, str]:
    sites: dict[str, str] = {}
    for sub in ("src", "tests"):
        for p in sorted(walk(top / sub, SOURCES)):
            for line in p.read_text(errors="replace").splitlines():
                m = INCLUDE.match(line)
                if m:
                    sites.setdefault(m.group(1), str(p.relative_to(top)))
    return sites


def header_owners(deps: Path, names: list[str]) -> dict[str, set[str]]:
    owners: dict[str, set[str]] = collections.defaultdict(set)
    for name in names:
        tree = deps / f"{name}-src"
        for p in walk(tree, HEADERS) if tree.is_dir() else []:
            parts = p.relative_to(tree).parts
            for i in range(len(parts)):
                owners["/".join(parts[i:])].add(name)
    return owners


def is_ancestor(deps: Path, name: str, older: str, newer: str) -> bool:
    r = subprocess.run(
        ["git", "-C", str(deps / f"{name}-src"), "merge-base", "--is-ancestor", older, newer],
        capture_output=True,
        check=False,
    )
    return r.returncode == 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--deps", default=".deps", type=Path)
    ap.add_argument("--strict", action="store_true", help="also fail on stale pins inside libraries")
    args = ap.parse_args()
    top = Path(__file__).resolve().parent.parent
    deps: Path = args.deps if args.deps.is_absolute() else Path.cwd() / args.deps

    ours = declarations(top / "CMakeLists.txt")
    sites = include_sites(top)
    owners = header_owners(deps, list(ours))
    failures: list[str] = []

    print("A1: declarations and include sites")
    for name, (_, tag) in ours.items():
        own = sorted(h for h, who in owners.items() if who == {name} and h in sites)
        if not own:
            own = sorted(h for h, who in owners.items() if name in who and h in sites)
        if own:
            print(f"  {name:20} {str(tag)[:10]}  {own[0]}  <- {sites[own[0]]}")
        else:
            print(f"  {name:20} {str(tag)[:10]}  NO INCLUDE SITE")
            failures.append(f"A1 {name}: no Xapiand source includes it")

    print("A2: comments that justify a declaration by winning over another pin")
    for n, line in enumerate((top / "CMakeLists.txt").read_text().splitlines(), 1):
        if line.lstrip().startswith("#") and OVERRIDE_WORDS.search(line):
            print(f"  CMakeLists.txt:{n}: {line.strip()}")
            failures.append(f"A2 CMakeLists.txt:{n}")

    print("A3: pins per library")
    pins: dict[str, list[tuple[str, str]]] = collections.defaultdict(list)
    for name, (_, tag) in ours.items():
        pins[name].append(("xapiand", str(tag)))
    for tree in sorted(deps.glob("*-src")):
        for cm in sorted(walk(tree, (".txt", ".cmake"))):
            if cm.name != "CMakeLists.txt" and cm.suffix != ".cmake":
                continue
            for name, (_, tag) in declarations(cm).items():
                if tag:
                    pins[name].append((tree.name[:-4], tag))
    stale = 0
    for name, v in sorted(pins.items()):
        tags = sorted({t for _, t in v})
        if len(tags) == 1:
            continue
        head = next((t for t in tags if all(o == t or is_ancestor(deps, name, o, t) for o in tags)), None)
        print(f"  {name}: head {head[:10] if head else 'UNORDERED'}")
        for who, tag in v:
            if tag == head:
                continue
            kind = "xapiand" if who == "xapiand" else "library"
            print(f"    {kind:8} {who:18} pins {tag[:10]}")
            if who == "xapiand" or head is None:
                failures.append(f"A3 {name}: xapiand pins {tag[:10]}, head {head[:10] if head else '?'}")
            else:
                stale += 1
                if args.strict:
                    failures.append(f"A3 {name}: {who} pins {tag[:10]}, head {head[:10]}")
    print(f"  {stale} stale pin(s) inside libraries")

    for f in failures:
        print("FAIL", f)
    print("pins_check:", "FAILED" if failures else "ok")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
