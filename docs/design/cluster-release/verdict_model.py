#!/usr/bin/env python3
"""What a `quorum` acknowledgement says when apply can refuse a committed command (cluster-release D3).

From the group-core design review's model (`.campaign/xapiand/reviews/group-core-design.md`, B4),
extended to compare the two answers. The leader prepares each command with the expectations apply
checks: the id's version and strata's fields mark (`expected_fields`, index.hh:79-80; a mismatch is
`fields_conflict`, index.hh:88). w1 and w2 are merges whose expectations the leader chose; w3 is a
client's conditional write (`if_version=0`), which the leader checks at prepare and refuses there
with 409 when its own prediction already mismatches. Every interleaving of the three prepares, a
commit, an apply and an election runs under each pair of an acknowledgement rule and a prepare rule.

Acknowledgement rules:
  A  `quorum` completes at commit; apply is total, so a refusal of a committed command fences the
     replica (application_failed), on every replica alike
  B  `quorum` completes when the proposer applies its placement (index, term), with apply's
     verdict; a refusal is a committed no-op on every replica. A refusal of an expectation the
     leader chose is re-prepared and proposed again; one of the client's own answers 409
Prepare rules:
  applied           expectations from the applied state
  pending           the applied state plus the leader's own unapplied proposals
  pending+start     as pending, and nothing is prepared before the leader applied through its term's
                    first entry (Core's NoOp, core.h:1146): the ready point
  versions+start    as pending+start for versions, but the fields mark from the applied state (a
                    predictor that misses one of apply's checks)
A pair is safe when no acknowledged success was refused and no replica fenced. It is also
transparent when no merge (w1, w2) is answered 409, since master never answers one a conflict.
Exit status 0 when A is safe only under pending+start, and B is safe and transparent under all four.
"""
from __future__ import annotations

import itertools
import sys

RULES = ("applied", "pending", "pending+start", "versions+start")


def run(ack: str, rule: str, order: tuple[str, ...]) -> dict[str, int]:
    log: list[tuple] = []  # (client, expected version, expected fields); NoOp is ("noop",)
    state = {"applied": 0, "version": 0, "fields": 0}
    own: list[int] = []  # log indexes the current leader proposed
    term_start = 0
    committed = 0
    effects: dict[int, bool] = {}  # log index -> accepted
    replies: dict[str, str] = {}  # client -> "ok" or "409"
    placed: dict[str, int] = {}  # client -> its latest log index
    stats = {"false_ack": 0, "fence": 0, "merge_409": 0, "retries": 0}
    fenced = False

    def predict() -> tuple[int, int]:
        pend = [i for i in own if i > state["applied"]]
        v, f = state["version"], state["fields"]
        if rule != "applied":
            v += len(pend)
            if rule != "versions+start":
                f += len(pend)
        return v, f

    def prepare(client: str) -> None:
        if rule.endswith("+start") and state["applied"] < term_start:
            apply_through(term_start)  # the ready point
        v, f = predict()
        if client == "w3" and v != 0:
            replies[client] = "409"  # the client's condition fails at prepare, as on master
            return
        log.append((client, 0 if client == "w3" else v, f))
        own.append(len(log))
        placed[client] = len(log)

    def apply_through(n: int) -> None:
        nonlocal fenced
        while state["applied"] < n:
            i = state["applied"] + 1
            entry = log[i - 1]
            state["applied"] = i
            if entry[0] == "noop":
                continue
            client, ev, ef = entry
            ok = ev == state["version"] and ef == state["fields"]
            effects[i] = ok
            if ok:
                state["version"] += 1
                state["fields"] += 1  # each write declares a new field
            if ack == "A" and not ok:
                fenced = True
            if ack == "B" and placed.get(client) == i and client not in replies:
                if ok:
                    replies[client] = "ok"
                elif client == "w3":
                    replies[client] = "409"
                else:  # the leader's own expectation was stale: prepare again from the state now
                    stats["retries"] += 1
                    prepare(client)

    for step in order:
        if step.startswith("prepare"):
            prepare(step.split(":")[1])
        elif step == "commit":
            committed = len(log)
            if ack == "A":
                for i in range(1, committed + 1):
                    if log[i - 1][0] != "noop" and placed.get(log[i - 1][0]) == i:
                        replies.setdefault(log[i - 1][0], "ok")
        elif step == "apply":
            apply_through(committed)
        elif step == "elect":
            log.append(("noop",))  # the new leader holds every entry so far and appends its NoOp
            term_start = len(log)
            own = []
    for _ in range(4):  # drain: commit and apply everything, re-proposals included
        apply_through(len(log))
    if ack == "A":
        for c, i in placed.items():
            replies.setdefault(c, "ok")
    for c, r in replies.items():
        if r == "ok" and not effects.get(placed.get(c, 0), False):
            stats["false_ack"] += 1
        if r == "409" and c != "w3":
            stats["merge_409"] += 1
    stats["fence"] = int(fenced)
    return stats


def main() -> int:
    steps = ["prepare:w1", "prepare:w2", "prepare:w3", "commit", "apply", "elect"]
    orders = [o for o in itertools.permutations(steps) if o.index("prepare:w1") < o.index("prepare:w2")]
    ok = True
    for ack in ("A", "B"):
        for rule in RULES:
            totals = {"false_ack": 0, "fence": 0, "merge_409": 0, "retries": 0}
            bad = []
            for order in orders:
                s = run(ack, rule, order)
                for k in totals:
                    totals[k] += bool(s[k]) if k != "retries" else s[k]
                if s["false_ack"] or s["fence"] or s["merge_409"]:
                    bad.append(" ".join(order))
            safe = totals["false_ack"] == 0 and totals["fence"] == 0
            print(f"{ack} {rule}: of {len(orders)} schedules, {totals['false_ack']} acknowledge a refused write, "
                  f"{totals['fence']} fence the replicas, {totals['merge_409']} answer a merge 409; "
                  f"{totals['retries']} re-proposals")
            for o in bad[:2]:
                print(f"  {o}")
            if ack == "A":
                ok &= safe == (rule == "pending+start")
            else:
                ok &= safe and totals["merge_409"] == 0
    print("RESULT", "as expected" if ok else "UNEXPECTED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
