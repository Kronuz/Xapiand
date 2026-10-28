"""Value-slot and accuracy-term byte counts: Xapiand's current codec against the proposed one (T5).

Usage: python3 -I sizes.py LEGACY_PROBE FILE.ndjson...
LEGACY_PROBE is legacy_probe.cc built against src/sortable_serialise.cc (a black box: called, never read).
"""

from __future__ import annotations

import json
import random
import subprocess
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import model

ACCURACIES = (
    100,
    1000,
    10000,
    100000,
    1000000,
    100000000,
)  # Xapiand's numeric defaults


def legacy(probe: str, items: list[tuple[str, str]]) -> list[int]:
    out = subprocess.run(
        [probe],
        input="".join(f"{k} {t}\n" for k, t in items),
        capture_output=True,
        text=True,
        check=True,
    ).stdout
    return [
        len(line.split()[2]) // 2 if len(line.split()) == 3 else 0
        for line in out.splitlines()
    ]


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


def item(v: float) -> tuple[str, str]:
    if isinstance(v, float):
        return ("d", repr(v))
    return ("u" if v >= 1 << 63 else "i", str(v))


def term_values(v: float) -> list[int]:
    iv = int(v)
    return [iv - iv % a for a in ACCURACIES]  # floor buckets


def main() -> None:
    probe, files = sys.argv[1], sys.argv[2:]
    rows: dict[str, list[int | float]] = defaultdict(list)
    for f in files:
        name = Path(f).stem
        for line in Path(f).read_text().splitlines():
            vals: list[tuple[str, int | float]] = []
            walk(json.loads(line), "", vals)
            for path, v in vals:
                rows[f"{name}.{path}"].append(v)
    rnd = random.Random(7)
    lo, hi = 1577836800, 1767225600  # 2020-01-01 .. 2026-01-01
    rows["synthetic.datetime_us"] = [
        round(rnd.uniform(lo, hi) * 1e6) / 1e6 for _ in range(1000)
    ]
    rows["synthetic.datetime_s"] = [float(rnd.randrange(lo, hi)) for _ in range(1000)]
    rows["estimate.version_1..1000"] = list(range(1, 1001))
    print(
        "| Field | Values | Current mean B | Proposed mean B | Proposed max B | Fixed 9 B | Term B current | Term B proposed |"
    )
    print("|---|---|---|---|---|---|---|---|")
    tot = defaultdict(int)
    for field, vals in rows.items():
        items = [item(v) for v in vals]
        cur = legacy(probe, items)
        new = [len(model.encode(v)) for v in vals]
        terms = (
            [t for v in vals for t in term_values(v)]
            if not field.startswith("estimate")
            else []
        )
        tcur = legacy(probe, [("i", str(t)) for t in terms]) if terms else []
        tnew = [len(model.encode(t)) for t in terms]
        n = len(vals)
        tot["n"] += n
        tot["cur"] += sum(cur)
        tot["new"] += sum(new)
        tot["fix"] += 9 * n
        tot["tcur"] += sum(tcur)
        tot["tnew"] += sum(tnew)
        tot["tn"] += len(terms)
        tm = lambda xs: f"{sum(xs) / len(xs):.2f}" if xs else "-"
        print(
            f"| {field} | {n} | {sum(cur) / n:.2f} | {sum(new) / n:.2f} | {max(new)} | 9 | {tm(tcur)} | {tm(tnew)} |"
        )
    print(
        f"| all | {tot['n']} | {tot['cur'] / tot['n']:.2f} | {tot['new'] / tot['n']:.2f} | | 9 | "
        f"{tot['tcur'] / tot['tn']:.2f} | {tot['tnew'] / tot['tn']:.2f} |"
    )
    print(
        f"totals: values current {tot['cur']} B, proposed {tot['new']} B, fixed-9 {tot['fix']} B; "
        f"terms current {tot['tcur']} B, proposed {tot['tnew']} B over {tot['tn']} terms"
    )


if __name__ == "__main__":
    main()
