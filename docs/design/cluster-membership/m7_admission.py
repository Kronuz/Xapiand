"""M7 of docs/design/cluster-membership.md as an executable model.

Revision 0's rule: a node raises and persists its admission number at each start and hourly; the
root leader records it and answers a second live boot `duplicate` and a number not above its
record `stale`. Keepalives ride the node pair's Bulk link, and an acceptor refuses a hello for a
lane it holds undrained (consensus host-connection.md, tie-break rule 2).

Number rules:
  design     raise and persist at every start, refused or not (revision 0)
  offered    the root commits the number it offers; the node persists it only once offered, and the
             root admits a new boot presenting the recorded or the offered number (the review's fix)
  confirmed  revision 1: a boot presenting the recorded number gets a fresh offer, committed; it
             saves the offer and presents it; the root then commits it as the record with the boot,
             and only then does the node open groups. A boot presenting the pending offer completes
             it (a crash after the save); anything else is `stale`.
  above      revision 3: `stale` only below the record; any other number gets a fresh offer above both
             the record and itself, and only the boot an offer was made to completes it
Liveness:
  leader     a new boot is `duplicate` only beside a boot this leader heard (revision 0)
  record     the hello carries the boot (consensus addition 7); while the recorded boot is not
             silent past the window, every acceptor refuses another boot's hello with `duplicate`;
             a new root leader counts the window from its election

Exits 0 when every check holds; prints one line per check. The first block reproduces the review's
lines (job 261120-121643-c02f08) with the same seed; revisions 1 and 3 repeat the protections.
Admission runs inside the run command, `xapiand [--token TOKEN]` (D157), whose supervisor restarts it
at once; those checks (S6 to S8, under rule "above") use time: the admitted boot is live for W
seconds after it was last heard, and a starting process retries a `duplicate` hello every TICK for
W + TICK before it exits, since the live boot may be its own dead predecessor. S9 explores random
histories, S10 a root salvage, S11 a cold root and S12 a root leader change with time, each beside
revision 1's rule or a mutant that fails.
"""

from __future__ import annotations

import copy
import itertools
import math
import random
import sys
from dataclasses import dataclass

RESTART_CAP = 1000
STEPPED = ("confirmed", "above")


@dataclass
class Root:
    rule: str
    liveness: str
    record: int = 0  # committed number
    offered: int = 0  # committed pending offer (rules "offered", "confirmed", "above"; 0 is none)
    offer_boot: int = 0  # rule "above": the boot the pending offer was made to
    next: int = 1  # rules "confirmed" and "above": the next number to offer
    boot: int = 0  # committed admitted boot
    silent: bool = False  # the admitted boot was silent past the window (it died)
    live: int = 0  # leader memory: a boot heard within the window
    lane: int = 0  # boot holding the node's Bulk lane at the leader

    def leader_change(self) -> None:
        self.live = self.lane = 0

    def died(self) -> None:
        self.live = self.lane = 0
        self.silent = True

    def hello(self, boot: int) -> str | None:
        """None when accepted, else the refusal the dialer reads."""
        if self.liveness == "record" and self.boot and boot != self.boot and not self.silent:
            return "duplicate"
        if self.lane and self.lane != boot:
            return "refused-hello"
        self.lane = boot
        return None

    def number_ok(self, n: int) -> bool:
        return n > self.record if self.rule == "design" else n in (self.record, self.offered)

    def commit(self, boot: int, n: int) -> None:
        self.record, self.boot, self.live, self.silent = n, boot, boot, False
        if self.rule == "offered":
            self.offered = n + 1

    def admit(self, boot: int, n: int) -> str:
        """Rules "confirmed" and "above": one step of a boot's admission or refresh."""
        if self.rule == "above":
            if n < self.record:
                return "stale"
            if self.offered and n == self.offered and boot == self.offer_boot:
                return self.confirm(boot, n)
            self.next = max(self.next, n + 1)
        elif self.offered and n == self.offered:
            return self.confirm(boot, n)
        elif n != self.record:
            return "stale"
        self.offered, self.offer_boot, self.next = self.next, boot, self.next + 1
        return "offer"

    def confirm(self, boot: int, n: int) -> str:
        self.record, self.offered = n, 0
        self.boot, self.live, self.silent = boot, boot, False
        return "admitted"

    def keepalive(self, boot: int, n: int, refresh: bool = False) -> str:
        if boot == self.boot and not self.silent:
            self.live = boot
            if refresh and self.rule in STEPPED:
                return self.admit(boot, n)
            if refresh and self.number_ok(n):
                self.commit(boot, n)
            return "admitted"
        if self.live and self.live != boot:
            return "duplicate"
        if self.rule in STEPPED:
            return self.admit(boot, n)
        if not self.number_ok(n):
            return "stale"
        self.commit(boot, n)
        return "admitted"


class Process:
    def __init__(self, root: Root, disk: int) -> None:
        self.root, self.disk, self.boot = root, disk, 0

    def start(self, crash: str = "") -> str:
        self.boot = random.getrandbits(62) + 1
        if self.root.rule == "design":
            self.disk += 1  # raised and persisted before the keepalive goes out
        return self.retry(crash)

    def retry(self, crash: str = "") -> str:
        """This boot dials the root leader and sends its keepalive; a refused process exits.
        `crash` kills it after the offer's commit ("before-save") or after its save ("before-confirm")."""
        refused = self.root.hello(self.boot)
        if refused:
            return refused
        answer = self.root.keepalive(self.boot, self.disk)
        if answer == "offer":
            if crash == "before-save":
                self.root.died()
                return "crashed"
            self.disk = self.root.offered  # saved only after the root committed the offer
            if crash == "before-confirm":
                self.root.died()
                return "crashed"
            answer = self.root.keepalive(self.boot, self.disk)
        if answer != "admitted":
            self.root.lane = 0  # its link closes as it exits
        elif self.root.rule == "offered":
            self.disk = self.root.offered  # persisted only after the root committed the offer
        return answer

    def hourly(self) -> str:
        if self.root.rule == "design":
            self.disk += 1
        answer = self.root.keepalive(self.boot, self.disk, refresh=True)
        if answer == "offer":
            self.disk = self.root.offered
            answer = self.root.keepalive(self.boot, self.disk, refresh=True)
        if answer == "admitted" and self.root.rule == "offered":
            self.disk = self.root.offered
        return answer


def restarts_until_admitted(p: Process) -> int | None:
    for n in range(1, RESTART_CAP + 1):
        if p.start() == "admitted":
            return n
    return None


def running(root: Root, hours: int) -> Process:
    p = Process(root, 0)
    assert p.start() == "admitted"
    for _ in range(hours):
        assert p.hourly() == "admitted"
    return p


def s1_restored_backup(rule: str) -> int | None:
    """The node dies; its directory comes back from a backup 30 refreshes old; a supervisor restarts it."""
    root = Root(rule, "leader")
    p = running(root, 0)
    backup = p.disk
    for _ in range(30):
        assert p.hourly() == "admitted"
    root.died()
    return restarts_until_admitted(Process(root, backup))


def s1_answers(rule: str, liveness: str) -> set[str]:
    """Every answer a backup 30 refreshes old gets over RESTART_CAP restarts."""
    root = Root(rule, liveness)
    p = running(root, 0)
    backup = p.disk
    for _ in range(30):
        assert p.hourly() == "admitted"
    root.died()
    restored = Process(root, backup)
    return {restored.start() for _ in range(RESTART_CAP)}


def s2_crash_between(rule: str) -> bool:
    """A crash between the root's answer and the node's persist must not lock the node out."""
    root = Root(rule, "leader")
    p = running(root, 3)
    if rule == "design":
        p.disk += 1  # raised and persisted, then the process dies before its keepalive
    else:
        root.commit(p.boot, p.disk)  # the root commits an offer the node never persists
    root.died()
    return restarts_until_admitted(Process(root, p.disk)) is not None


def s2_crash_point(rule: str, point: str, refresh: bool) -> str:
    """A start (or a refresh) killed at `point`; the next boot's answer."""
    root = Root(rule, "record")
    p = running(root, 3)
    if refresh:
        assert root.keepalive(p.boot, p.disk, refresh=True) == "offer"
        if point == "before-confirm":
            p.disk = root.offered
        root.died()
    else:
        root.died()
        assert p.start(crash=point) == "crashed"
    return Process(root, p.disk).start()


def s3_cold_copy(rule: str, liveness: str = "leader") -> bool:
    """A copy made while stopped starts first and runs 5 refreshes; the original crash-loops 200 times
    beside it. True when the original, older than the copy, is admitted at the copy's first lane drop."""
    root = Root(rule, liveness)
    p = running(root, 2)
    root.died()
    copy, original = Process(root, p.disk), Process(root, p.disk)
    assert copy.start() == "admitted"
    for _ in range(5):
        assert copy.hourly() == "admitted"
    for _ in range(200):
        assert original.start() in ("duplicate", "stale", "refused-hello")
    root.leader_change()  # a root leader change, or the copy's link dropping
    return original.start() == "admitted"


def s5_copy_ran_then_died(rule: str) -> str:
    """A cold copy is admitted and runs (writing state) but dies before any refresh; the original,
    now older than the copy's groups, starts. Its answer."""
    root = Root(rule, "record")
    p = running(root, 2)
    root.died()
    copy, original = Process(root, p.disk), Process(root, p.disk)
    assert copy.start() == "admitted"
    root.died()
    return original.start()


def s4_hot_copy(rule: str, liveness: str, trials: int = 2000) -> float:
    """A hot copy of a running node keeps retrying; at a root leader change both reach the new leader
    in random order. The fraction of trials in which the copy is admitted while its original runs."""
    wins = 0
    for _ in range(trials):
        root = Root(rule, liveness)
        original = running(root, 1)
        copy = Process(root, original.disk)
        assert copy.start() in ("refused-hello", "duplicate")
        root.leader_change()
        answers = {id(proc): proc.retry() for proc in random.sample([original, copy], 2)}
        wins += answers[id(copy)] == "admitted"
    return wins / trials


def s4_many_changes(rule: str, trials: int = 2000, changes: int = 20) -> tuple[int, set[str]]:
    """Liveness "record": a hot copy beside its running original across `changes` root leader changes
    per trial, the original refreshing at random. Returns the trials the copy was admitted in and
    every answer it read."""
    admitted, seen = 0, set()
    for _ in range(trials):
        root = Root(rule, "record")
        original = running(root, 1)
        copy = Process(root, original.disk)
        answers = {copy.start()}
        for _ in range(changes):
            root.leader_change()
            got = {id(proc): proc.retry() for proc in random.sample([original, copy], 2)}
            assert got[id(original)] == "admitted"
            answers.add(got[id(copy)])
            if random.random() < 0.5:
                assert original.hourly() == "admitted"
        admitted += "admitted" in answers
        seen |= answers
    return admitted, seen


W, TICK = 2.0, 0.5
CRASH_POINTS = ("after-key", "after-join-commit", "after-id-save", "after-offer", "after-save", "after-confirm")


class TimedRoot:
    """Liveness "record" with time, rule "above" (or "confirmed"); also the token join keyed by the
    saved key, and the certificate's re-admission of a key a root salvage lost."""

    def __init__(self, rule: str = "above") -> None:
        self.rule = rule
        self.boot, self.heard = 0, float("-inf")
        self.record, self.offered, self.offer_boot, self.next = 0, 0, 0, 1
        self.ids: dict[int, int] = {}  # key pin -> node id, committed by the join
        self.uses = 1  # a one-use token

    def join(self, pin: int) -> int | None:
        if pin in self.ids:
            return self.ids[pin]  # a retry with the saved key: the same node, no use burned
        if self.uses == 0:
            return None
        self.uses -= 1
        self.ids[pin] = len(self.ids) + 1
        return self.ids[pin]

    def hello(self, boot: int, now: float) -> bool:
        return not (self.boot and boot != self.boot and now - self.heard < W)


class Disk:
    def __init__(self) -> None:
        self.pin, self.node, self.number = 0, 0, 0

    def copy(self) -> Disk:
        d = Disk()
        d.pin, d.node, d.number = self.pin, self.node, self.number
        return d


def steps(root: TimedRoot, disk: Disk, boot: int, t: float, crash: str = "") -> str:
    """The admission steps of one boot (a start, or a running boot's refresh)."""
    if disk.pin not in root.ids:
        if root.rule == "confirmed":
            return "unknown"
        root.ids[disk.pin] = disk.node  # the certificate proves the id: re-admitted under it
    if root.rule == "confirmed" and root.offered and disk.number == root.offered:
        pass  # revision 2: presenting the pending offer completes it
    elif root.rule == "confirmed" and disk.number != root.record or disk.number < root.record:
        return "stale"
    else:
        offer = max(root.next, disk.number + 1) if root.rule == "above" else root.next
        root.offered, root.offer_boot, root.next = offer, boot, offer + 1
        if crash == "after-offer":
            return "crashed"
        disk.number = root.offered
        if crash == "after-save":
            return "crashed"
    root.record, root.offered, root.boot, root.heard = disk.number, 0, boot, t
    return "crashed" if crash == "after-confirm" else "admitted"


def run(root: TimedRoot, disk: Disk, start: float, crash: str = "", patience: float = W + TICK) -> tuple[str, float]:
    """One process of the run command with a token: join if needed, then admission. Returns its
    outcome ("admitted", "duplicate", "used", "stale", "unknown" or "crashed") and the time."""
    boot = random.getrandbits(62) + 1
    if not disk.pin:
        disk.pin = random.getrandbits(62) + 1  # the key is saved before the first request
        if crash == "after-key":
            return "crashed", start
    if not disk.node:
        node = root.join(disk.pin)
        if node is None:
            return "used", start
        if crash == "after-join-commit":
            return "crashed", start
        disk.node = node
        if crash == "after-id-save":
            return "crashed", start
    t = start
    while not root.hello(boot, t):
        if t - start >= patience:
            return "duplicate", t
        t += TICK
    return steps(root, disk, boot, t, crash), t


def s6_restart(delay: float, crash: str, patience: float = W + TICK) -> tuple[str, float, int, int]:
    """A node admitted at 0 and heard until 10 dies; the supervisor restarts it after `delay`, that
    process dies at `crash`, and the next one starts after `delay` again. Returns the final outcome,
    the time past the first death, the ids allocated and the uses left."""
    root, disk = TimedRoot(), Disk()
    assert run(root, disk, 0.0)[0] == "admitted"
    root.heard = 10.0
    outcome, t = run(root, disk, 10.0 + delay, crash, patience)
    if outcome == "crashed":
        outcome, t = run(root, disk, t + delay)
    return outcome, t - 10.0, len(root.ids), root.uses


def s7_first_start(crash: str) -> tuple[str, int, int]:
    """An empty directory with a one-use token, killed at `crash`, restarted at once."""
    root, disk = TimedRoot(), Disk()
    outcome, t = run(root, disk, 0.0, crash)
    if outcome == "crashed":
        outcome, t = run(root, disk, t)
    return outcome, len(root.ids), root.uses


def s8_hot_copy(start: float) -> tuple[str, float]:
    """A copy of a running node's directory (identity and number) started at `start`, while the
    original keeps its keepalives. Its outcome and how long it took from its first hello."""
    root, disk = TimedRoot(), Disk()
    assert run(root, disk, 0.0)[0] == "admitted"
    root.heard = start + W + TICK  # the original is heard throughout the copy's retries
    outcome, t = run(root, disk.copy(), start)
    return outcome, t - start


class History:
    """One identity's directories under one root: per directory its number, its divergence time from
    each other directory, and the time of its last confirm."""

    def __init__(self, rule: str) -> None:
        self.root = Root(rule, "record")
        self.n, self.div, self.last = [0], [{}], [-1]
        self.holder: list[int] = []  # the running directory and its boot
        self.salvaged = False
        self.violations = 0

    def steps(self, i: int, boot: int, crash: str, clock: int) -> bool:
        answer = self.root.admit(boot, self.n[i])
        if answer == "offer":
            if crash == "before-save":
                return False
            self.n[i] = self.root.offered
            if crash == "before-confirm":
                return False
            answer = self.root.admit(boot, self.n[i])
        if answer != "admitted":
            return False
        rivals = (j for j in range(len(self.n)) if j != i)
        if not self.salvaged and any(self.last[j] > self.div[i].get(j, -1) for j in rivals):
            self.violations += 1
        self.last[i] = clock
        return True

    def copy(self, s: int, clock: int) -> None:
        c = len(self.n)
        self.n.append(self.n[s])
        self.div.append(dict(self.div[s]))
        self.last.append(-1)
        for j in range(c):
            self.div[j][c] = self.div[j].get(s, -1) if j != s else clock
        self.div[c][s] = clock


def explore(rule: str, salvage: bool, trials: int = 4000, length: int = 40) -> tuple[int, int]:
    """Random histories: starts killed at any step, refreshes killed at any step, kills, cold and hot
    copies (a backup is a copy started later), and with `salvage` root salvages back to any root state
    since the last one. A violation admits a directory after another directory confirmed since the two
    diverged (Limit 2's copy since the last confirm is allowed); a salvage accepts loss, so none count
    after one. A lockout: at the end, the last directory to confirm, restarted alone, is refused.
    Returns (violations, lockouts)."""
    violations = lockouts = 0
    for _ in range(trials):
        h = History(rule)
        snaps = [vars(h.root).copy()]
        for clock in range(length):
            event = random.random()
            crash = random.choice(("", "", "before-save", "before-confirm"))
            if clock == 0 or event < 0.3:
                i, boot = random.randrange(len(h.n)), random.getrandbits(62) + 1
                if not h.holder and h.steps(i, boot, crash, clock):
                    h.holder = [i, boot]
            elif event < 0.5:
                if h.holder and not h.steps(h.holder[0], h.holder[1], crash, clock):
                    h.holder = []  # killed mid-refresh, or refused
            elif event < 0.65:
                h.holder = []
            elif event < 0.9 or not salvage:
                h.copy(random.randrange(len(h.n)), clock)
            else:
                vars(h.root).update(random.choice(snaps))
                snaps, h.salvaged = [vars(h.root).copy()], True
            snaps.append(vars(h.root).copy())
        i = max(range(len(h.n)), key=lambda j: h.last[j])
        lockouts += not h.steps(i, random.getrandbits(62) + 1, "", length)
        violations += h.violations
    return violations, lockouts


def s10_salvage(rule: str) -> dict[str, str]:
    """A root salvage keeps a survivor that lacks Y's last two refreshes and Z's join. Y restarts, Y
    running refreshes, Z restarts; Y's backups from before the survivor's record and from the lost
    window start while Y is down."""
    out = {}
    for case in ("restart", "running", "backup", "lost-window backup"):
        root, y = TimedRoot(rule), Disk()
        assert run(root, y, 0.0)[0] == "admitted"
        old = y.copy()
        assert steps(root, y, root.boot, 1.0) == "admitted"
        boot, survivor = root.boot, copy.deepcopy(vars(root))
        assert steps(root, y, boot, 2.0) == "admitted"
        window = y.copy()
        assert steps(root, y, boot, 3.0) == "admitted"
        vars(root).update(survivor)
        root.heard = float("-inf")  # the salvage's survivor heard no boot since
        if case == "running":
            out[case] = steps(root, y, boot, 100.0)
        else:
            d = {"restart": y, "backup": old, "lost-window backup": window}[case]
            out[case] = run(root, d, 100.0)[0]
    root, z = TimedRoot(rule), Disk()
    survivor = copy.deepcopy(vars(root))
    assert run(root, z, 0.0)[0] == "admitted"
    node = z.node
    vars(root).update(survivor)
    out["join lost"] = run(root, z, 100.0)[0]
    out["same id, no use"] = str(root.ids.get(z.pin) == node == z.node and root.uses == 1)
    return out


def s11_cold_start(rule: str, voters: int, samples: int = 0) -> tuple[int, int]:
    """Every root voter starts while the root has no leader. A voter's disk holds its record or a
    committed pending offer. Each voter's copy of each record is current, ahead (an uncommitted offer
    or confirm) or lagging (it lacks the last refresh); a majority of copies hold every commit (log
    matching). An acceptor answers a voter's number against its copy: `stale` below the record (the
    voter exits), accepted at the record or the pending offer, otherwise `wait` ("confirmed": `stale`).
    A pair links if either side accepts the other. A leader needs links to a majority from a copy
    holding every commit; lagging voters then dial it, catch up, and every voter is admitted.
    Enumerates every state, or `samples` random ones. Returns (states, states admitting every voter)."""
    major = voters // 2 + 1
    kinds = list(itertools.product(("record", "pending"), repeat=voters))

    def outcome(kind: tuple[str, ...], copies: list[list[str]]) -> bool:
        disk = [10 * v + (3 if kind[v] == "pending" else 2) for v in range(voters)]

        def answer(u: int, v: int) -> str:
            state = copies[u][v]
            r, p = 10 * v + 2, 10 * v + 3 if kind[v] == "pending" else 0
            if state == "lag":
                r, p = 10 * v + 1, 0
            elif state == "ahead":
                r, p = (p, 0) if p else (r, 10 * v + 4)  # an uncommitted confirm, or offer
            n = disk[v]
            if n in (r, p):
                return "ok"
            return "wait" if rule == "above" and n > r else "stale"

        answers = {(u, v): answer(u, v) for u in range(voters) for v in range(voters) if u != v}
        if "stale" in answers.values():
            return False
        linked = [[u == v or "ok" in (answers.get((u, v)), answers.get((v, u))) for v in range(voters)] for u in range(voters)]
        current = [u for u in range(voters) if "lag" not in copies[u]]
        leaders = [c for c in current if sum(linked[c]) >= major]
        return bool(leaders) and all(answers.get((leaders[0], u), "ok") == "ok" for u in range(voters))

    def states() -> object:
        if samples:
            for _ in range(samples):
                lagging = set(random.sample(range(voters), random.randint(0, voters - major)))
                yield random.choice(kinds), [
                    [random.choice(("current", "ahead", "lag") if u in lagging else ("current", "ahead")) for _ in range(voters)]
                    for u in range(voters)]
            return
        for kind in kinds:
            for flat in itertools.product(("current", "ahead", "lag"), repeat=voters * voters):
                copies = [list(flat[u * voters:(u + 1) * voters]) for u in range(voters)]
                if sum("lag" not in row for row in copies) >= major:
                    yield kind, copies

    total = good = 0
    for kind, copies in states():
        total += 1
        good += outcome(kind, copies)
    return total, good


def s12_leader_changes(dial: float, trials: int = 2000, changes: int = 20) -> tuple[int, float]:
    """The root leader's host dies; a leader is elected 1 to 2 s later (consensus's 1,000 ms election
    timeout, randomized). A running original keeps its 500 ms keepalive tick: each missed answer asks
    a root replica for the leader (answer within 500 ms) and dials it (within `dial`). A hot copy says
    hello every TICK. The new leader refuses the copy until 2 s past its election unless it heard the
    original. Returns the leader changes the copy was admitted in and the original's worst arrival
    after an election."""
    admitted, worst = 0, 0.0
    for _ in range(trials):
        phase_o, phase_c = random.uniform(0, TICK), random.uniform(0, TICK)
        for _ in range(changes):
            elected = random.uniform(1.0, 2.0)
            tick = phase_o + TICK * math.ceil((elected - phase_o) / TICK)
            arrival = tick + random.uniform(0, 0.5) + random.uniform(0, dial)
            worst = max(worst, arrival - elected)
            hello = phase_c + TICK * math.ceil((elected - phase_c) / TICK)
            while hello < arrival:
                if hello - elected >= W:
                    admitted += 1
                    break
                hello += TICK
    return admitted, worst


def main() -> int:
    random.seed(20261120)
    ok = True

    def check(name: str, got: object, want: object) -> None:
        nonlocal ok
        ok &= got == want
        print(f"{'ok  ' if got == want else 'FAIL'} {name}: {got!r} (want {want!r})")

    n = s1_restored_backup("design")
    check("S1 design: restarts until a backup 30 refreshes old is admitted", n, 31)
    check("S1 offered: admitted within 1000 restarts", s1_restored_backup("offered"), None)
    check("S2 design: a crash before the keepalive locks the node out", not s2_crash_between("design"), False)
    check("S2 offered: a crash between offer and persist locks it out", not s2_crash_between("offered"), False)
    check("S3 design: the older original admitted after the copy ran", s3_cold_copy("design"), True)
    check("S3 offered: the older original admitted", s3_cold_copy("offered"), False)
    for rule in ("design", "offered"):
        f = s4_hot_copy(rule, "leader")
        print(f"     S4 {rule}/leader: copy admitted beside its running original in {f:.1%} of leader changes")
        check(f"S4 {rule}/leader: copy admitted in some leader changes", f > 0.3, True)
    check("S4 offered/record: copy admitted in any leader change", s4_hot_copy("offered", "record"), 0.0)

    check("S5 offered: a copy that ran and died lets the older original in", s5_copy_ran_then_died("offered"), "admitted")
    for rule in STEPPED:
        print(f"     revision {1 if rule == 'confirmed' else 3}: rule {rule}, liveness record")
        check(f"S5 {rule}: the older original after the copy ran and died", s5_copy_ran_then_died(rule), "stale")
        check(f"S1 {rule}: answers over 1000 restarts of a restored backup", s1_answers(rule, "record"), {"stale"})
        for refresh in (False, True):
            for point in ("before-save", "before-confirm"):
                where = "refresh" if refresh else "start"
                check(f"S2 {rule}: the next boot after a kill {point} at {where}", s2_crash_point(rule, point, refresh), "admitted")
        check(f"S3 {rule}: the older original admitted", s3_cold_copy(rule, "record"), False)
        f = s4_hot_copy(rule, "leader")
        print(f"     S4 {rule}/leader: copy admitted in {f:.1%} of leader changes (the number alone)")
        check(f"S4 {rule}/leader: copy admitted in some leader changes", f > 0.3, True)
        admitted, seen = s4_many_changes(rule)
        check(f"S4 {rule}/record: trials of 20 leader changes the copy was admitted in", admitted, 0)
        check(f"S4 {rule}/record: what the copy reads", seen, {"duplicate"})

    print("     admission inside the run command (D157), rule above, with time")
    worst, results = 0.0, set()
    for step in range(25):
        delay = step * 0.25
        for crash in ("",) + CRASH_POINTS[3:]:
            outcome, took, ids, uses = s6_restart(delay, crash)
            results.add((outcome, ids, uses))
            if not crash:
                worst = max(worst, took - max(delay, W))
    check("S6: restarts after 0 to 6 s, killed at each admission step, end (outcome, ids, uses left)",
          results, {("admitted", 1, 0)})
    check("S6: uncrashed, admitted within one TICK of the later of the restart and the window", worst <= TICK, True)
    check("S6 without the retry: an immediate restart", s6_restart(0.0, "", patience=0.0)[0], "duplicate")
    for crash in CRASH_POINTS:
        check(f"S7: an empty directory with a one-use token killed {crash}, then restarted",
              s7_first_start(crash), ("admitted", 1, 0))
    outcome, took = s8_hot_copy(5.0)
    check("S8: a hot copy beside its running original", outcome, "duplicate")
    check("S8: the copy exits within 3 s of its first hello", took <= 3.0, True)

    print("     S9: 4,000 random histories of 40 events per rule (violations, lockouts)")
    v2, l2 = explore("confirmed", False)
    print(f"     S9 confirmed: {v2} violations, {l2} lockouts")
    check("S9 confirmed: some directory admitted after a sibling confirmed", v2 > 0, True)
    check("S9 above: violations and lockouts", explore("above", False), (0, 0))
    v2, l2 = explore("confirmed", True)
    print(f"     S9 confirmed with salvages: {l2} lockouts")
    check("S9 confirmed with salvages: the last directory to confirm locked out in some histories", l2 > 0, True)
    check("S9 above with salvages: violations before any salvage, lockouts", explore("above", True), (0, 0))

    check("S10 confirmed: after a root salvage (F17)", s10_salvage("confirmed"),
          {"restart": "stale", "running": "stale", "backup": "stale", "lost-window backup": "stale",
           "join lost": "unknown", "same id, no use": "False"})
    check("S10 above: after a root salvage", s10_salvage("above"),
          {"restart": "admitted", "running": "admitted", "backup": "stale", "lost-window backup": "admitted",
           "join lost": "admitted", "same id, no use": "True"})

    total, good = s11_cold_start("confirmed", 3)
    print(f"     S11 confirmed: 3 voters, {total - good} of {total} cold-start states exit a voter")
    check("S11 confirmed: some cold starts exit a voter `stale`", good < total, True)
    total, good = s11_cold_start("above", 3)
    check(f"S11 above: 3 voters, every one of {total} states admits every voter", good, total)
    total, good = s11_cold_start("above", 5, samples=20000)
    check(f"S11 above: 5 voters, every one of {total} sampled states admits every voter", good, total)

    admitted, worst = s12_leader_changes(dial=0.5)
    print(f"     S12: the original reaches a new leader within {worst:.2f} s of its election (bound 1.5 s)")
    check("S12: leader changes of 40,000 the copy was admitted in, 500 ms dial", admitted, 0)
    admitted, worst = s12_leader_changes(dial=1.5)
    print(f"     S12 mutant: a 1.5 s dial, the copy admitted in {admitted} of 40,000 (worst {worst:.2f} s)")
    check("S12 mutant: a 1.5 s dial admits the copy in some", admitted > 0, True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
