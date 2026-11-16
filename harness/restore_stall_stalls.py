#!/usr/bin/env python3
"""Reads a node.log written under XAPIAND_LAG_SPIN: the sleep-free probe's SPIN gaps and the STALL stacks sampled
while the loop was held, symbolizes the stacks with atos and prints the gap histogram and each stack's frames.

  harness/restore_stall_stalls.py BIN node.log [--top N] [--frames N]
"""

from __future__ import annotations

import argparse
import collections
import os
import re
import subprocess
import time


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("bin")
    ap.add_argument("log")
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--frames", type=int, default=14)
    ap.add_argument("--min-age-us", type=int, default=0)
    ap.add_argument("--probe-log", help="the probe's stdout: its scrape stamps and phase lines split the log by phase")
    ap.add_argument("--phase", help="keep only captures and gaps inside this phase (needs --probe-log)")
    ap.add_argument(
        "--probe-clock",
        default="raw",
        choices=("raw", "monotonic", "uptime"),
        help="the clock of the probe log's stamps",
    )
    args = ap.parse_args()
    # The node's steady_clock is CLOCK_MONOTONIC_RAW; an older probe log may carry another clock.
    probe_clock = {
        "uptime": time.monotonic_ns,
        "monotonic": lambda: time.clock_gettime_ns(time.CLOCK_MONOTONIC),
        "raw": None,
    }[args.probe_clock]
    shift = 0 if probe_clock is None else time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW) - probe_clock()
    windows: dict[str, tuple[int, int]] = {}
    scrapes: list[tuple[int, int]] = []
    if args.probe_log:
        names: list[str] = []
        for line in open(args.probe_log, errors="replace"):
            if line.startswith('{"scrape"'):
                a, b = re.findall(r"\d+", line)
                scrapes.append((int(a) + shift, int(b) + shift))
            elif line.startswith('{"quiet"'):
                a, b = re.findall(r"\d+", line)
                windows["quiet"] = (int(a) + shift, int(b) + shift)
            elif m := re.match(r'\{"phase": "(\w+)"', line):
                names.append(m.group(1))
        for i, name in enumerate(names):
            windows[name] = (scrapes[2 * i][1], scrapes[2 * i + 1][0])
    base = None
    gaps: list[tuple[int, int]] = []
    stalls: list[tuple[int, int, list[str]]] = []
    for line in open(args.log, errors="replace"):
        if m := re.match(r"SPIN loop=\d+ image_base=(0x[0-9a-f]+)", line):
            base = m.group(1)
        elif m := re.match(r"SPIN loop=\d+ at_ns=(\d+) gap_us=(\d+)", line):
            gaps.append((int(m.group(1)), int(m.group(2))))
        elif m := re.match(r"STALL loop=\d+ at_ns=(\d+) age_us=(\d+) frames=((?:0x[0-9a-f]+,?)+)", line):
            stalls.append((int(m.group(1)), int(m.group(2)), [a for a in m.group(3).split(",") if a]))
    if args.phase:
        lo, hi = windows[args.phase]
        gaps = [g for g in gaps if lo <= g[0] <= hi]
        stalls = [x for x in stalls if lo <= x[0] <= hi]
    if scrapes:
        inside = [g for g in gaps if any(a <= g[0] <= b + 5_000_000 for a, b in scrapes)]
        print(
            f"{len(inside)} of {len(gaps)} gaps end inside a scrape (or within 5 ms after it): "
            f"{sorted((g for _, g in inside), reverse=True)[:10]}"
        )
    print(f"image_base {base}; {len(gaps)} gaps >= threshold; {len(stalls)} stall captures")
    if gaps:
        h = collections.Counter()
        for _, g in gaps:
            h[1 << (g.bit_length() - 1)] += 1
        print("gap histogram (us, power-of-two floor):", dict(sorted(h.items())))
        print("largest gaps (us):", sorted((g for _, g in gaps), reverse=True)[:15])
    if not stalls or base is None:
        return 0
    addrs = sorted({a for _, _, fr in stalls for a in fr})
    lo_app, hi_app = int(base, 16), int(base, 16) + 0x4000000
    app = [a for a in addrs if lo_app <= int(a, 16) < hi_app]
    other = [a for a in addrs if a not in app]
    sym = dict(
        zip(
            app,
            subprocess.run(
                ["atos", "-o", args.bin, "-l", base, *app], capture_output=True, text=True, check=False
            ).stdout.splitlines(),
        )
    )
    if other:  # the dyld shared cache is mapped at one address in every process of a boot
        sym.update(
            zip(
                other,
                subprocess.run(
                    ["atos", "-p", str(os.getpid()), *other], capture_output=True, text=True, check=False
                ).stdout.splitlines(),
            )
        )

    def short(a: str) -> str:
        s = sym.get(a, a)
        s = re.sub(r"\(in [^)]*\)", "", s)
        return s.strip()

    leaf: collections.Counter[str] = collections.Counter()
    chains: collections.Counter[tuple[str, ...]] = collections.Counter()
    ages = []
    for _, age, fr in stalls:
        if age < args.min_age_us:
            continue
        ages.append(age)
        syms = [short(a) for a in fr]
        leaf[syms[0]] += 1
        chains[tuple(syms[: args.frames])] += 1
    print(f"{len(ages)} captures with age >= {args.min_age_us} us (one per ~1.5 ms of stall)")
    print("\nleaf frames:")
    for s, n in leaf.most_common(args.top):
        print(f"  {n:5d}  {s}")
    print("\nstacks:")
    for ch, n in chains.most_common(args.top):
        print(f"  {n:5d}")
        for s in ch:
            print(f"         {s}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
