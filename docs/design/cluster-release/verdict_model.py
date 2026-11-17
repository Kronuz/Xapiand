#!/usr/bin/env python3
"""What a `quorum` acknowledgement says when apply can refuse a committed command (cluster-release D3).

From the group-core design review's model (`.campaign/xapiand/reviews/group-core-design.md`, B4,
B8), with per-node state: two voters N1 and N2 share one committed log and each applies it at its
own pace. The leader prepares each command with the expectations apply checks: the id's version
and strata's fields mark (`expected_fields`, index.hh:79-80; a mismatch is `fields_conflict`). w1
and w2 are merges whose expectations the leader chose; w3 is a client's conditional write
(`if_version=0`), checked at prepare; w4 (B only) is a document apply finds `invalid`. Every
interleaving of the prepares (at the current leader), a commit, each node's apply and an election
runs under each pair of an acknowledgement rule and a prepare rule.

Acknowledgement rules:
  A  `quorum` completes at commit; a refusal of a committed command fences the replicas
  B  `quorum` completes when the request's proposer applies its placement, with apply's verdict.
     A refusal of an expectation the leader chose (`version_conflict`, `fields_conflict`) is
     re-submitted through the write path: prepared again at the leader, or forwarded once to it
     when the proposer was deposed, which then owns the answer. At most ATTEMPTS placements per
     request; after the last, a retryable 503. `invalid` answers 400 and a failed client
     condition 409; neither is re-submitted
Prepare rules:
  applied           expectations from the leader's applied state
  pending           plus the leader's own unapplied proposals of its term
  pending+start     as pending, and nothing is prepared before the leader applied through its
                    term's first entry (Core's NoOp, core.h:1146): the ready point
  versions+start    as pending+start for versions, the fields mark from the applied state (a
                    predictor that misses one of apply's checks)
Checks: no acknowledged success was refused; replicas agree on every verdict; A fences nothing;
under B every request is answered, no merge is answered 409, `invalid` is never re-submitted, no
request exceeds ATTEMPTS, and pending+start makes no re-submission. Exit status 0 when A is safe
only under pending+start and B passes every check under all four, the deposed-proposer forward
reached.
"""
from __future__ import annotations

import itertools
import sys

RULES = ("applied", "pending", "pending+start", "versions+start")
ATTEMPTS = 3
NODES = ("N1", "N2")


class Run:
    def __init__(self, ack: str, rule: str) -> None:
        self.ack, self.rule = ack, rule
        self.log: list[tuple] = []  # ("noop",) or (client, expected version, expected fields)
        self.commit = 0
        self.node = {n: {"applied": 0, "version": 0, "fields": 0} for n in NODES}
        self.leader, self.term_start, self.own = "N1", 0, []
        self.req: dict[str, dict] = {}  # client -> proposer, placement, attempts, reply
        self.effects: dict[int, str] = {}  # index -> verdict, the same on every node
        self.s = {"false_ack": 0, "fence": 0, "merge_409": 0, "unanswered": 0, "diverge": 0,
                  "invalid_resubmit": 0, "over_bound": 0, "resubmits": 0, "forwards": 0, "merge_503": 0}

    def predict(self, n: str) -> tuple[int, int]:
        st = self.node[n]
        pend = [i for i in self.own if i > st["applied"] and self.log[i - 1][0] != "w4"]
        v, f = st["version"], st["fields"]
        if self.rule != "applied":
            v += len(pend)
            if self.rule != "versions+start":
                f += len(pend)
        return v, f

    def prepare(self, client: str) -> None:
        n = self.leader
        if self.rule.endswith("+start") and self.node[n]["applied"] < self.term_start:
            self.commit = max(self.commit, self.term_start)  # the NoOp commits, and all before it
            self.apply(n, self.term_start)
        v, f = self.predict(n)
        r = self.req.setdefault(client, {"attempts": 0, "reply": None})
        if client == "w3" and v != 0:
            r["reply"] = "409"  # the client's condition fails at prepare, as on master
            return
        self.log.append((client, 0 if client == "w3" else v, f))
        self.own.append(len(self.log))
        r.update(proposer=n, index=len(self.log), attempts=r["attempts"] + 1)

    def apply(self, n: str, through: int | None = None) -> None:
        st = self.node[n]
        through = self.commit if through is None else through
        while st["applied"] < through:
            i = st["applied"] + 1
            st["applied"] = i
            e = self.log[i - 1]
            if e[0] == "noop":
                continue
            client, ev, ef = e
            if client == "w4":
                verdict = "invalid"
            elif ev == st["version"] and ef == st["fields"]:
                verdict = "accepted"
                st["version"] += 1
                st["fields"] += 1  # each write declares a new field
            else:
                verdict = "conflict"
            if self.effects.setdefault(i, verdict) != verdict:
                self.s["diverge"] += 1
            if self.ack == "A":
                self.s["fence"] |= verdict != "accepted"
                continue
            r = self.req[client]
            if r.get("index") != i or r.get("proposer") != n or r["reply"]:
                continue
            if verdict == "accepted":
                r["reply"] = "ok"
            elif verdict == "invalid":
                r["reply"] = "400"
            elif client == "w3":
                r["reply"] = "409"
            elif r["attempts"] >= ATTEMPTS:
                r["reply"] = "503"
            else:
                self.s["resubmits"] += 1
                self.s["forwards"] += n != self.leader
                self.prepare(client)

    def step(self, step: str) -> None:
        if step.startswith("prepare"):
            self.prepare(step.split(":")[1])
        elif step == "commit":
            self.commit = len(self.log)
            if self.ack == "A":
                for r in self.req.values():
                    if r.get("index", 1 << 30) <= self.commit:
                        r["reply"] = r["reply"] or "ok"
        elif step.startswith("apply"):
            self.apply(step.split(":")[1])
        elif step == "elect":
            self.leader = "N2" if self.leader == "N1" else "N1"
            self.log.append(("noop",))  # the new leader holds every entry so far
            self.term_start, self.own = len(self.log), []

    def finish(self) -> dict[str, int]:
        for _ in range(4 * ATTEMPTS):
            self.step("commit")
            for n in NODES:
                self.apply(n)
        for c, r in self.req.items():
            if r["reply"] is None:
                self.s["unanswered"] += 1
            if r["reply"] == "ok" and self.effects.get(r["index"]) != "accepted":
                self.s["false_ack"] += 1
            if c in ("w1", "w2") and r["reply"] == "409":
                self.s["merge_409"] += 1
            if c in ("w1", "w2") and r["reply"] == "503":
                self.s["merge_503"] += 1
            if c == "w4" and r["attempts"] > 1:
                self.s["invalid_resubmit"] += 1
            if r["attempts"] > ATTEMPTS:
                self.s["over_bound"] += 1
        return self.s


def main() -> int:
    ok, forwards = True, 0
    for ack in ("A", "B"):
        writes = ["prepare:w1", "prepare:w2", "prepare:w3"] + (["prepare:w4"] if ack == "B" else [])
        steps = writes + ["commit", "apply:N1", "apply:N2", "elect"]
        orders = [o for o in itertools.permutations(steps) if o.index("prepare:w1") < o.index("prepare:w2")]
        for rule in RULES:
            tot: dict[str, int] = {}
            for order in orders:
                r = Run(ack, rule)
                for st in order:
                    r.step(st)
                for k, v in r.finish().items():
                    tot[k] = tot.get(k, 0) + (v if k in ("resubmits", "forwards") else bool(v))
            print(f"{ack} {rule}: {len(orders)} schedules; " + ", ".join(f"{k} {v}" for k, v in tot.items()))
            if ack == "A":
                ok &= (tot["false_ack"] == 0 and tot["fence"] == 0) == (rule == "pending+start")
            else:
                bad = ("false_ack", "fence", "merge_409", "unanswered", "diverge", "invalid_resubmit", "over_bound")
                ok &= all(tot[k] == 0 for k in bad)
                if rule == "pending+start":
                    ok &= tot["resubmits"] == 0 and tot["merge_503"] == 0
                forwards += tot["forwards"]
    ok &= forwards > 0
    print("RESULT", "as expected" if ok else "UNEXPECTED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
