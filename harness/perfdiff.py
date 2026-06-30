#!/usr/bin/env python3
"""
perfdiff.py -- compare two loadtest.py results and report improving/regressing.

  python3 perfdiff.py baseline.json ours.json

Per metric it prints baseline, ours, delta%, and a verdict. Direction-aware:
higher docs/sec and QPS are better; lower latency and on-disk bytes are better.
A change is only flagged when it clears a noise band (default 5%, widened by the
observed trial spread) so run-to-run jitter doesn't read as a regression.
"""
import json
import sys

THRESHOLD = 0.05  # 5% minimum to flag


def pct(base, cur):
    if base in (None, 0) or cur is None:
        return None
    return (cur - base) / base


def spread(trials, key):
    vals = [t.get(key) for t in trials if t.get(key) is not None]
    if len(vals) < 2:
        return 0.0
    return (max(vals) - min(vals)) / (sum(vals) / len(vals))


def verdict(delta, higher_better, band):
    if delta is None:
        return "n/a"
    if abs(delta) < band:
        return "~same"
    improved = (delta > 0) == higher_better
    return "IMPROVED" if improved else "REGRESSED"


def row(name, base, cur, higher_better, band):
    d = pct(base, cur)
    v = verdict(d, higher_better, band)
    ds = f"{d*100:+.1f}%" if d is not None else "n/a"
    print(f"  {name:22} {str(base):>14} -> {str(cur):>14}  {ds:>8}  {v}")
    return v


def main():
    base = json.load(open(sys.argv[1]))
    ours = json.load(open(sys.argv[2]))
    bl = base.get("label") or sys.argv[1]
    ol = ours.get("label") or sys.argv[2]
    print(f"baseline = {bl}\nours     = {ol}\n")

    bi, oi = base["index_phase"], ours["index_phase"]
    bq, oq = base["query_phase"], ours["query_phase"]
    # noise band for QPS from the trial spread on each side
    qps_band = max(THRESHOLD, spread(base.get("query_trials", []), "qps"),
                   spread(ours.get("query_trials", []), "qps"))

    idx_band = max(THRESHOLD, spread(base.get("index_trials", []), "docs_per_sec"),
                   spread(ours.get("index_trials", []), "docs_per_sec"))

    verdicts = []
    print("INDEX")
    verdicts.append(row(f"docs/sec (band {idx_band*100:.0f}%)", bi["docs_per_sec"], oi["docs_per_sec"], True, idx_band))
    if bi.get("disk_bytes") and oi.get("disk_bytes"):
        verdicts.append(row("on-disk bytes", bi["disk_bytes"], oi["disk_bytes"], False, THRESHOLD))

    print("\nQUERY")
    verdicts.append(row(f"qps (band {qps_band*100:.0f}%)", bq["qps"], oq["qps"], True, qps_band))
    for p in ("p50", "p95", "p99"):
        verdicts.append(row(f"latency {p} (ms)", bq["latency_ms"][p], oq["latency_ms"][p], False, THRESHOLD))

    print("\n=== VERDICT ===")
    if "REGRESSED" in verdicts:
        print("REGRESSION: at least one metric regressed beyond the noise band.")
        sys.exit(1)
    elif "IMPROVED" in verdicts:
        print("IMPROVED (or neutral): no regressions; some metrics improved.")
    else:
        print("WITHIN NOISE: no metric moved beyond the band.")


if __name__ == "__main__":
    main()
