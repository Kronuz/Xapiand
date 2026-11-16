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

"""Cluster histories and their checker.

A history is one JSON line per invocation and one per completion:

    {"client": "get@node2", "op": "get", "key": "probe/1", "value": "t3", "level": "prefix",
     "outcome": "ok", "time": 1.25}

The checker treats each key as a register whose writes carry unique values, and runs three checks:

- written: every read returns the initial null or a value some write on its key had begun to write.
- prefix: one order of the key's writes explains every read as a prefix of it (durability-modes
  D10), monotonic on each node's launch and never behind a `quorum` write that node acknowledged.
- linearizable: the writes and the reads at level `linearizable` linearize (Gibbons and Korach's zones).

python3 -I harness/cluster/history.py FILE [--level prefix|linearizable]
"""

from __future__ import annotations

import argparse
import itertools
import json
import sys
import time
from collections import defaultdict
from dataclasses import dataclass, replace
from pathlib import Path
from typing import Any, TextIO

OPS = ("write", "read", "get")
READS = ("read", "get")
OUTCOMES = ("invoke", "ok", "fail", "info")
READ_LEVELS = ("prefix", "linearizable")
WRITE_LEVELS = ("quorum", "none")
CHECKS = ("written", "prefix", "linearizable")
INF = float("inf")

type Value = str | int | float | bool | None


class HistoryError(ValueError):
    """A history the checker cannot read."""


@dataclass(frozen=True)
class Op:
    """One invocation and its completion. An unknown completion (`info`) completes at infinity."""

    client: str
    op: str
    key: str
    value: Value
    level: str
    outcome: str
    invoke: float
    complete: float
    line: int
    node: str | None = None  # the node that served or acknowledged it, when the history names one
    launch: int = 1  # that node's launch: a restart starts the next

    @property
    def is_read(self) -> bool:
        return self.op in READS

    def describe(self) -> str:
        end = "?" if self.complete == INF else f"{self.complete:.3f}"
        where = f" on {self.node}#{self.launch}" if self.node else ""
        return f"{self.op}({self.value!r}) by {self.client}{where} [{self.invoke:.3f}, {end}] line {self.line}"


class Recorder:
    """Appends a history's lines as a harness issues and completes its operations."""

    def __init__(self, path: Path) -> None:
        self.path = path
        self.file: TextIO = path.open("a", buffering=1)
        self.start = time.monotonic()
        self.last = -1.0

    def record(
        self, client: str, op: str, key: str, value: Value, level: str, outcome: str, **extra: str | int
    ) -> None:
        line = {"client": client, "op": op, "key": key, "value": value, "level": level, "outcome": outcome}
        self.last = max(round(time.monotonic() - self.start, 6), round(self.last + 1e-6, 6))  # strictly increasing
        line["time"] = self.last
        self.file.write(json.dumps(line | extra) + "\n")

    def close(self) -> None:
        self.file.close()


def load(lines: list[str]) -> list[Op]:
    """Pairs each client's invocations with their completions; an invocation left open is `info`."""
    ops: list[Op] = []
    pending: dict[str, tuple[int, dict[str, Any]]] = {}  # decoded JSON lines
    finished: dict[str, float] = {}  # each client's last completion
    for number, text in enumerate(lines, 1):
        if not text.strip():
            continue
        try:
            entry = json.loads(text)
        except json.JSONDecodeError as error:
            raise HistoryError(f"line {number}: {error}") from None
        if not isinstance(entry, dict):
            raise HistoryError(f"line {number}: not an object")
        for field in ("client", "op", "key", "level", "outcome", "time", "value"):
            if field not in entry:
                raise HistoryError(f"line {number}: no {field!r}")
        client, op, outcome = str(entry["client"]), entry["op"], entry["outcome"]
        if op not in OPS or outcome not in OUTCOMES:
            raise HistoryError(f"line {number}: op {op!r} or outcome {outcome!r} unknown")
        if entry["level"] not in (READ_LEVELS if op in READS else WRITE_LEVELS):
            raise HistoryError(f"line {number}: level {entry['level']!r} is not a {op} level")
        if outcome == "invoke":
            if client in pending:
                raise HistoryError(f"line {number}: {client} invokes while line {pending[client][0]} is open")
            if float(entry["time"]) <= finished.get(client, -INF):  # a tie keeps the client's program order
                entry = entry | {"time": finished[client] + 1e-9}
            pending[client] = (number, entry)
            continue
        if client not in pending:
            raise HistoryError(f"line {number}: {client} completes nothing")
        start, call = pending.pop(client)
        if call["op"] != op or call["key"] != entry["key"]:
            raise HistoryError(f"line {number}: completes a different operation than line {start}")
        value = entry["value"] if op in READS else call["value"]
        complete = INF if outcome == "info" else float(entry["time"])
        finished[client] = float(entry["time"])
        ops.append(_op(call, value, outcome, complete, start))
    for start, call in pending.values():
        ops.append(_op(call, call["value"] if call["op"] == "write" else None, "info", INF, start))
    return ops


def _op(call: dict[str, Any], value: object, outcome: str, complete: float, line: int) -> Op:
    if not (value is None or isinstance(value, str | int | float | bool)):
        raise HistoryError(f"line {line}: value {value!r} is not a scalar")
    invoke = float(call["time"])
    return Op(
        str(call["client"]),
        str(call["op"]),
        str(call["key"]),
        value,
        str(call["level"]),
        outcome,
        invoke,
        complete,
        line,
        str(call["node"]) if call.get("node") is not None else None,
        int(call.get("launch", 1)),
    )


def check(ops: list[Op], level: str | None = None, disabled: frozenset[str] = frozenset()) -> list[str]:
    """Returns the violations. `level` overrides every read's own level; `disabled` exists for the self-test's mutants."""
    by_key: dict[str, list[Op]] = defaultdict(list)
    for op in ops:
        by_key[op.key].append(op)
    violations: list[str] = []
    for key, key_ops in sorted(by_key.items()):
        writes: dict[Value, Op] = {}
        for op in key_ops:
            if op.op == "write":
                if op.value is None or op.value in writes:
                    raise HistoryError(f"line {op.line}: key {key!r} writes {op.value!r} twice or writes null")
                writes[op.value] = op
        reads = [op for op in key_ops if op.is_read and op.outcome == "ok"]
        if "written" not in disabled:
            violations += _written(key, reads, writes)
        if "prefix" not in disabled:
            violations += _prefix(key, reads, writes)
        if "linearizable" not in disabled:
            chosen = [op for op in reads if (level or op.level) == "linearizable"]
            violations += _linearizable(key, chosen, writes)
    return violations


def _source(read: Op, writes: dict[Value, Op]) -> Op | None:
    """The write a read observed, if it could have observed it."""
    write = writes.get(read.value)
    if write is None or write.outcome == "fail" or write.invoke > read.complete:
        return None
    return write


def _written(key: str, reads: list[Op], writes: dict[Value, Op]) -> list[str]:
    return [
        f"{key}: written: {read.describe()} returns a value no write had begun to write"
        for read in reads
        if read.value is not None and _source(read, writes) is None
    ]


def _prefix(key: str, reads: list[Op], writes: dict[Value, Op]) -> list[str]:
    """Consistent prefix (durability-modes D10): some one order of the writes explains every read.

    A read of v sees the prefix that ends at v (null: the empty prefix). The order must put a write
    that completed before another began first; each node's launch reads monotonically in real
    time; and a node's read is never behind a `quorum` write that node acknowledged before it.
    Each rule is a "before" edge between writes, so one order exists when the edges have no cycle.
    A `none` write may commit after its acknowledgement, so it orders nothing by real time. A read
    whose history line names no node may come from any replica, so only the order binds it.
    """
    violations: list[str] = []
    edges: dict[Value, dict[Value, str]] = defaultdict(dict)  # a: {b: why a precedes b}

    def before(a: Value, b: Value, why: str) -> None:
        if a != b and a is not None:
            if b is None:
                violations.append(f"{key}: prefix: {why}")
            else:
                edges[a].setdefault(b, why)

    for a, first in writes.items():
        for b, second in writes.items():
            settled = first.outcome == "ok" and first.level == "quorum"  # a `none` write may commit later
            if settled and second.outcome != "fail" and first.complete < second.invoke:
                edges[a].setdefault(b, f"{first.describe()} completed before {second.describe()} began")
    seen = [read for read in reads if read.value is None or _source(read, writes) is not None]
    by_replica: dict[tuple[str, int], list[Op]] = defaultdict(list)
    for read in seen:
        if read.node is not None:
            by_replica[read.node, read.launch].append(read)
    for replica in by_replica.values():
        for first in replica:
            for second in replica:
                if first.complete < second.invoke:
                    before(first.value, second.value, f"{second.describe()} reads behind {first.describe()}")
    for read in seen:
        for write in writes.values():
            durable = write.outcome == "ok" and write.level == "quorum" and write.complete < read.invoke
            if durable and read.node is not None and write.node == read.node:
                before(
                    write.value, read.value, f"{read.describe()} reads behind {write.describe()}, acknowledged there"
                )
    cycle = _cycle(edges)
    if cycle:
        steps = [edges[a][b] for a, b in itertools.pairwise(cycle)]
        violations.append(f"{key}: prefix: no one order of the writes explains the reads: " + "; ".join(steps))
    return violations


def _cycle(edges: dict[Value, dict[Value, str]]) -> list[Value]:
    """A cycle of the graph as a closed walk [a, ..., a], or [] when it has none."""
    state: dict[Value, int] = {}  # 1 on the stack, 2 done
    for root in list(edges):
        if root in state:
            continue
        stack: list[tuple[Value, list[Value]]] = [(root, list(edges[root]))]
        path = [root]
        state[root] = 1
        while stack:
            node, todo = stack[-1]
            if not todo:
                state[node] = 2
                stack.pop()
                path.pop()
                continue
            nxt = todo.pop()
            if state.get(nxt) == 1:
                return [*path[path.index(nxt) :], nxt]
            if nxt not in state:
                state[nxt] = 1
                path.append(nxt)
                stack.append((nxt, list(edges.get(nxt, {}))))
    return []


def _linearizable(key: str, reads: list[Op], writes: dict[Value, Op]) -> list[str]:
    """Gibbons and Korach's test for a register with unique writes whose reads name their write.

    A cluster is a write and the reads of its value; its zone runs from the cluster's earliest
    completion to its latest invocation, forward when the first is earlier. The history is
    linearizable when no read precedes its write, no two forward zones overlap, and no backward
    zone lies inside a forward one. A write nobody read may be dropped (it could take effect last),
    so only acknowledged `quorum` writes and the writes some read observed form clusters. A `none`
    write is acknowledged before it commits (cluster-release D4), so an observed one completes at
    infinity, as an `info` write does.
    """
    clusters: dict[Value, list[Op]] = defaultdict(list)
    violations: list[str] = []
    for read in reads:
        write = _source(read, writes) if read.value is not None else None
        if read.value is not None and write is None:
            continue  # the written check names it
        clusters[read.value].append(read)
    initial = Op("initial", "write", key, None, "quorum", "ok", -INF, -INF, 0)
    members: dict[Value, list[Op]] = {None: [initial]}
    for value, write in writes.items():
        required = write.outcome == "ok" and write.level == "quorum"
        if required or value in clusters:
            members[value] = [replace(write, complete=INF) if write.level == "none" else write]
    forward: list[tuple[float, float, Value]] = []
    backward: list[tuple[float, float, Value]] = []
    for value, group in members.items():
        group += clusters.get(value, [])
        first_end = min(op.complete for op in group)
        last_start = max(op.invoke for op in group)
        if first_end < last_start:
            forward.append((first_end, last_start, value))
        else:
            backward.append((last_start, first_end, value))
    forward.sort(key=lambda zone: zone[0])
    for (_, end, a), (start, _, b) in itertools.pairwise(forward):
        if start < end:
            violations.append(f"{key}: linearizable: {_cluster(a, members)} and {_cluster(b, members)} overlap")
    for start, end, value in backward:
        for outer_start, outer_end, outer in forward:
            if outer_start < start and end < outer_end:
                violations.append(
                    f"{key}: linearizable: {_cluster(value, members)} falls inside {_cluster(outer, members)}"
                )
    return violations


def _cluster(value: Value, members: dict[Value, list[Op]]) -> str:
    group = members[value]
    reads = [op for op in group if op.is_read]
    head = "the initial null" if value is None else group[0].describe()
    tail = f" and its read {reads[-1].describe()}" if reads else ""
    return head + tail


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="history.py", description="Check a cluster history.")
    parser.add_argument("history", type=Path)
    parser.add_argument("--level", choices=READ_LEVELS, help="check every read at this level, not its own")
    args = parser.parse_args(argv)
    try:
        ops = load(args.history.read_text().splitlines())
        violations = check(ops, args.level)
    except (HistoryError, OSError) as error:
        print(f"history: {error}", file=sys.stderr)
        return 2
    keys = len({op.key for op in ops})
    for violation in violations:
        print(f"  violation: {violation}")
    verdict = "VIOLATIONS" if violations else "ok"
    print(f"history: {len(ops)} operations on {keys} keys at {args.level or 'their own levels'}: {verdict}")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
