#!/usr/bin/env python3
"""The restore-stall A/B's verdict, from restore_stall_ab.sh's OUTDIR: one line per arm, then

  A1: head's trivial GET p99 during the RESTORE phase is within 2x of the A/A idle p99 (the median p99 of every
      arm's trivial and trivial3 phases), and its spin run has no loop turn over 5 ms in the RESTORE phase;
  A2: head's RESTORE docs/s is within base's A/A band (base's min minus base's spread) or faster.

Exits 0 when both hold.

  harness/restore_stall_verdict.py OUTDIR
"""

from __future__ import annotations

import glob
import json
import os
import re
import statistics
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from restore_stall_stalls import phase_gaps


def main() -> int:
    out = sys.argv[1]
    arms: dict[str, list[dict]] = {"base": [], "head": []}
    idle_p99: list[float] = []
    for path in sorted(glob.glob(os.path.join(out, "ab*-*.json")), key=lambda p: int(re.findall(r"ab(\d+)", p)[0])):
        arm = re.findall(r"ab\d+-(\w+)\.json", path)[0]
        phases = {r["phase"]: r for r in json.load(open(path))}
        r = phases["restore"]
        row = {
            "arm": os.path.basename(path)[:-5],
            "trivial_p99": phases["trivial"]["trivial"]["p99_ms"],
            "restore_p99": r["trivial"]["p99_ms"],
            "restore_p999": r["trivial"]["p999_ms"],
            "restore_max": r["trivial"]["max_ms"],
            "trivial3_p99": phases["trivial3"]["trivial"]["p99_ms"],
            "docs_s": r["restore_docs_per_s"],
            "lag_p999_us": r["lag_us"]["http0"]["p999_us"],
            "errors": sum(p["errors"] for p in phases.values() if "errors" in p),
        }
        idle_p99 += [row["trivial_p99"], row["trivial3_p99"]]
        arms[arm].append(row)
        print(
            f"{row['arm']:10s} trivial p99 {row['trivial_p99']:6.2f} | RESTORE phase p99 {row['restore_p99']:6.2f} "
            f"p99.9 {row['restore_p999']:6.2f} max {row['restore_max']:6.2f} lag p99.9 {row['lag_p999_us']} us "
            f"| trivial3 p99 {row['trivial3_p99']:6.2f} | {row['docs_s']:.0f} docs/s | errors {row['errors']}"
        )
    turns = {}
    for arm in ("base", "head"):
        log = os.path.join(out, f"spin-{arm}", "node.log")
        gaps = phase_gaps(log, os.path.join(out, f"spin-{arm}.log"), "restore")
        turns[arm] = gaps
        print(f"spin {arm}: {len(gaps)} loop turns over 3 ms in the RESTORE phase, longest {max(gaps, default=0)} us")

    aa = statistics.median(idle_p99)
    head_p99 = [r["restore_p99"] for r in arms["head"]]
    base_p99 = [r["restore_p99"] for r in arms["base"]]
    longest = max(turns["head"], default=0)
    a1 = max(head_p99) <= 2 * aa and longest <= 5000 and all(r["errors"] == 0 for r in arms["head"])
    print(
        f"A1 {'PASS' if a1 else 'FAIL'}: RESTORE-phase p99 head {head_p99} (base {base_p99}) against 2x the A/A "
        f"idle p99 {aa:.2f} = {2 * aa:.2f} ms; head's longest loop turn in the RESTORE phase {longest} us (bound 5,000)"
    )
    base_s = [r["docs_s"] for r in arms["base"]]
    head_s = [r["docs_s"] for r in arms["head"]]
    floor = min(base_s) - (max(base_s) - min(base_s))
    a2 = statistics.mean(head_s) >= floor
    print(
        f"A2 {'PASS' if a2 else 'FAIL'}: RESTORE docs/s head {head_s} (mean {statistics.mean(head_s):.0f}) against "
        f"base {base_s} (mean {statistics.mean(base_s):.0f}, band floor {floor:.0f})"
    )
    return 0 if a1 and a2 else 1


if __name__ == "__main__":
    raise SystemExit(main())
