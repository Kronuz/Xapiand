#!/usr/bin/env python3
"""Reads a node.log written under XAPIAND_LAG_SPIN: the sleep-free probe's SPIN gaps and the STALL stacks sampled
while the loop was held, symbolizes the stacks with atos and prints the gap histogram and each stack's frames.

  harness/restore_stall_stalls.py BIN node.log [--top N] [--frames N]
"""

from __future__ import annotations

import argparse
import collections
import re
import subprocess


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("bin")
    ap.add_argument("log")
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--frames", type=int, default=14)
    ap.add_argument("--min-age-us", type=int, default=0)
    args = ap.parse_args()
    base = None
    gaps: list[tuple[int, int]] = []
    stalls: list[tuple[int, int, list[str]]] = []
    for line in open(args.log, errors="replace"):
        if m := re.match(r"SPIN loop=\d+ image_base=(0x[0-9a-f]+)", line):
            base = m.group(1)
        elif m := re.match(r"SPIN loop=\d+ at_ns=(\d+) gap_us=(\d+)", line):
            gaps.append((int(m.group(1)), int(m.group(2))))
        elif m := re.match(r"STALL loop=\d+ at_ns=(\d+) age_us=(\d+) frames=(\S+)", line):
            stalls.append((int(m.group(1)), int(m.group(2)), m.group(3).split(",")))
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
    out = subprocess.run(["atos", "-o", args.bin, "-l", base, *addrs], capture_output=True, text=True).stdout.splitlines()
    sym = dict(zip(addrs, out))

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
