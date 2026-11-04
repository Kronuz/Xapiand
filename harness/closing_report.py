#!/usr/bin/env python3
"""closing_report.py -- the closing benchmark's anchors and verdicts (docs/design/closing-benchmark.md, D10 and D11).

Results live at ROOT/UNIT/ARM/rN.rows.json, one file per arm and round: {"metric": {"v": 1.5, "higher": false,
"axis": "query"}, ...} (extract.py writes them from a tool's JSON). An arm is S, Sp (S's binary again, the A/A control),
X, E, Q, or a shape such as E-shared in the tenant rows.

  closing_report.py anchors ROOT       fix each metric's comparator from rounds 1 and 2 and write ROOT/anchors.json
  closing_report.py arms ROOT UNIT N   the arms round N of UNIT runs (all of them in rounds 1 and 2)
  closing_report.py report ROOT        D11's verdict per metric against every finalist, and against X, as CSV

The verdict comes from cpp-libraries' tools/ab_compare.py (AB_COMPARE, or ~/data/tools/ab_compare.py, which setup.sh
installs): `compare --a S --b FINALIST --aa Sp --output csv` over the five rounds. A verdict of "better" or "worse"
needs all five pairs: ab_compare drops a pair whose value is not positive, so the row's `pairs` column is checked
(C6, CB-R17).
"""

from __future__ import annotations

import argparse
import csv
import io
import json
import os
import re
import statistics
import subprocess
import sys
import tempfile
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

ROUNDS = 5  # a claim needs all of them
CALIBRATION = (1, 2)
NEAR = 0.10  # an arm within 10% of the anchor in calibration stays in the later rounds
BETTER = 0.97  # the median ratio of a "better" row, and 1 / BETTER for a "worse" one
MARGIN = {"query": 0.7, "index": 1 / 1.3, "memory": 0.7, "disk": 0.8}  # PLAN's ratios, S's side of 1
CONTROL = "Sp"
SUBJECT = "S"
REFERENCE = "X"  # runs every round; no row may be worse than it
ARM_FILE = re.compile(r"r(\d+)\.rows\.json")


@dataclass(frozen=True)
class Metric:
    value: float
    higher: bool
    axis: str


Rows = dict[str, Metric]


def read_rows(path: Path) -> Rows:
    raw = json.loads(path.read_text())
    return {k: Metric(float(v["v"]), bool(v.get("higher", False)), str(v.get("axis", "other"))) for k, v in raw.items()}


def write_rows(path: Path, rows: Rows) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({k: {"v": m.value, "higher": m.higher, "axis": m.axis} for k, m in rows.items()}, indent=1)
    )


def collect(root: Path, unit: str) -> dict[str, dict[int, Rows]]:
    """arm -> round -> rows, for the files under ROOT/UNIT."""
    out: dict[str, dict[int, Rows]] = {}
    base = root / unit
    for arm_dir in sorted(p for p in base.iterdir() if p.is_dir()) if base.is_dir() else []:
        for f in sorted(arm_dir.iterdir()):
            if m := ARM_FILE.fullmatch(f.name):
                out.setdefault(arm_dir.name, {})[int(m.group(1))] = read_rows(f)
    return out


def calibration_value(rounds: dict[int, Rows], metric: str) -> float | None:
    vals = [rows[metric].value for n, rows in rounds.items() if n in CALIBRATION and metric in rows]
    return statistics.median(vals) if len(vals) == len(CALIBRATION) else None


@dataclass
class Comparator:
    anchor: str
    value: float
    higher: bool
    near: list[str] = field(default_factory=list)  # arms within NEAR of the anchor, the anchor excluded


def comparators(data: dict[str, dict[int, Rows]]) -> dict[str, Comparator]:
    """Per metric, the best calibration median among the arms other than S and S', and the arms within 10% of it."""
    metrics: dict[str, Metric] = {}
    for rounds in data.values():
        for rows in rounds.values():
            metrics.update(rows)
    out: dict[str, Comparator] = {}
    for name, meta in sorted(metrics.items()):
        cal = {
            arm: v
            for arm, rounds in data.items()
            if arm not in (SUBJECT, CONTROL) and (v := calibration_value(rounds, name)) is not None
        }
        if not cal:
            continue
        pick = max if meta.higher else min
        anchor = pick(cal, key=lambda a: cal[a])
        best = cal[anchor]
        near = [
            a
            for a, v in cal.items()
            if a != anchor and ((v >= best / (1 + NEAR)) if meta.higher else (v <= best * (1 + NEAR)))
        ]
        out[name] = Comparator(anchor, best, meta.higher, sorted(near))
    return out


def arms_for_round(data: dict[str, dict[int, Rows]], n: int, all_arms: list[str]) -> list[str]:
    """Rounds 1 and 2 run every arm; later rounds S, S', X, the anchors and the arms near them (D10)."""
    if n in CALIBRATION or n < min(CALIBRATION):
        return all_arms
    keep = {SUBJECT, CONTROL, REFERENCE}
    for c in comparators(data).values():
        keep.add(c.anchor)
        keep.update(c.near)
    return [a for a in all_arms if a in keep]


# -- ab_compare ------------------------------------------------------------------------------------


def ab_compare_path() -> str:
    for cand in (os.environ.get("AB_COMPARE"), os.path.expanduser("~/data/tools/ab_compare.py")):
        if cand and os.path.exists(cand):
            return cand
    raise SystemExit("ab_compare.py not found: set AB_COMPARE (setup.sh installs it at ~/data/tools/ab_compare.py)")


@dataclass(frozen=True)
class Pair:
    ratio: float
    low: float
    high: float
    aa_dev: float | None
    pairs: int
    verdict: str


def write_series(directory: Path, runs: dict[int, Rows], metrics: list[str]) -> list[str]:
    """One text file per round 1..ROUNDS (ab_compare pairs by position): `metric value` rows. A round without the
    arm is a file with a placeholder row, so the later rounds keep their place."""
    directory.mkdir(parents=True, exist_ok=True)
    files = []
    for n in range(1, ROUNDS + 1):
        rows = runs.get(n, {})
        lines = [f"{m} {rows[m].value!r}" for m in metrics if m in rows] or ["_placeholder 1"]
        path = directory / f"r{n}.txt"
        path.write_text("\n".join(lines) + "\n")
        files.append(str(path))
    return files


def compare(ab: str, data: dict[str, dict[int, Rows]], other: str, metrics: list[str], higher: bool) -> dict[str, Pair]:
    """ab_compare S against `other` (and S' as the A/A), on the metrics that are all times or all throughputs."""
    if not metrics:
        return {}
    with tempfile.TemporaryDirectory(prefix="abc", dir=os.environ.get("CLOSING_SCRATCH")) as tmp:
        root = Path(tmp)
        a = write_series(root / "a", data.get(SUBJECT, {}), metrics)
        b = write_series(root / "b", data.get(other, {}), metrics)
        aa = write_series(root / "aa", data.get(CONTROL, {}), metrics)
        cmd = [
            sys.executable,
            "-I",
            ab,
            "compare",
            "--format",
            "table",
            "--a",
            *a,
            "--b",
            *b,
            "--aa",
            *aa,
            "--output",
            "csv",
        ]
        if higher:
            cmd.append("--higher-better")
        res = subprocess.run(cmd, capture_output=True, text=True, check=False)
    if res.returncode not in (0, 1):
        raise SystemExit(f"ab_compare failed ({res.returncode}): {res.stderr.strip()[:500]}")
    out: dict[str, Pair] = {}
    for row in csv.DictReader(io.StringIO(res.stdout)):
        dev = row["aa_dev"]
        out[row["row"]] = Pair(
            float(row["ratio"]),
            float(row["low"]),
            float(row["high"]),
            float(dev) if dev else None,
            int(row["pairs"]),
            row["verdict"],
        )
    return out


# -- D11 -------------------------------------------------------------------------------------------


def judge(p: Pair | None, axis: str) -> tuple[str, str]:
    """(status, why): better, better by the margin, worse, or not distinguishable, from S against one comparator."""
    if p is None:
        return "not distinguishable", "no pairs"
    if p.pairs < ROUNDS:
        return "not distinguishable", f"{p.pairs} of {ROUNDS} pairs"
    if p.aa_dev is None:
        return "not distinguishable", "no A/A control"
    if p.verdict == "slower" and p.high < 1.0 and p.ratio <= BETTER:
        return ("better by the margin" if p.high <= MARGIN.get(axis, 0.0) else "better"), ""
    if p.verdict == "faster" and p.low > 1.0 and p.ratio >= 1.0 / BETTER:
        return "worse", ""
    return "not distinguishable", ""


@dataclass
class Verdict:
    unit: str
    metric: str
    axis: str
    anchor: str
    finalists: list[str]
    status: str
    why: str
    ratio: float | None
    low: float | None
    high: float | None
    aa_dev: float | None
    pairs: int
    against_x: str
    regression: bool
    detail: dict[str, str] = field(default_factory=dict)


def report_unit(root: Path, unit: str, ab: str) -> list[Verdict]:
    data = collect(root, unit)
    comp = comparators(data)
    meta: dict[str, Metric] = {}
    for rounds in data.values():
        for rows in rounds.values():
            meta.update(rows)
    # A finalist is the anchor and the arms near it that ran all rounds; X is compared apart.
    need = {a for c in comp.values() for a in [c.anchor, *c.near]} | ({REFERENCE} & set(data))
    results: dict[tuple[str, bool], dict[str, Pair]] = {}
    for arm in sorted(need):
        for higher in (False, True):
            names = [m for m, c in comp.items() if meta[m].higher == higher]
            results[arm, higher] = compare(ab, data, arm, names, higher)
    out: list[Verdict] = []
    for name, c in comp.items():
        m = meta[name]
        full = [a for a in [c.anchor, *c.near] if len(data.get(a, {})) >= ROUNDS]
        judged = {a: judge(results[a, m.higher].get(name), m.axis) for a in full}
        status, why = "not distinguishable", "" if full else "the anchor ran fewer than five rounds"
        if judged and any(s == "worse" for s, _ in judged.values()):
            status = "worse"
        elif judged and all(s.startswith("better") for s, _ in judged.values()):
            status = (
                "better by the margin" if all(s == "better by the margin" for s, _ in judged.values()) else "better"
            )
        elif judged:
            why = "; ".join(f"{a}: {w}" for a, (s, w) in judged.items() if w)
        pair = results[c.anchor, m.higher].get(name) if (c.anchor, m.higher) in results else None
        x_status = "n/a"
        if REFERENCE in data:
            x_status = judge(results[REFERENCE, m.higher].get(name), m.axis)[0]
        out.append(
            Verdict(
                unit, name, m.axis, c.anchor, full, status, why,
                pair.ratio if pair else None, pair.low if pair else None, pair.high if pair else None,
                pair.aa_dev if pair else None, pair.pairs if pair else 0,
                x_status, x_status == "worse", {a: s for a, (s, _) in judged.items()},
            )
        )  # fmt: skip
    return out


COLUMNS = [
    "unit",
    "metric",
    "axis",
    "anchor",
    "finalists",
    "status",
    "why",
    "ratio",
    "low",
    "high",
    "aa_dev",
    "pairs",
    "against_x",
    "regression",
]


def write_csv(verdicts: list[Verdict], out: Any) -> None:
    w = csv.writer(out)
    w.writerow(COLUMNS)
    for v in verdicts:
        w.writerow(
            [v.unit, v.metric, v.axis, v.anchor, " ".join(v.finalists), v.status, v.why]
            + ["" if x is None else f"{x:.4g}" for x in (v.ratio, v.low, v.high, v.aa_dev)]
            + [v.pairs, v.against_x, "REGRESSION" if v.regression else ""]
        )


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)
    a = sub.add_parser("anchors")
    a.add_argument("root")
    r = sub.add_parser("arms")
    r.add_argument("root")
    r.add_argument("unit")
    r.add_argument("round", type=int)
    r.add_argument("--all", nargs="+", default=["S", "Sp", "X", "E", "Q"], help="every arm of the unit")
    p = sub.add_parser("report")
    p.add_argument("root")
    p.add_argument("--out")
    args = ap.parse_args()
    root = Path(args.root)
    units = sorted(d.name for d in root.iterdir() if d.is_dir())
    if args.cmd == "anchors":
        doc = {
            u: {m: {"anchor": c.anchor, "near": c.near} for m, c in comparators(collect(root, u)).items()}
            for u in units
        }
        (root / "anchors.json").write_text(json.dumps(doc, indent=1))
        print(f"wrote {root / 'anchors.json'}: {sum(len(v) for v in doc.values())} metrics")
        return 0
    if args.cmd == "arms":
        print(" ".join(arms_for_round(collect(root, args.unit), args.round, args.all)))
        return 0
    ab = ab_compare_path()
    verdicts = [v for u in units for v in report_unit(root, u, ab)]
    with open(args.out, "w") if args.out else sys.stdout as fh:
        write_csv(verdicts, fh)
    return 0


if __name__ == "__main__":
    sys.exit(main())
