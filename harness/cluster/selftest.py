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

"""The history checker against planted histories, and its linearizability test against brute force.

Each planted history names the verdict it must get at `linearizable` and at `prefix`. The three of
A2 (a lost acknowledged write, a stale read, and a stale get-by-id from a partitioned old leader)
fail at `linearizable` and pass at `prefix`; the others pin the written and prefix checks and the
histories every check must accept. Then 3,000 random small histories compare the zone test with an
exhaustive search for a linearization.

`--mutant CHECK` runs it with that check disabled: the self-test must then fail.

python3 harness/cluster/selftest.py [--mutant written|prefix|linearizable]
"""

from __future__ import annotations

import argparse
import itertools
import json
import random
import sys
from dataclasses import dataclass

from history import CHECKS, Op, Value, check, load


@dataclass(frozen=True)
class Call:
    client: str
    op: str
    value: Value
    start: float
    end: float | None  # None: no completion line, an unknown outcome
    level: str = ""
    outcome: str = "ok"
    node: str = "node1"


def lines(calls: list[Call], key: str = "k") -> list[str]:
    events: list[tuple[float, int, dict[str, object]]] = []
    for call in calls:
        level = call.level or ("quorum" if call.op == "write" else "linearizable")
        base: dict[str, object] = {"client": call.client, "op": call.op, "key": key, "level": level, "node": call.node}
        written = call.value if call.op == "write" else None
        events.append((call.start, 0, base | {"value": written, "outcome": "invoke", "time": call.start}))
        if call.end is not None:
            events.append((call.end, 1, base | {"value": call.value, "outcome": call.outcome, "time": call.end}))
    return [json.dumps(event) for _, _, event in sorted(events, key=lambda e: (e[0], e[1]))]


@dataclass(frozen=True)
class Case:
    name: str
    calls: list[Call]
    linearizable: bool  # passes when checked at linearizable
    prefix: bool  # passes when checked at prefix
    own: bool | None = None  # passes when each read is checked at its own level; None skips it


def w(client: str, value: str, start: float, end: float | None, **kw: str) -> Call:
    return Call(client, "write", value, start, end, **kw)


def r(client: str, value: Value, start: float, end: float, op: str = "read", **kw: str) -> Call:
    return Call(client, op, value, start, end, **kw)


CASES = [
    # A2: the three planted histories.
    Case("lost acknowledged write", [w("a", "x", 0, 1), r("b", None, 2, 3), r("c", None, 4, 5)], False, True),
    Case("stale read", [w("a", "x", 0, 1), w("a", "y", 2, 3), r("b", "x", 4, 5)], False, True),
    Case(
        "stale get-by-id from a partitioned old leader",
        [
            w("a", "x", 0, 1, node="node1"),
            w("a", "y", 5, 6, node="node2"),  # node1 is partitioned; node2 leads
            r("c", "x", 7, 8, op="get", node="node1"),
        ],
        False,
        True,
    ),
    # The prefix and written checks, which apply at both levels.
    Case(
        "a client reads back in time",
        [w("a", "x", 0, 1), w("a", "y", 2, 3), r("b", "y", 4, 5, level="prefix"), r("b", "x", 6, 7, level="prefix")],
        False,
        False,
    ),
    Case(
        "a client reads null after a value",
        [w("a", "x", 0, 1), r("b", "x", 2, 3, level="prefix"), r("b", None, 4, 5, level="prefix")],
        False,
        False,
    ),
    Case("a value nobody wrote", [w("a", "x", 0, 1), r("b", "z", 2, 3, level="prefix")], False, False),
    Case("a value read before its write began", [r("b", "x", 0, 1, level="prefix"), w("a", "x", 2, 3)], False, False),
    Case(
        "a refused write's value", [w("a", "x", 0, 1, outcome="fail"), r("b", "x", 2, 3, level="prefix")], False, False
    ),
    # Histories every check accepts.
    Case(
        "concurrent reads either side of a write",
        [w("a", "x", 0, 1), w("a", "y", 2, 6), r("b", "y", 3, 4), r("c", "x", 3.5, 5), r("b", "y", 7, 8)],
        True,
        True,
    ),
    Case("an unknown write that took effect", [w("a", "x", 0, None), r("b", "x", 5, 6), r("c", "x", 7, 8)], True, True),
    Case("an unknown write that did not", [w("a", "x", 0, None), r("b", None, 5, 6)], True, True),
    Case("a `none` write lost", [w("a", "x", 0, 1, level="none"), r("b", None, 2, 3)], True, True),
    Case(
        "prefix reads lag, a linearizable one may not",
        [w("a", "x", 0, 1), w("a", "y", 2, 3), r("b", "x", 4, 5, level="prefix"), r("c", "x", 6, 7)],
        False,
        True,
        own=False,
    ),
    Case(
        "prefix reads lag at their own level",
        [w("a", "x", 0, 1), w("a", "y", 2, 3), r("b", "x", 4, 5, level="prefix"), r("c", "y", 6, 7)],
        False,
        True,
        own=True,
    ),
]


def planted(disabled: frozenset[str]) -> int:
    failures = 0
    for case in CASES:
        ops = load(lines(case.calls))
        for level, expected in (("linearizable", case.linearizable), ("prefix", case.prefix), (None, case.own)):
            if expected is None:
                continue
            violations = check(ops, level, disabled)
            ok = (not violations) == expected
            failures += not ok
            want = "accepted" if expected else "rejected"
            print(
                f"  {'ok:  ' if ok else 'FAIL:'} {case.name} at {level or 'own levels'}: {want} ({len(violations)} violations)"
            )
            if not ok or (violations and level == "linearizable" and not case.prefix):
                for violation in violations[:3]:
                    print(f"         {violation}")
    return failures


def brute(ops: list[Op]) -> bool:
    """Whether some choice of the optional writes and some order respecting real time explains every read."""
    required = [op for op in ops if op.op != "write" or (op.outcome == "ok" and op.level == "quorum")]
    optional = [op for op in ops if op not in required]
    for count in range(len(optional) + 1):
        for chosen in itertools.combinations(optional, count):
            if _search(required + list(chosen)):
                return True
    return False


def _search(ops: list[Op]) -> bool:
    seen: set[tuple[int, Value]] = set()
    full = (1 << len(ops)) - 1

    def step(mask: int, state: Value) -> bool:
        if mask == full:
            return True
        if (mask, state) in seen:
            return False
        seen.add((mask, state))
        bound = min(op.complete for i, op in enumerate(ops) if not mask >> i & 1)
        for i, op in enumerate(ops):
            if mask >> i & 1 or op.invoke > bound:
                continue
            if op.is_read and op.value != state:
                continue
            if step(mask | 1 << i, op.value if op.op == "write" else state):
                return True
        return False

    return step(0, None)


def randomized(disabled: frozenset[str], runs: int = 3000) -> int:
    rng = random.Random(20261116)
    disagreements = 0
    compared = 0
    for _ in range(runs):
        calls: list[Call] = []
        values = [f"v{i}" for i in range(rng.randint(1, 3))]
        for i, value in enumerate(values):
            start = rng.uniform(0, 10)
            kind = rng.random()
            if kind < 0.15:
                calls.append(w(f"w{i}", value, start, None))
            else:
                level = "none" if kind < 0.25 else "quorum"
                calls.append(w(f"w{i}", value, start, start + rng.uniform(0.1, 4), level=level))
        for i in range(rng.randint(1, 4)):
            start = rng.uniform(0, 12)
            calls.append(r(f"r{i}", rng.choice([*values, None]), start, start + rng.uniform(0.1, 3)))
        ops = load(lines(calls))
        if check(ops, "linearizable", frozenset({"prefix", "linearizable"})):
            continue  # a read the written check rejects has no write to linearize against
        compared += 1
        zones = not check(ops, "linearizable", disabled | {"written", "prefix"})
        if zones != brute(ops):
            disagreements += 1
            if disagreements <= 3:
                print(f"  FAIL: zones say {zones}, brute force {not zones}:")
                for op in ops:
                    print(f"         {op.describe()} {op.outcome} {op.level}")
    print(
        f"  {'ok:  ' if not disagreements else 'FAIL:'} zones agree with brute force on {compared - disagreements}/{compared} random histories"
    )
    return disagreements


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(prog="selftest.py", description="Test the history checker.")
    parser.add_argument("--mutant", choices=CHECKS, help="disable this check; the self-test must then fail")
    args = parser.parse_args(argv)
    disabled = frozenset({args.mutant}) if args.mutant else frozenset()
    if disabled:
        print(f"== mutant: the {args.mutant} check is disabled")
    print("== planted histories")
    failures = planted(disabled)
    print("== zones against brute force")
    failures += randomized(disabled)
    print(f"=== VERDICT: {'GREEN' if not failures else f'RED ({failures} failures)'}")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
