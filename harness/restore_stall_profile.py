#!/usr/bin/env python3
"""Shares of a loop thread's busy samples by frame, from a macOS `sample` file (profile_light.parse_sample's rows).

  harness/restore_stall_profile.py restore.sample.txt [--thread http0 (a name; the thread running TcpServer::start is taken too)] [--top 40] [--match REGEX ...]

Busy samples are those whose leaf is not an idle wait. Prints the thread totals, the busiest threads, and for the
loop thread the inclusive share of each frame (once per stack) and the leaf shares.
"""

from __future__ import annotations

import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import profile_light as pl  # noqa: E402


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("sample")
    ap.add_argument("--thread", default="http0")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--match", action="append", default=[])
    args = ap.parse_args()
    rows, _ = pl.parse_sample(args.sample)
    per_thread: collections.Counter[str] = collections.Counter()
    for thread, frames, n in rows:
        if frames and not pl.IDLE.search(frames[-1]):
            per_thread[thread] += n
    print("busiest threads (busy samples):")
    for t, n in per_thread.most_common(8):
        print(f"  {n:6d}  {t[:90]}")
    # The HTTP loop's thread is unnamed in `sample`; it is the one whose stacks run reactor::TcpServer::start's lambda.
    http = {t for t, frames, _ in rows if any("TcpServer::start" in f for f in frames)}
    loop = [t for t in per_thread if args.thread in t or t in http]
    if not loop:
        print("no thread matches", args.thread)
        return 1
    name = max(loop, key=lambda t: per_thread[t])
    total = per_thread[name]
    print(f"\nloop thread {name!r}: {total} busy samples")
    incl: collections.Counter[str] = collections.Counter()
    leaf: collections.Counter[str] = collections.Counter()
    for thread, frames, n in rows:
        if thread != name or not frames or pl.IDLE.search(frames[-1]):
            continue
        leaf[frames[-1]] += n
        for f in set(frames):
            incl[f] += n
    print("\ninclusive (share of the loop's busy samples):")
    for f, n in incl.most_common(args.top):
        print(f"  {100 * n / total:5.1f}%  {n:6d}  {f}")
    print("\nleaf:")
    for f, n in leaf.most_common(20):
        print(f"  {100 * n / total:5.1f}%  {n:6d}  {f}")
    for pat in args.match:
        rx = re.compile(pat)
        n = sum(c for f, c in incl.items() if rx.search(f))
        print(f"\nmatch {pat!r}: frames matching sum (not deduped across frames) {n}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
