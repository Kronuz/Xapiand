#!/usr/bin/env python3
"""perf_sample.py -- `sample(1)` for Linux: records a process with perf and writes macOS `sample`'s call-graph text.

  perf_sample.py PID SECS OUT [--freq 1000] [--fp]

perf records the on-CPU samples of every thread of PID for SECS seconds (cpu-clock, so it works in a VM without a PMU;
DWARF unwinding unless --fp, since release builds omit frame pointers). The file has one root per thread whose count is
the whole window (FREQ samples a second): a thread that was not on a CPU for the rest of it is shown idle in
`futex_wait`, as `sample` shows a blocked thread. profile_light.py and profile_bulk.sh read it unchanged.
Needs perf and permission to profile PID (root, or kernel.perf_event_paranoid <= 1).
"""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import tempfile


def record(pid: int, secs: int, freq: int, fp: bool, data: str) -> None:
    cg = "fp" if fp else "dwarf,8192"
    cmd = ["perf", "record", "-q", "-e", "cpu-clock", "-F", str(freq), "--call-graph", cg, "-p", str(pid), "-o", data, "--", "sleep", str(secs)]
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


class Node:
    __slots__ = ("count", "kids")

    def __init__(self) -> None:
        self.count = 0
        self.kids: dict[str, Node] = {}


def stacks(data: str):
    """(tid, comm, frames root..leaf) for each sample."""
    out = subprocess.run(["perf", "script", "-i", data, "-F", "comm,tid,ip,sym"], check=True, capture_output=True, text=True, errors="replace").stdout
    head: tuple[int, str] | None = None
    frames: list[str] = []
    for line in out.splitlines() + [""]:
        if not line.strip():
            if head is not None and frames:
                yield head[0], head[1], frames[::-1]
            head, frames = None, []
        elif head is None:
            comm, _, tid = line.strip().rpartition(" ")
            head = (int(tid), comm.strip())
        else:
            parts = line.split(None, 1)
            frames.append(parts[1].strip() if len(parts) > 1 else "[unknown]")


def write(path: str, window: int, threads: dict[int, tuple[str, Node]]) -> None:
    def emit(fh, name: str, node: Node, level: int) -> None:
        fh.write(f"{' ' * (4 + 2 * level)}{node.count} {name}\n")
        for kid_name, kid in sorted(node.kids.items(), key=lambda kv: -kv[1].count):
            emit(fh, kid_name, kid, level + 1)

    with open(path, "w") as fh:
        fh.write(f"Sampling process (perf) for {window} samples\n\nCall graph:\n")
        for tid, (comm, root) in sorted(threads.items()):
            busy = root.count
            root.count = max(window, busy)
            if window > busy:
                idle = root.kids.setdefault("futex_wait", Node())
                idle.count += window - busy
            emit(fh, f"Thread_{tid}: {comm}", root, 0)
        fh.write("\nTotal number in stack (recursive counted multiple, when >=5):\n\nSort by top of stack, same collapsed (when >= 5):\n")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("pid", type=int)
    ap.add_argument("secs", type=int)
    ap.add_argument("out")
    ap.add_argument("--freq", type=int, default=1000)
    ap.add_argument("--fp", action="store_true")
    a = ap.parse_args()
    with tempfile.TemporaryDirectory(dir=os.path.dirname(os.path.abspath(a.out))) as tmp:
        data = os.path.join(tmp, "perf.data")
        record(a.pid, a.secs, a.freq, a.fp, data)
        threads: dict[int, tuple[str, Node]] = {}
        for tid, comm, frames in stacks(data):
            _, root = threads.setdefault(tid, (comm, Node()))
            root.count += 1
            node = root
            for f in frames:
                node = node.kids.setdefault(f, Node())
                node.count += 1
    write(a.out, a.secs * a.freq, threads)
    return 0


if __name__ == "__main__":
    sys.exit(main())
