#!/usr/bin/env python3
"""Prints a restore_stall_probe --out JSON as one line per phase."""

import json
import sys

for r in json.load(open(sys.argv[1])):
    t = r.get("trivial")
    lag = r["lag_us"]["http0"]
    if t:
        print(
            f"{r['phase']:9s} p50 {t['p50_ms']:6.2f} p99 {t['p99_ms']:6.2f} p99.9 {t['p999_ms']:6.2f} max {t['max_ms']:6.2f} | lag p50 {lag['p50_us']} p99 {lag['p99_us']} p99.9 {lag['p999_us']} max_window {lag['max_us_window']} | restore/s {r.get('restore_docs_per_s')} cpu_s {r['node_cpu_s']}"
        )
    else:
        print(
            f"{r['phase']:9s} lag http0 p50 {lag['p50_us']} p99 {lag['p99_us']} p99.9 {lag['p999_us']} max_window {lag['max_us_window']} wakes {lag['wakes']}"
        )
