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

"""wide_search_stalls.py BIN LOG... [--max-age US]: what a loop was running when it stalled.

Reads the STALL captures that `harness/restore_stall_reactor_lag.patch` prints (env XAPIAND_LAG_SPIN=<threshold us>)
into a node's log, symbolizes their frames against BIN (`atos` on macOS, `addr2line` elsewhere) and prints, for each
function, the stalls whose stack holds it, the nearest frame of the binary to each stall's leaf and its caller chain
(what called into a lock or a system call), then the longest stalls' stacks. --max-age drops captures older than US
(a node that hangs at its stop is captured every 1.5 s). Late beats (`SPIN ... gap_us cpu_us`) and stalls split by the
loop thread's CPU time over them: on CPU (at least half the gap) is a long turn, off CPU is a thread preempted or blocked.
"""

import collections
import re
import subprocess
import sys

args = sys.argv[1:]
MAX_AGE = 1 << 62
if "--max-age" in args:
    i = args.index("--max-age")
    MAX_AGE = int(args[i + 1])
    del args[i : i + 2]
BIN, LOGS = args[0], args[1:]
STALL = re.compile(r"STALL loop=(\d+) at_ns=(\d+) age_us=(\d+) frames=((?:0x[0-9a-f]+,?)+)(?: cpu_us=(-?\d+))?")
GAP = re.compile(r"SPIN loop=\d+ at_ns=\d+ gap_us=(\d+) cpu_us=(-?\d+)")
BASE = re.compile(r"SPIN loop=\d+ image_base=(0x[0-9a-f]+)")

base = None
stalls: list[tuple[int, int, list[str]]] = []
oncpu: dict[int, bool] = {}  # stall index -> the loop ran code through it
gaps: list[tuple[int, int]] = []
for log in LOGS:
    for line in open(log, errors="replace"):
        if m := BASE.match(line):
            base = m.group(1)
        elif (m := GAP.match(line)) and int(m.group(1)) <= MAX_AGE:
            gaps.append((int(m.group(1)), int(m.group(2))))
        elif (m := STALL.match(line)) and int(m.group(3)) <= MAX_AGE:
            if m.group(5) is not None:
                oncpu[len(stalls)] = 2 * int(m.group(5)) >= int(m.group(3))
            stalls.append((int(m.group(1)), int(m.group(3)), [a for a in m.group(4).split(",") if a]))


def pct(xs: list[int], q: float) -> int:
    return sorted(xs)[min(len(xs) - 1, int(q * len(xs)))] if xs else 0


on = [g for g, c in gaps if 2 * c >= g]
off = [g for g, c in gaps if 2 * c < g]
print(
    f"{len(gaps)} late beats: on CPU {len(on)} (p50 {pct(on, 0.5)} us, max {max(on, default=0)} us), "
    f"off CPU {len(off)} (p50 {pct(off, 0.5)} us, max {max(off, default=0)} us)"
)
print(
    f"{len(stalls)} stalls captured in {len(LOGS)} logs, on CPU {sum(oncpu.values())}, off CPU {len(oncpu) - sum(oncpu.values())}"
)
if not stalls or base is None:
    sys.exit(0)

addrs = sorted({a for _, _, frames in stalls for a in frames})
if sys.platform == "darwin":
    out = subprocess.run(
        ["atos", "-o", BIN, "-l", base, *addrs], capture_output=True, text=True, check=True
    ).stdout.splitlines()
else:
    out = subprocess.run(
        ["addr2line", "-f", "-C", "-e", BIN, *addrs], capture_output=True, text=True, check=True
    ).stdout.splitlines()[::2]


def short(sym: str) -> str:
    sym = re.sub(r" \(in [^)]*\)", "", sym)
    sym = re.sub(r"<[^<>]*>", "", re.sub(r"<[^<>]*>", "", re.sub(r"<[^<>]*>", "", sym)))
    return sym[:140]


name = {a: short(s) for a, s in zip(addrs, out)}
holding: collections.Counter[str] = collections.Counter()
for _, _, frames in stalls:
    for fn in {name.get(a, a) for a in frames}:
        holding[fn] += 1
print("stalls holding each function (top 40):")
for fn, n in holding.most_common(40):
    print(f"  {n:4d} {fn}")
known = set(name)
for cls in (True, False, None):
    leaf: collections.Counter[str] = collections.Counter()
    for i, (_, _, frames) in enumerate(stalls):
        if oncpu.get(i) is cls:
            mine = [name[a] for a in frames if a in known and not name[a].startswith("0x")]
            leaf[" <- ".join(mine[:4])] += 1
    if leaf:
        label = {True: "on CPU", False: "off CPU", None: "CPU unknown"}[cls]
        print(f"nearest frames of the binary to the leaf, {label} (top 25):")
        for chain, n in leaf.most_common(25):
            print(f"  {n:4d} {chain[:400]}")
for loop, age, frames in sorted(stalls, key=lambda s: -s[1])[:8]:
    print(f"loop {loop} stalled {age} us:")
    for a in frames[:24]:
        print(f"    {name.get(a, a)}")
