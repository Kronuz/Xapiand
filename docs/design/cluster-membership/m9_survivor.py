"""M9 of docs/design/cluster-membership.md: which survivor a forced recovery keeps, as an executable model.

Five (and seven) voters under consensus's rules (ARCHITECTURE.md: a vote compares last term then last index,
a leader appends a current-term no-op on election, a leader commits only a current-term entry a
majority matched; a follower's commit index is min(leader's, its matched index)). Random
schedules, then any three voters lost. Rules for "the most advanced survivor":
  index  the longest log
  vote   most up to date as a vote compares (last term, then last index)
  known  among survivors holding every entry up to the highest commit index any survivor heard,
         the most up to date as a vote compares
Per rule: `known loss`, the chosen survivor lacks an entry another survivor heard committed;
`held loss`, it lacks a committed entry another survivor holds. The model checks its own safety
(no two committed entries at one index differ; a follower's known commits are committed).
`unseen loss`: a committed entry no survivor holds, which no plan can list. Revision 1 adds the
seven-voter runs (any four lost, three survivors) and the plan's statement: under `known`, every
committed entry the chosen survivor lacks is either listed (another survivor holds it) or unseen.
Exits 0 when the expected outcome holds: index and vote show known losses, known never does.
"""

from __future__ import annotations

import itertools
import random
import sys



class Node:
    def __init__(self, i: int) -> None:
        self.i, self.term, self.voted, self.log, self.commit, self.leader = i, 0, None, [], 0, False
        self.match: dict[int, int] = {}

    def last(self) -> tuple[int, int]:
        return (self.log[-1] if self.log else 0, len(self.log))


class Cluster:
    def __init__(self, n: int = 5) -> None:
        self.nodes = [Node(i) for i in range(n)]
        self.majority = n // 2 + 1
        self.committed: dict[int, int] = {}

    @staticmethod
    def observe(v: Node, term: int) -> None:
        if term > v.term:
            v.term, v.voted, v.leader = term, None, False

    def campaign(self, c: Node, voters: list[Node]) -> bool:
        c.term, c.voted, c.leader = c.term + 1, c.i, False
        votes = 1
        for v in voters:
            if v is c:
                continue
            if v.term > c.term:
                self.observe(c, v.term)
                return False
            self.observe(v, c.term)
            if v.voted in (None, c.i) and c.last() >= v.last():
                v.voted, votes = c.i, votes + 1
        if votes >= self.majority:
            c.leader = True
            c.log.append(c.term)  # the current-term no-op
            c.match = {c.i: len(c.log)}
        return c.leader

    def propose(self, leader: Node) -> None:
        if leader.leader:
            leader.log.append(leader.term)
            leader.match[leader.i] = len(leader.log)

    def replicate(self, leader: Node, f: Node, upto: int | None = None, rng: random.Random | None = None) -> None:
        if not leader.leader or f is leader:
            return
        if f.term > leader.term:
            self.observe(leader, f.term)
            return
        self.observe(f, leader.term)
        f.leader = False
        p = 0
        while p < min(len(f.log), len(leader.log)) and f.log[p] == leader.log[p]:
            p += 1
        if upto is None:
            upto = rng.randint(p, len(leader.log)) if rng else len(leader.log)
        assert p <= upto <= len(leader.log)
        if upto > p:
            del f.log[p:]  # f.log[p], if any, conflicts with the leader's
            f.log.extend(leader.log[p:upto])
        leader.match[f.i] = max(leader.match.get(f.i, 0), upto)
        f.commit = max(f.commit, min(leader.commit, upto))
        for n in range(len(leader.log), leader.commit, -1):
            if leader.log[n - 1] != leader.term:
                break
            if sum(m >= n for m in leader.match.values()) >= self.majority:
                leader.commit = n
                for k in range(1, n + 1):
                    assert self.committed.setdefault(k, leader.log[k - 1]) == leader.log[k - 1], "model safety"
                break

    def check(self) -> None:
        for v in self.nodes:
            for k in range(1, v.commit + 1):
                assert self.committed.get(k) == v.log[k - 1], "a known commit that is not committed"


def choose(rule: str, survivors: list[Node]) -> Node:
    vote_key = lambda s: (*s.last(), -s.i)  # noqa: E731
    if rule == "index":
        return max(survivors, key=lambda s: (len(s.log), -s.i))
    if rule == "vote":
        return max(survivors, key=vote_key)
    k = max(s.commit for s in survivors)
    ref = next(s for s in survivors if s.commit == k)
    return max((s for s in survivors if s.log[:k] == ref.log[:k]), key=vote_key)


def lacks(chosen: Node, k: int, term: int) -> bool:
    return len(chosen.log) < k or chosen.log[k - 1] != term


def losses(cluster: Cluster, survivors: list[Node], chosen: Node) -> tuple[int, int]:
    known = {(k, s.log[k - 1]) for s in survivors for k in range(1, s.commit + 1)}
    held = {(k, t) for s in survivors for k, t in enumerate(s.log, 1) if cluster.committed.get(k) == t}
    return sum(lacks(chosen, k, t) for k, t in known), sum(lacks(chosen, k, t) for k, t in held)


def unlisted(cluster: Cluster, survivors: list[Node], chosen: Node) -> tuple[int, int]:
    """Committed entries the chosen survivor lacks: (listed by the plan, unseen by every survivor)."""
    listed = unseen = 0
    for k, t in cluster.committed.items():
        if not lacks(chosen, k, t):
            continue
        if any(not lacks(s, k, t) for s in survivors):
            listed += 1
        else:
            unseen += 1
    return listed, unseen


def trace_vote() -> tuple[Cluster, list[Node]]:
    """B heard that e (index 1, term 1) is committed; C holds only its own term-2 no-op."""
    c = Cluster()
    a, b, cc, d, e = c.nodes
    assert c.campaign(a, [b, cc])  # term 1: A's no-op e at index 1
    c.replicate(a, b, 1)  # e on A and B only
    assert c.campaign(cc, [d, e])  # term 2: C's no-op at index 1, never replicated
    assert not c.campaign(a, [b, d])  # term 2: D already voted for C
    assert c.campaign(a, [b, d])  # term 3: A's no-op f at index 2
    c.replicate(a, d, 2)
    c.replicate(a, e, 2)  # f, and e below it, committed on A, D, E
    c.replicate(a, b, 1)  # a heartbeat: B learns commit index 1
    c.check()
    return c, [b, cc]


def trace_index() -> tuple[Cluster, list[Node]]:
    """A holds a long uncommitted term-1 suffix; D holds term-2 entries it heard committed."""
    c = Cluster()
    a, b, cc, d, e = c.nodes
    assert c.campaign(a, [b, cc])
    for f in (b, cc, d, e):
        c.replicate(a, f, 1)
    for _ in range(4):
        c.propose(a)
    c.replicate(a, b, 5)  # indexes 2..5 on A and B: uncommitted
    assert c.campaign(cc, [d, e])  # term 2
    c.propose(cc)
    c.replicate(cc, d, 3)
    c.replicate(cc, e, 3)  # committed 2..3
    c.replicate(cc, d, 3)  # D hears commit index 3
    c.check()
    return c, [a, d]


def explore(runs: int, steps: int, seed: int, n: int = 5) -> dict[str, list[int]]:
    rng = random.Random(seed)
    # cases, known losses, held losses, then under `known` only: unseen losses
    counts = {r: [0, 0, 0, 0] for r in ("index", "vote", "known")}
    for _ in range(runs):
        c = Cluster(n)
        for _ in range(steps):
            x = rng.random()
            if x < 0.15:
                cand = rng.choice(c.nodes)
                c.campaign(cand, rng.sample(c.nodes, rng.randint(1, len(c.nodes))))
            elif x < 0.45:
                leaders = [v for v in c.nodes if v.leader]
                if leaders:
                    c.propose(rng.choice(leaders))
            else:
                leaders = [v for v in c.nodes if v.leader]
                if leaders:
                    c.replicate(rng.choice(leaders), rng.choice(c.nodes), rng=rng)
        c.check()
        for group in itertools.combinations(c.nodes, c.majority - 1):
            survivors = list(group)
            for rule in counts:
                chosen = choose(rule, survivors)
                known, held = losses(c, survivors, chosen)
                counts[rule][0] += 1
                counts[rule][1] += known > 0
                counts[rule][2] += held > 0
                counts[rule][3] += unlisted(c, survivors, chosen)[1] > 0
    return counts


def main() -> int:
    ok = True

    def show(name: str, got: object, want: object) -> None:
        nonlocal ok
        ok &= got == want
        print(f"{'ok  ' if got == want else 'FAIL'} {name}: {got!r} (want {want!r})")

    c, s = trace_vote()
    for rule, want in (("index", (0, 0)), ("vote", (1, 1)), ("known", (0, 0))):
        show(f"trace vote, rule {rule}: (known, held) entries lost", losses(c, s, choose(rule, s)), want)
    c, s = trace_index()
    for rule, want in (("index", (2, 2)), ("vote", (0, 0)), ("known", (0, 0))):
        show(f"trace index, rule {rule}: (known, held) entries lost", losses(c, s, choose(rule, s)), want)
    for n, runs in ((5, 4000), (7, 1000)):
        counts = explore(runs=runs, steps=60, seed=20261120, n=n)
        for rule, (cases, known, held, unseen) in counts.items():
            print(
                f"     explore {n} voters, {rule}: {cases} survivor sets, known loss in {known}, "
                f"held loss in {held}, a commit no survivor holds in {unseen}"
            )
        show(f"explore {n}: index has known losses", counts["index"][1] > 0, True)
        if n == 5:  # the vote rule's rare case (3 in 40,000) is not reached at seven in 1,000 runs
            show(f"explore {n}: vote has known losses", counts["vote"][1] > 0, True)
        show(f"explore {n}: known never has a known loss", counts["known"][1], 0)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
