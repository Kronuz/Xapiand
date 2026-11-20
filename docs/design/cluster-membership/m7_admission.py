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
Liveness:
  leader     a new boot is `duplicate` only beside a boot this leader heard (revision 0)
  record     the hello carries the boot (consensus addition 7); while the recorded boot is not
             silent past the window, every acceptor refuses another boot's hello with `duplicate`;
             a new root leader counts the window from its election

Exits 0 when every check holds; prints one line per check. The first block reproduces the review's
lines (job 261120-121643-c02f08) with the same seed; revision 1's checks follow. Revision 2 (D157)
runs admission inside the run command, `xapiand [--token TOKEN]`: the process that joins and saves
the number is the one that runs the node, and a supervisor restarts it at once. Its checks use time:
the admitted boot is live for W seconds after it was last heard, and a starting process retries a
`duplicate` hello every TICK for W + TICK before it exits, since the live boot may be its own dead
predecessor.
"""

from __future__ import annotations

import random
import sys
from dataclasses import dataclass

RESTART_CAP = 1000


@dataclass
class Root:
    rule: str
    liveness: str
    record: int = 0  # committed number
    offered: int = 0  # committed pending offer (rules "offered" and "confirmed"; 0 is none)
    next: int = 1  # rule "confirmed": the next number to offer
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
        """Rule "confirmed": one step of a boot's admission or refresh."""
        if self.offered and n == self.offered:
            self.record, self.offered = n, 0
            self.boot, self.live, self.silent = boot, boot, False
            return "admitted"
        if n == self.record:
            self.offered, self.next = self.next, self.next + 1
            return "offer"
        return "stale"

    def keepalive(self, boot: int, n: int, refresh: bool = False) -> str:
        if boot == self.boot and not self.silent:
            self.live = boot
            if refresh and self.rule == "confirmed":
                return self.admit(boot, n)
            if refresh and self.number_ok(n):
                self.commit(boot, n)
            return "admitted"
        if self.live and self.live != boot:
            return "duplicate"
        if self.rule == "confirmed":
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


def s2_crash_point(point: str, refresh: bool) -> str:
    """Rule "confirmed": a start (or a refresh) killed at `point`; the next boot's answer."""
    root = Root("confirmed", "record")
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


def s4_many_changes(trials: int = 2000, changes: int = 20) -> tuple[int, set[str]]:
    """Revision 1 (confirmed, record): a hot copy beside its running original across `changes` root
    leader changes per trial, the original refreshing at random. Returns the trials the copy was
    admitted in and every answer it read."""
    admitted, seen = 0, set()
    for _ in range(trials):
        root = Root("confirmed", "record")
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
    """Rule "confirmed", liveness "record", with time; also the token join keyed by the saved key."""

    def __init__(self) -> None:
        self.boot, self.heard = 0, float("-inf")
        self.record, self.offered, self.next = 0, 0, 1
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


def run(root: TimedRoot, disk: Disk, start: float, crash: str = "", patience: float = W + TICK) -> tuple[str, float]:
    """One process of the run command with a token: join if needed, then admission. Returns its
    outcome ("admitted", "duplicate", "used" or "crashed") and the time."""
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
    if disk.number == root.offered and root.offered:
        pass  # its predecessor crashed after the save: presenting the offer completes it
    elif disk.number == root.record:
        root.offered, root.next = root.next, root.next + 1
        if crash == "after-offer":
            return "crashed", t
        disk.number = root.offered
        if crash == "after-save":
            return "crashed", t
    else:
        return "stale", t
    root.record, root.offered, root.boot, root.heard = disk.number, 0, boot, t
    if crash == "after-confirm":
        return "crashed", t
    return "admitted", t


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
    original keeps its keepalives. Its outcome and how long it took."""
    root, disk = TimedRoot(), Disk()
    assert run(root, disk, 0.0)[0] == "admitted"
    copy = Disk()
    copy.pin, copy.node, copy.number = disk.pin, disk.node, disk.number
    root.heard = start + W + TICK  # the original is heard throughout the copy's retries
    outcome, t = run(root, copy, start)
    return outcome, t - start


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

    print("     revision 1: rule confirmed, liveness record")
    check("S5 offered: a copy that ran and died lets the older original in", s5_copy_ran_then_died("offered"), "admitted")
    check("S5 confirmed: the older original after the copy ran and died", s5_copy_ran_then_died("confirmed"), "stale")
    check("S1 confirmed: answers over 1000 restarts of a restored backup", s1_answers("confirmed", "record"), {"stale"})
    for refresh in (False, True):
        for point in ("before-save", "before-confirm"):
            where = "refresh" if refresh else "start"
            check(f"S2 confirmed: the next boot after a kill {point} at {where}", s2_crash_point(point, refresh), "admitted")
    check("S3 confirmed: the older original admitted", s3_cold_copy("confirmed", "record"), False)
    f = s4_hot_copy("confirmed", "leader")
    print(f"     S4 confirmed/leader: copy admitted in {f:.1%} of leader changes (the number alone)")
    check("S4 confirmed/leader: copy admitted in some leader changes", f > 0.3, True)
    admitted, seen = s4_many_changes()
    check("S4 confirmed/record: trials of 20 leader changes the copy was admitted in", admitted, 0)
    check("S4 confirmed/record: what the copy reads", seen, {"duplicate"})

    print("     revision 2: admission inside the run command (D157), with time")
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
    check("S8: the copy exits within 3 s", took <= 3.0, True)
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
