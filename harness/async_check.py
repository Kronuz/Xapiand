#!/usr/bin/env python3
"""async_check.py -- Phase 7c D14: every blocking primitive on a reactor-reachable path is classified.

Scans the C++ sources under src/ (and any --extern library tree) with rules for the blocking
classes (sync_wait, condition variables and waits, mutexes, sleeps and yields, file and storage
syscalls, pool checkouts) and puts every hit in one of four classes through
harness/async_allowlist.tsv:

  allow         O(1) to O(log n) work on an in-memory structure, with the reason it is cheap.
  owed          removed by a named 7c decision (D1 to D16); the note names it.
  compiled_out  not built into the gate binaries; the note names the switch.
  unclassified  anything else.

Paths that step X1 deletes (src/xapian/, src/engine/xapian/, the remote and replication
protocol files) and the files the GPL fork owns are never opened; they are counted as compiled out.

  harness/async_check.sh                  report the classes (exit 0)
  harness/async_check.sh --strict         exit 1 on an unclassified hit or a stale allowlist entry
  harness/async_check.sh --strict --owed  also exit 1 on an owed hit (the gate after X6)
  harness/async_check.sh --list           every hit with its file:line, class and note
  harness/async_check.sh --json           the same as JSON

Allowlist rows (tab separated): file, rule, anchor, class, count, note. The anchor is a regex
searched in the hit's source line; the row's count is the most hits it may cover, so a new hit in a
covered file fails instead of hiding behind an old row, while a removal only reports "shrunk". --extern NAME=DIR scans the
library tree DIR for rows whose file starts with NAME/.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
from dataclasses import dataclass, field

# Paths never opened: step X1 deletes them, or the owner keeps them off limits.
NEVER_OPEN: tuple[str, ...] = (
    r"^src/xapian/",
    r"^src/engine/xapian/",
    r"^src/sortable_serialise\.",
    r"^src/server/remote_protocol_",
    r"^src/server/replication_protocol_",
)

SOURCE_SUFFIXES: tuple[str, ...] = (".cc", ".h", ".hh", ".cpp", ".hpp", ".c", ".ipp")

# rule name -> regular expression, searched in a line with comments and string literals removed.
RULES: dict[str, str] = {
    "sync_wait": r"\bsync_wait\b",
    "wait": (
        r"\bcondition_variable\b|\.wait\(|\.wait_for\(|\.wait_until\(|\.join\(\)|\bstd::future\b|"
        r"\bstd::promise\b|\bstd::async\(|\bstd::latch\b|\bstd::barrier\b|semaphore|\bwait_all\(|\bdrain\("
    ),
    "mutex": (
        r"\bstd::(mutex|shared_mutex|recursive_mutex|timed_mutex|shared_timed_mutex)\b|\block_guard\b|"
        r"\bunique_lock\b|\bscoped_lock\b|\bshared_lock\b|\.lock\(\)|\.try_lock\(\)|\bcall_once\b|"
        r"\bstd::lock\("
    ),
    "sleep": r"\bsleep_for\b|\bsleep_until\b|\busleep\(|\bnanosleep\(|(?<![\w.:])sleep\(|\byield\(\)|\bsched_yield\(",
    "file": (
        r"\bstd::(i|o)?fstream\b|\bfopen\(|\bfread\(|\bfwrite\(|\bfflush\(|\bfclose\(|\bstd::filesystem\b|"
        r"\bfs::|(?<![\w>])::(open|read|write|pread|pwrite|fsync|fdatasync|close|unlink|rename|stat|fstat|lstat|"
        r"mkdir|rmdir|ftruncate|lseek|mmap|munmap|flock|fcntl|msync|readdir|opendir)\(|\bio::[a-z_]+\(|"
        r"\bexists\(|\bis_directory\(|\bis_regular_file\(|\bgetline\(|\bpopen\(|\bsystem\(|\bgetaddrinfo\("
    ),
    "pool": r"\block_shard\s+\w+\s*[({]|\bcheckout\(|\bcheck_out\(|\bstorage_commit\(|\bstorage->(read|write)\(|\bwritable_storage->",
}

CLASSES: tuple[str, ...] = ("allow", "owed", "compiled_out")


@dataclass
class Hit:
    file: str
    line: int
    rule: str
    text: str
    cls: str = "unclassified"
    note: str = ""


@dataclass
class Row:
    file: str
    rule: str
    anchor: re.Pattern[str]
    cls: str
    count: int
    note: str
    row_line: int
    matched: int = 0


@dataclass
class Report:
    hits: list[Hit] = field(default_factory=list)
    errors: list[str] = field(default_factory=list)
    skipped: list[str] = field(default_factory=list)
    unscanned: list[str] = field(default_factory=list)
    stale: list[str] = field(default_factory=list)


_STRING = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'')


def strip_code(lines: list[str]) -> list[str]:
    """The lines with block comments, line comments and string literals blanked out."""
    out: list[str] = []
    in_block = False
    for raw in lines:
        s = raw
        res = ""
        i = 0
        while i < len(s):
            if in_block:
                end = s.find("*/", i)
                if end < 0:
                    i = len(s)
                else:
                    in_block = False
                    i = end + 2
                continue
            if s.startswith("/*", i):
                in_block = True
                i += 2
                continue
            if s.startswith("//", i):
                break
            res += s[i]
            i += 1
        out.append(_STRING.sub('""', res))
    return out


def load_rows(path: str) -> list[Row]:
    rows: list[Row] = []
    if not os.path.exists(path):
        return rows
    with open(path, encoding="utf-8") as fh:
        for n, line in enumerate(fh, 1):
            line = line.rstrip("\n")
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            cols = line.split("\t")
            if len(cols) != 6:
                raise SystemExit(f"{path}:{n}: want 6 tab separated columns, got {len(cols)}")
            file, rule, anchor, cls, count, note = cols
            if rule not in RULES:
                raise SystemExit(f"{path}:{n}: unknown rule {rule!r}")
            if cls not in CLASSES:
                raise SystemExit(f"{path}:{n}: unknown class {cls!r}")
            if not note.strip():
                raise SystemExit(f"{path}:{n}: a row needs a note (the reason, or the decision that removes it)")
            if cls == "owed" and not re.match(r"^(D\d+|X\d)\b", note):
                raise SystemExit(
                    f"{path}:{n}: an owed row's note starts with its decision or step (D1 to D16, X1 to X7)"
                )
            rows.append(Row(file, rule, re.compile(anchor), cls, int(count), note, n))
    return rows


def tracked_sources(root: str) -> list[str]:
    try:
        out = subprocess.run(
            ["git", "ls-files", "src"],
            cwd=root,
            capture_output=True,
            text=True,
            check=True,
        ).stdout
        names = out.splitlines()
    except (subprocess.CalledProcessError, FileNotFoundError):
        names = []
        for base, _dirs, files in os.walk(os.path.join(root, "src")):
            for f in files:
                names.append(os.path.relpath(os.path.join(base, f), root))
    return sorted(n for n in names if n.endswith(SOURCE_SUFFIXES))


def extern_sources(name: str, directory: str, rows: list[Row]) -> list[str]:
    wanted = {r.file for r in rows if r.file.startswith(name + "/")}
    found: list[str] = []
    for base, _dirs, files in os.walk(directory):
        for f in files:
            rel = name + "/" + os.path.relpath(os.path.join(base, f), directory)
            if rel in wanted:
                found.append(rel)
    return sorted(found)


# A declaration or an include names a primitive without using it.
_NOT_A_USE = re.compile(
    r"^\s*#\s*include\b|^\s*(static |mutable |inline |extern )*std::(mutex|shared_mutex|recursive_mutex|condition_variable|"
    r"condition_variable_any|timed_mutex)\s+[\w:]+(\{\})?\s*;|^\s*(class|struct|friend class)\s+\w+\s*[;{]?\s*$"
)


def scan_file(display: str, path: str) -> list[Hit]:
    try:
        with open(path, encoding="utf-8", errors="replace") as fh:
            raw = fh.read().splitlines()
    except OSError:
        return []
    code = strip_code(raw)
    compiled = {name: re.compile(rx) for name, rx in RULES.items()}
    hits: list[Hit] = []
    for n, (text, original) in enumerate(zip(code, raw), 1):
        if _NOT_A_USE.search(text):
            continue
        for name, rx in compiled.items():
            if rx.search(text):
                hits.append(Hit(display, n, name, original.strip()))
    return hits


def classify(hits: list[Hit], rows: list[Row]) -> None:
    by_file: dict[str, list[Row]] = {}
    for r in rows:
        by_file.setdefault(r.file, []).append(r)
    for h in hits:
        for r in by_file.get(h.file, []):
            if r.rule == h.rule and r.anchor.search(h.text):
                h.cls, h.note = r.cls, r.note
                r.matched += 1
                break


def run(root: str, allowlist: str, externs: dict[str, str]) -> Report:
    rep = Report()
    rows = load_rows(allowlist)
    paths = tracked_sources(root)
    never = [re.compile(p) for p in NEVER_OPEN]
    for p in paths:
        if any(rx.search(p) for rx in never):
            rep.skipped.append(p)
            continue
        rep.hits.extend(scan_file(p, os.path.join(root, p)))
    for name, directory in externs.items():
        if not os.path.isdir(directory):
            rep.errors.append(f"extern {name}: {directory} is not a directory")
            continue
        for rel in extern_sources(name, directory, rows):
            rep.hits.extend(scan_file(rel, os.path.join(directory, rel[len(name) + 1 :])))
    classify(rep.hits, rows)
    # A library file is watched only at the lines its rows name; its other hits are its own.
    rep.hits = [h for h in rep.hits if h.cls != "unclassified" or h.file.startswith("src/")]
    scanned_externs = set(externs)
    for r in rows:
        ext = r.file.split("/", 1)[0] if "/" in r.file else ""
        if r.file.startswith("src/") is False and ext not in scanned_externs:
            rep.unscanned.append(f"{r.file} (row {r.row_line}) needs --extern {ext}=DIR")
            continue
        where = f"{os.path.basename(allowlist)}:{r.row_line}: {r.file} [{r.rule}] /{r.anchor.pattern}/"
        if r.matched > r.count:
            rep.errors.append(
                f"{where} covers {r.matched} hit(s), the row allows {r.count}: a new hit needs its own row"
            )
        elif r.matched < r.count:
            # A removal (X1, X6) shrinks a row; that is progress, so it is reported and never fails.
            rep.stale.append(f"{where} covers {r.matched} of {r.count} hit(s): lower the count")
    return rep


def main(argv: list[str]) -> int:
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=os.path.dirname(here))
    ap.add_argument("--allowlist", default=os.path.join(here, "async_allowlist.tsv"))
    ap.add_argument("--extern", action="append", default=[], metavar="NAME=DIR")
    ap.add_argument("--strict", action="store_true")
    ap.add_argument("--owed", action="store_true", help="with --strict, an owed hit also fails")
    ap.add_argument("--list", action="store_true")
    ap.add_argument("--json", action="store_true")
    args = ap.parse_args(argv)
    externs: dict[str, str] = {}
    for spec in args.extern:
        name, _, directory = spec.partition("=")
        externs[name] = directory
    rep = run(args.root, args.allowlist, externs)

    counts = {c: 0 for c in (*CLASSES, "unclassified")}
    for h in rep.hits:
        counts[h.cls] += 1
    by_rule: dict[str, int] = {}
    for h in rep.hits:
        by_rule[h.rule] = by_rule.get(h.rule, 0) + 1
    decisions: dict[str, int] = {}
    for h in rep.hits:
        if h.cls == "owed":
            d = h.note.split()[0].rstrip(":,.")
            decisions[d] = decisions.get(d, 0) + 1

    if args.json:
        json.dump(
            {
                "hits": [h.__dict__ for h in rep.hits],
                "counts": counts,
                "by_rule": by_rule,
                "owed_by_decision": decisions,
                "never_opened": len(rep.skipped),
                "errors": rep.errors,
                "unscanned": rep.unscanned,
                "shrunk": rep.stale,
            },
            sys.stdout,
            indent=1,
        )
        print()
    else:
        if args.list or counts["unclassified"]:
            for h in rep.hits:
                if args.list or h.cls == "unclassified":
                    print(
                        f"{h.file}:{h.line}: [{h.rule}] {h.cls}: {h.text[:110]}" + (f"  -- {h.note}" if h.note else "")
                    )
        for e in rep.errors:
            print(f"error: {e}")
        for u in rep.unscanned:
            print(f"unscanned: {u}")
        for w in rep.stale:
            print(f"shrunk: {w}")
        print(
            f"async_check: {len(rep.hits)} hits: "
            + ", ".join(f"{c} {counts[c]}" for c in (*CLASSES, "unclassified"))
            + f"; {len(rep.skipped)} files never opened (X1's or off limits)"
        )
        print("async_check: by rule: " + ", ".join(f"{k} {v}" for k, v in sorted(by_rule.items())))
        if decisions:
            print("async_check: owed by decision: " + ", ".join(f"{k} {v}" for k, v in sorted(decisions.items())))

    failed = counts["unclassified"] > 0 or bool(rep.errors)
    if args.owed and counts["owed"]:
        failed = failed or args.strict
    if args.strict and failed:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
