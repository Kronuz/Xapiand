"""Value-slot and accuracy-term byte counts: Xapiand's current codec against the proposed one (T5).

Usage: python3 -I sizes.py LEGACY_PROBE FILE.ndjson...
LEGACY_PROBE is legacy_probe.cc built against src/sortable_serialise.cc (a black box: called, never read).
Sets: the NDJSON corpora's numeric fields, 13 synthetic distributions of 2,000 values (seed 27) and a
version estimate. Accuracy terms follow Xapiand's defaults: numeric floors 100..10^8 (schema.cc:150-157,
generate_terms.cc:226-239; floats truncate to int64 first), datetime calendar buckets hour, day, month, year,
decade, century (def_accuracy_datetime, schema.cc:169-176; generate_terms.cc:253-293). T5 passes when no set is
larger than the old codec in values or in terms; the exit status says so.
"""

from __future__ import annotations

import calendar
import datetime as dt
import json
import random
import subprocess
import sys
from collections import defaultdict
from functools import cache
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import model

NUM_ACC = (100, 1000, 10000, 100000, 1000000, 100000000)
DT_ACC = ("hour", "day", "month", "year", "decade", "century")
PROBE = ""


def item(v: int | float) -> tuple[str, str]:
    if isinstance(v, float):
        return ("d", repr(v))
    return ("u" if v >= 1 << 63 else "i", str(v))


def legacy(items: list[tuple[str, str]]) -> list[int]:
    out = subprocess.run(
        [PROBE],
        input="".join(f"{k} {t}\n" for k, t in items),
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return [len(p[2]) // 2 if len(p := line.split()) == 3 else 0 for line in out.splitlines()]


@cache
def new_len(v: int | float) -> int:
    return len(model.encode(v))


def walk(obj: object, path: str, out: list[tuple[str, int | float]]) -> None:
    if isinstance(obj, dict):
        if "_point" in obj or path.endswith("._point"):
            return  # geo values do not use the codec
        for k, v in obj.items():
            walk(v, f"{path}.{k}" if path else k, out)
    elif isinstance(obj, list):
        for v in obj:
            walk(v, path, out)
    elif isinstance(obj, bool):
        return
    elif isinstance(obj, (int, float)):
        out.append((path, obj))


def num_terms(v: int | float) -> list[int]:
    iv = int(v)
    return [iv - iv % a for a in NUM_ACC]  # floor buckets


def dt_terms(ts: float) -> list[float]:
    t = dt.datetime.fromtimestamp(int(ts // 1), dt.UTC)
    g = lambda *a: float(calendar.timegm(dt.datetime(*a, tzinfo=dt.UTC).timetuple()))  # noqa: E731
    yr = lambda n: max(t.year - t.year % n, n)  # noqa: E731  GenerateTerms::year (generate_terms.h:116)
    return [g(t.year, t.month, t.day, t.hour), g(t.year, t.month, t.day), g(t.year, t.month, 1), g(t.year, 1, 1), g(yr(10), 1, 1), g(yr(100), 1, 1)]


def synthetic() -> dict[str, tuple[list[int | float], str]]:
    rnd = random.Random(27)
    lo, hi = 1577836800, 1767225600  # 2020-01-01 .. 2026-01-01
    n = 2000
    return {
        "small ints 0..100": ([rnd.randrange(0, 101) for _ in range(n)], "num"),
        "signed ints -1000..1000": ([rnd.randrange(-1000, 1001) for _ in range(n)], "num"),
        "negative ints -1e6..0": ([-rnd.randrange(0, 1000001) for _ in range(n)], "num"),
        "prices 2 decimals 0.01..1000": ([round(rnd.uniform(0.01, 1000), 2) for _ in range(n)], "num"),
        "negative prices -1000..-0.01": ([-round(rnd.uniform(0.01, 1000), 2) for _ in range(n)], "num"),
        "scores uniform [0,1)": ([rnd.random() for _ in range(n)], "num"),
        "epoch ms as integer": ([rnd.randrange(lo * 1000, hi * 1000) for _ in range(n)], "num"),
        "snowflake ids ~2^60..2^62": ([rnd.randrange(1 << 60, 1 << 62) for _ in range(n)], "num"),
        "uint64 hashes": ([rnd.randrange(0, 1 << 64) for _ in range(n)], "num"),
        "datetime ms precision": ([round(rnd.uniform(lo, hi), 3) for _ in range(n)], "dt"),
        "datetime us precision": ([round(rnd.uniform(lo, hi) * 1e6) / 1e6 for _ in range(n)], "dt"),
        "datetime whole seconds": ([float(rnd.randrange(lo, hi)) for _ in range(n)], "dt"),
        "timedelta 0..3600 s, ms": ([round(rnd.uniform(0, 3600), 3) for _ in range(n)], "none"),
    }


def mean(xs: list[int]) -> str:
    return f"{sum(xs) / len(xs):.2f}" if xs else "-"


def main() -> None:
    global PROBE
    PROBE, files = sys.argv[1], sys.argv[2:]
    sets: dict[str, tuple[list[int | float], str]] = {}
    fields: dict[str, list[int | float]] = defaultdict(list)
    for f in files:
        for line in Path(f).read_text().splitlines():
            vals: list[tuple[str, int | float]] = []
            walk(json.loads(line), "", vals)
            for path, v in vals:
                fields[f"{Path(f).stem}.{path}"].append(v)
    for name, vs in fields.items():
        sets[name] = (vs, "num")
    sets.update(synthetic())
    sets["estimate: versions 1..1000"] = (list(range(1, 1001)), "none")

    print("| Set | Values | Old mean B | New mean B | New max B | Terms | Old term B | New term B |")
    print("|---|---|---|---|---|---|---|---|")
    tot: dict[str, dict[str, int]] = {"corpora": defaultdict(int), "synthetic": defaultdict(int)}
    larger: list[str] = []
    bucket: dict[str, tuple[list[int], list[int]]] = {a: ([], []) for a in DT_ACC}
    for name, (vals, kind) in sets.items():
        old = legacy([item(v) for v in vals])
        new = [new_len(v) for v in vals]
        terms: list[int | float] = []
        if kind == "num":
            terms = [t for v in vals for t in num_terms(v)]
        elif kind == "dt":
            terms = [t for v in vals for t in dt_terms(v)]
        told = legacy([item(t) for t in terms]) if terms else []
        tnew = [new_len(t) for t in terms]
        if name == "datetime us precision":
            for j, a in enumerate(DT_ACC):
                bucket[a][0].extend(told[j::6])
                bucket[a][1].extend(tnew[j::6])
        group = "corpora" if name in fields else "synthetic"
        for key, xs in (("o", old), ("n", new), ("to", told), ("tn", tnew)):
            tot[group][key] += sum(xs)
        tot[group]["v"] += len(vals)
        tot[group]["t"] += len(terms)
        if sum(new) > sum(old) or sum(tnew) > sum(told):
            larger.append(name)
        print(f"| {name} | {len(vals)} | {mean(old)} | {mean(new)} | {max(new)} | {len(terms)} | {mean(told)} | {mean(tnew)} |")
    for group, t in tot.items():
        print(
            f"\n{group}: values old {t['o']} B, new {t['n']} B ({(t['n'] - t['o']) / t['o']:+.1%}) over {t['v']}; "
            f"terms old {t['to']} B, new {t['tn']} B ({(t['tn'] - t['to']) / t['to']:+.1%}) over {t['t']}"
        )
    print("\n| Datetime accuracy (microsecond set) | Old term B | New term B |\n|---|---|---|")
    for a, (o, n) in bucket.items():
        print(f"| {a} | {mean(o)} | {mean(n)} |")
    print(f"\nT5: {'PASS' if not larger else 'FAIL, larger: ' + ', '.join(larger)}")
    sys.exit(1 if larger else 0)


if __name__ == "__main__":
    main()
