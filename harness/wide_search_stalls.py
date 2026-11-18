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

"""wide_search_stalls.py BIN LOG...: what a loop was running when it stalled.

Reads the STALL captures that `harness/restore_stall_reactor_lag.patch` prints (env XAPIAND_LAG_SPIN=<threshold us>)
into a node's log, symbolizes their frames against BIN (`atos` on macOS, `addr2line` elsewhere) and prints, for each
function, the stalls whose stack holds it, then the longest stalls' stacks.
"""

import collections
import re
import subprocess
import sys

BIN, LOGS = sys.argv[1], sys.argv[2:]
STALL = re.compile(r"STALL loop=(\d+) at_ns=(\d+) age_us=(\d+) frames=((?:0x[0-9a-f]+,?)+)")
BASE = re.compile(r"SPIN loop=\d+ image_base=(0x[0-9a-f]+)")

base = None
stalls: list[tuple[int, int, list[str]]] = []
for log in LOGS:
    for line in open(log, errors="replace"):
        if m := BASE.match(line):
            base = m.group(1)
        elif m := STALL.match(line):
            stalls.append((int(m.group(1)), int(m.group(3)), [a for a in m.group(4).split(",") if a]))
print(f"{len(stalls)} stalls captured in {len(LOGS)} logs")
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
for loop, age, frames in sorted(stalls, key=lambda s: -s[1])[:8]:
    print(f"loop {loop} stalled {age} us:")
    for a in frames[:24]:
        print(f"    {name.get(a, a)}")
