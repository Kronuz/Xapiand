#!/usr/bin/env python3
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
"""light_share_turns.py DIR: per-turn wall and thread-CPU time of drain() resumes in the loaded phase (T4's settle).

Reads DIR/turns-N-ARM.jsonl (LS_TURN_HIST snapshots, cumulative per loop and lane) and DIR/ls-N-ARM.json (the
loaded phase's start and length), takes the loaded phase's delta per loop, keeps the loops with the most turns (the
HTTP loops), and prints per arm and lane the turns, wall and CPU percentiles, the turns over 1.024 ms (T4's bound)
and how much of their wall time was CPU. Wall near CPU: turn length. Wall well over CPU: the thread was off its CPU.
"""

from __future__ import annotations

import glob
import json
import os
import re
import sys

LOOPS = 16
BOUND_NS = 1_024_000


def edge(b: int) -> float:
    """Lower edge in ns of log-scale bucket b (4 per octave)."""
    if b < 4:
        return float(b)
    lg, q = divmod(b, 4)
    return float((4 + q) << (lg - 2))


def pct(buckets: dict[int, int], p: float) -> float:
    n = sum(buckets.values())
    if not n:
        return 0.0
    acc, want = 0, n * p
    for b in sorted(buckets):
        acc += buckets[b]
        if acc >= want:
            return edge(b) * 1.0905  # the bucket's geometric middle
    return edge(max(buckets))


def load(path: str) -> dict[tuple[int, int], list[tuple[int, dict[int, list[int]]]]]:
    snaps: dict[tuple[int, int], list[tuple[int, dict[int, list[int]]]]] = {}
    for line in open(path):
        d = json.loads(line)
        snaps.setdefault((d["s"], d["heavy"]), []).append((d["t"], {b[0]: b[1:] for b in d["w"]}))
    return snaps


def at(series: list[tuple[int, dict[int, list[int]]]], t_ms: float) -> dict[int, list[int]]:
    last: dict[int, list[int]] = {}
    for t, h in series:
        if t <= t_ms:
            last = h
    return last


def delta(a: dict[int, list[int]], b: dict[int, list[int]]) -> dict[int, list[int]]:
    out = {}
    for k, v in b.items():
        w = a.get(k, [0, 0, 0, 0])
        d = [x - y for x, y in zip(v, w)]
        if any(d):
            out[k] = d
    return out


def main() -> int:
    d = sys.argv[1]
    rows = 0
    for tp in sorted(glob.glob(os.path.join(d, "turns-*.jsonl")), key=lambda p: int(re.search(r"turns-(\d+)-", p)[1])):  # type: ignore[index]
        m = re.search(r"turns-(\d+)-(.+)\.jsonl$", tp)
        assert m
        lp = os.path.join(d, f"ls-{m[1]}-{m[2]}.json")
        if not os.path.exists(lp) or not os.path.getsize(tp):
            continue
        ph = json.load(open(lp))["phases"][-1]
        t0 = ph["started"] * 1000
        t1 = t0 + ph["seconds"] * 1000 + 1000
        snaps = load(tp)
        per = {k: delta(at(v, t0), at(v, t1)) for k, v in snaps.items()}
        turns = {s: sum(x[0] for kk, h in per.items() if kk[0] == s for x in h.values()) for s in {k[0] for k in per}}
        top = sorted(turns, key=lambda s: -turns[s])[:LOOPS]
        print(f"arm {m[1]}-{m[2]}: loaded phase, {len(top)} busiest loops of {len(turns)} (turns {sorted(turns.values())[::-1][:LOOPS]})")
        for heavy in (0, 1):
            w: dict[int, int] = {}
            c: dict[int, int] = {}
            wall_sum = cpu_sum = over_n = over_wall = over_cpu = 0
            worst = 0.0
            for s in top:
                h = per.get((s, heavy), {})
                lw: dict[int, int] = {}
                for b, (n, ws, cw, cn) in h.items():
                    w[b] = w.get(b, 0) + n
                    lw[b] = n
                    c[b] = c.get(b, 0) + cn
                    wall_sum += ws
                    cpu_sum += cw
                    if edge(b) >= BOUND_NS:
                        over_n += n
                        over_wall += ws
                        over_cpu += cw
                worst = max(worst, pct(lw, 0.99))
            n = sum(w.values())
            if not n:
                continue
            rows += 1
            print(
                f"  {'heavy' if heavy else 'light'}: {n} turns; wall p50 {pct(w, .5) / 1000:.0f} p99 {pct(w, .99) / 1000:.0f}"
                f" p99.9 {pct(w, .999) / 1000:.0f} us (worst loop p99 {worst / 1000:.0f}); cpu p50 {pct(c, .5) / 1000:.0f}"
                f" p99 {pct(c, .99) / 1000:.0f} p99.9 {pct(c, .999) / 1000:.0f} us; cpu/wall {cpu_sum / max(wall_sum, 1):.2f};"
                f" turns over 1.024 ms wall: {over_n} ({100 * over_n / n:.2f}%), their cpu/wall {over_cpu / max(over_wall, 1):.2f}"
            )
    return 0 if rows else 1


if __name__ == "__main__":
    sys.exit(main())
