"""Encoded sizes of real numbers under the variable codec (Kronuz/sortable) against fixed 8 and 10 bytes.

Byte counts only (no timing). For each set: the mean bytes of a value, of a distinct value (what a
per-segment dictionary of a `bytes` column stores), and of a distinct term (the exact-value term plus
Xapiand's accuracy terms: def_accuracy_num for numbers, def_accuracy_datetime for datetimes).

Usage: python3 -I sizes.py SORTABLE_MODEL ASSETS_DIR FILES_ROOT GIT_ROOT [MAX_FILES]
  SORTABLE_MODEL  path to Kronuz/sortable tools/model.py (the format's reference encoder)
  ASSETS_DIR      Xapiand docs/public/assets (accounts.ndjson, cities.ndjson)
  FILES_ROOT      a directory tree whose file sizes and modification times are sampled
  GIT_ROOT        a directory whose immediate subdirectories are git repositories (commit times)
"""

from __future__ import annotations

import calendar
import importlib.util
import json
import os
import subprocess
import sys
import time
from fractions import Fraction

ACC_NUM = [100, 1000, 10000, 100000, 1000000, 100000000]  # schema.cc:151-158


def load_model(path: str):
    spec = importlib.util.spec_from_file_location("sortable_model", path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def num_terms(v: int) -> list[tuple[int, int]]:
    """(level, bucket start) per accuracy; the bucket floors (negatives too)."""
    return [(a, v - (v % a)) for a in ACC_NUM]


def dt_terms(t: float) -> list[tuple[str, float]]:
    """Calendar truncation to hour, day, month, year, decade, century (def_accuracy_datetime)."""
    tm = time.gmtime(int(t // 1))
    y = tm.tm_year
    return [
        ("hour", float(calendar.timegm((y, tm.tm_mon, tm.tm_mday, tm.tm_hour, 0, 0)))),
        ("day", float(calendar.timegm((y, tm.tm_mon, tm.tm_mday, 0, 0, 0)))),
        ("month", float(calendar.timegm((y, tm.tm_mon, 1, 0, 0, 0)))),
        ("year", float(calendar.timegm((y, 1, 1, 0, 0, 0)))),
        ("decade", float(calendar.timegm((y - y % 10, 1, 1, 0, 0, 0)))),
        ("century", float(calendar.timegm((y - y % 100, 1, 1, 0, 0, 0)))),
    ]


def walk_json(o, path, out):
    if isinstance(o, dict):
        for k, v in o.items():
            if k in ("_point", "_longitude", "_latitude"):
                continue  # geo has its own encoding
            walk_json(v, f"{path}.{k}" if path else k, out)
    elif isinstance(o, bool):
        return
    elif isinstance(o, (int, float)):
        out.setdefault(path, []).append(o)


def ndjson_sets(assets: str) -> dict[str, tuple[str, list]]:
    sets: dict[str, tuple[str, list]] = {}
    for name in ("accounts", "cities"):
        fields: dict[str, list] = {}
        with open(os.path.join(assets, f"{name}.ndjson")) as f:
            for line in f:
                line = line.strip()
                if line:
                    walk_json(json.loads(line), "", fields)
        for k, vs in sorted(fields.items()):
            kind = "float" if any(isinstance(v, float) for v in vs) else "int"
            sets[f"{name}.{k}"] = (kind, [float(v) if kind == "float" else v for v in vs])
    coords: list = []
    with open(os.path.join(assets, "cities.ndjson")) as f:
        for line in f:
            if line.strip():
                p = json.loads(line)["location"]["_point"]
                coords += [p["_longitude"], p["_latitude"]]
    sets["cities coordinates as floating"] = ("float", coords)
    return sets


def file_sets(root: str, max_files: int) -> dict[str, tuple[str, list]]:
    sizes, mtimes = [], []
    for d, dirs, files in os.walk(root):
        dirs.sort()
        for fn in sorted(files):
            try:
                st = os.lstat(os.path.join(d, fn))
            except OSError:
                continue
            sizes.append(st.st_size)
            mtimes.append(round(st.st_mtime_ns / 1000) / 1e6)  # Xapiand rounds datetimes to microseconds
            if len(sizes) >= max_files:
                return {"file sizes (bytes)": ("int", sizes), "file mtimes (datetime, us)": ("datetime", mtimes)}
    return {"file sizes (bytes)": ("int", sizes), "file mtimes (datetime, us)": ("datetime", mtimes)}


def git_sets(root: str) -> dict[str, tuple[str, list]]:
    times = []
    for name in sorted(os.listdir(root)):
        p = os.path.join(root, name)
        if not os.path.isdir(os.path.join(p, ".git")) or name.endswith("-wt"):
            continue
        out = subprocess.run(
            ["git", "-C", p, "log", "--all", "--format=%at"], capture_output=True, text=True, check=False
        ).stdout
        times += [float(x) for x in out.split()]
    return {"git commit times (datetime, s)": ("datetime", times)}


def main() -> None:
    model_path, assets, files_root, git_root = sys.argv[1:5]
    max_files = int(sys.argv[5]) if len(sys.argv) > 5 else 200000
    m = load_model(model_path)

    def enc(v) -> bytes:
        return m.encode(v if isinstance(v, float) else Fraction(v))

    sets = {**ndjson_sets(assets), **file_sets(files_root, max_files), **git_sets(git_root)}
    print("| Set | Kind | Values | Distinct | Value B | Distinct value B | Distinct terms | Term B | Max B |")
    print("|---|---|---|---|---|---|---|---|---|")
    tot = {"v": 0, "vb": 0, "d": 0, "db": 0, "t": 0, "tb": 0}
    for name, (kind, vs) in sets.items():
        lens = [len(enc(v)) for v in vs]
        distinct = set(vs)
        dl = sum(len(enc(v)) for v in distinct)
        terms: set = set()
        for v in distinct:
            terms.add(("exact", v))
            if kind == "datetime":
                terms.update(dt_terms(v))
            else:
                terms.update(num_terms(int(v)))  # floats truncate to int64 first (schema.cc:5365)
        tl = sum(len(enc(t[1])) for t in terms)
        print(
            f"| {name} | {kind} | {len(vs)} | {len(distinct)} | {sum(lens) / len(lens):.2f} | "
            f"{dl / len(distinct):.2f} | {len(terms)} | {tl / len(terms):.2f} | {max(lens)} |"
        )
        tot["v"] += len(vs)
        tot["vb"] += sum(lens)
        tot["d"] += len(distinct)
        tot["db"] += dl
        tot["t"] += len(terms)
        tot["tb"] += tl
    print()
    for label, n, b in (
        ("values", tot["v"], tot["vb"]),
        ("distinct values", tot["d"], tot["db"]),
        ("distinct terms", tot["t"], tot["tb"]),
    ):
        print(
            f"{label}: {n} items, variable {b} B ({b / n:.2f} B each); fixed 8: {8 * n} B "
            f"({8 * n / b:.2f}x the variable bytes); fixed 10: {10 * n} B ({10 * n / b:.2f}x)"
        )


if __name__ == "__main__":
    main()
