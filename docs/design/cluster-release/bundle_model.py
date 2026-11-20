#!/usr/bin/env python3
"""Crash model: a lone voter's checkpoint bundle with an application file set (cluster-release D16).

From the group-core design review's model (`.campaign/xapiand/reviews/group-core-design.md`, B1-B3,
B6), extended to the revised D16. Every log entry is durable when appended (a lone voter commits at
its own barrier), so recovery has every entry above the latest published bundle. The model crashes
after every atomic step of each script, under power loss (unsynced bytes and directory entries are
lost) and process crash (nothing written is lost), recovers, replays to the log's end, and compares
the logical digest (documents with versions, resolved through their blob locators; the live files;
the volume counter, the application state) with an uncrashed run of the same log, which also
stands for another replica: its retries repeat only strata's write, as a replica that never hit
`no_space` would. Reads open once applied reaches strata's
head P_H; at that point every document strata holds must resolve as in an uncrashed run through P_H.

Rules of `fixed` (D16): the bundle is captured on the apply chore between two entries (position P_b,
file lengths, the application state section); files and the directory are synced and strata commits
before the bundle publishes; a retired file is unlinked only after a bundle that omits it publishes;
file effects run whatever strata answers, before strata's write, and the application state updates
after it whatever it answered; every file effect writes at the volume and offset the entry's
starting application state names, so a failed write (ENOSPC) stalls and rewrites the same bytes;
a failed checkpoint sync fences the replica, which reopens through recovery; a retryable answer (`no_space`) leaves strata's position and is
retried with strata's write alone; the capture moves strata's position to P_b; recovery opens
strata at its head P_H, fails closed if P_b > P_H, truncates and deletes to the bundle, and replays
from P_b while strata skips at or below P_H.

Variants that must fail somewhere (each breaks one rule):
  a1-literal          replay the application from P_H instead of P_b
  eager-delete        unlink a file when its command applies
  counter-from-dir    the volume counter from the directory, not the bundle's application state
  publish-first       the bundle publishes before the files are synced
  commit-after-publish strata commits after the bundle publishes (recovery fails closed: P_b > P_H)
  capture-mid         the capture is taken inside an entry's apply, after its file write
  verdict-gated       file effects run only when strata accepts or skips the write
  read-early          reads open at P_b instead of P_H
  no-mark             the capture leaves strata's position behind P_b after entries strata never saw
  nospace-moves       a no_space moves strata's position, so the retry is skipped (strata 491a1bdc, B7)
  append-on-retry     a failed blob push is retried by appending at the file's end, not at its offset
  sync-retry          a failed checkpoint sync is retried and the bundle publishes, instead of fencing
  appstate-gated      the application state updates only when strata does not answer `skipped`
`fixed`, `sync-late` (files synced after strata's commit, still before publication) and
`retry-whole` (a retry re-runs the whole entry: positional file effects rewrite the same bytes) must
fail nowhere, with a second crash inside recovery (before replay, and halfway through it).
Exit status 0 when every variant behaves as stated.
"""
from __future__ import annotations

import argparse
import copy
import sys

SCRIPTS = {
    # The window: a checkpoint's strata commit is durable and its bundle is not.
    "window": [("put", "a", b"AAAA"), ("ckpt",), ("put", "b", b"BBBB"), ("rotate",), ("put", "c", b"CCCC"),
               ("ckpt",), ("put", "d", b"DDDD")],
    # A checkpoint whose capture is rejected or cancelled after strata committed: the head stays ahead.
    "cancel": [("put", "a", b"AAAA"), ("ckpt",), ("put", "b", b"BBBB"), ("ckpt-cancel",), ("put", "c", b"CCCC")],
    # A strata commit outside the group (the catalog's default commit hook), between checkpoints.
    "outside": [("put", "a", b"AAAA"), ("ckpt",), ("put", "b", b"BBBB"), ("scommit",), ("put", "c", b"CCCC"),
                ("scommit",), ("ckpt",), ("put", "d", b"DDDD")],
    # A staging file the bundle names, consumed by a final entry above the bundle (D12).
    "staging": [("chunk", "x", 0, b"XX"), ("chunk", "x", 2, b"YY"), ("ckpt",), ("final", "x", "doc"),
                ("put", "e", b"EEEE"), ("ckpt",), ("put", "f", b"FFFF")],
    # An aborted upload's volume: allocated, written, retired, then a later volume allocated.
    "counter": [("put", "a", b"AAAA"), ("upload", "u"), ("uchunk", "u", 0, b"UUUU"), ("uabort", "u"),
                ("ckpt",), ("rotate",), ("put", "g", b"GGGG"), ("ckpt",), ("put", "h", b"HHHH")],
    # An upload open across a checkpoint, finished above it.
    "upload": [("upload", "v"), ("uchunk", "v", 0, b"V1"), ("ckpt",), ("uchunk", "v", 2, b"V2"),
               ("ufinal", "v", "big"), ("put", "i", b"IIII"), ("ckpt",)],
    # A write strata refuses (expected version 0, the id exists) between a checkpoint and a strata commit.
    "refused": [("put", "a", b"AAAA"), ("ckpt",), ("put", "a", b"RRRR", 0), ("put", "b", b"BBBB"), ("scommit",),
                ("put", "c", b"CCCC")],
    # A write whose first strata attempt answers no_space on this replica, retried once space frees.
    "nospace": [("put", "a", b"AAAA"), ("ckpt",), ("put", "b", b"BBBB", None, "nospace"), ("scommit",),
                ("put", "c", b"CCCC", None, "nospace"), ("ckpt",), ("put", "d", b"DDDD")],
    # A checkpoint right after entries strata never saw (chunks), for the capture's mark.
    "mark": [("put", "a", b"AAAA"), ("ckpt",), ("chunk", "y", 0, b"ZZ"), ("ckpt",), ("chunk", "y", 2, b"WW"),
             ("final", "y", "doc2")],
    # A blob push that fails partway (ENOSPC before strata answers), stalls, and is retried.
    "eio": [("put", "a", b"AAAA"), ("ckpt",), ("put", "b", b"BBBB", None, "eio"), ("scommit",),
            ("put", "c", b"CCCC"), ("ckpt",), ("put", "d", b"DDDD")],
    # A checkpoint whose file sync fails (step 2): the replica fences and reopens, then goes on.
    "syncfail": [("put", "a", b"AAAA"), ("ckpt",), ("put", "b", b"BBBB"), ("failsync",), ("ckpt",),
                 ("put", "c", b"CCCC"), ("ckpt",), ("put", "d", b"DDDD")],
}

PASS = ("fixed", "sync-late", "retry-whole")
FAIL = ("a1-literal", "eager-delete", "counter-from-dir", "publish-first", "commit-after-publish", "capture-mid",
        "verdict-gated", "read-early", "no-mark", "nospace-moves", "appstate-gated", "append-on-retry",
        "sync-retry")
SEMANTIC = ("eager-delete", "verdict-gated")  # variants whose uncrashed run differs from fixed's


class Disk:
    """Application files with page-cache and durable images, and a directory with both."""

    def __init__(self) -> None:
        self.data: dict[str, bytearray] = {}
        self.synced: dict[str, bytes] = {}
        self.durable: dict[str, bytes] = {}  # durable directory entries and their durable bytes
        self.failed = {}

    def create(self, name: str) -> None:
        self.data.setdefault(name, bytearray())
        self.synced.setdefault(name, b"")

    failed: dict[str, bytes]

    def fsync_fails(self, name: str) -> None:
        # The kernel reports the error and marks the pages clean: a later fsync of the same bytes
        # succeeds without making them durable, until a write dirties them again.
        self.failed[name] = bytes(self.data[name])

    def write(self, name: str, off: int, b: bytes) -> None:
        self.failed.pop(name, None)
        f = self.data[name]
        if len(f) < off:
            f.extend(b"\0" * (off - len(f)))
        f[off:off + len(b)] = b

    def fsync(self, name: str) -> None:
        if self.failed.get(name) == bytes(self.data[name]):
            return
        self.synced[name] = bytes(self.data[name])
        if name in self.durable:
            self.durable[name] = self.synced[name]

    def sync_dir(self) -> None:
        self.durable = {n: self.synced[n] for n in self.data}

    def unlink(self, name: str) -> None:
        self.data.pop(name, None)
        self.synced.pop(name, None)

    def truncate(self, name: str, n: int) -> None:
        self.failed.pop(name, None)
        del self.data[name][n:]

    def crash(self, power: bool) -> Disk:
        d = Disk()
        src = self.durable if power else self.data
        d.data = {n: bytearray(b) for n, b in src.items()}
        d.synced = {n: bytes(b) for n, b in d.data.items()}
        d.durable = dict(d.synced)
        return d


class Failed(Exception):
    pass


class Fenced(Exception):
    pass


class Node:
    def __init__(self, variant: str) -> None:
        self.variant = variant
        self.disk = Disk()
        self.log: list[tuple] = []  # entry i at index i + 1, durable at append
        self.bundle: dict | None = None  # the latest published bundle (durable)
        self.head: tuple[int, dict, dict] = (0, {}, {})  # strata's durable head: position, documents, versions
        self.docs: dict[str, tuple] = {}
        self.versions: dict[str, int] = {}
        self.strata_applied = 0
        self.applied = 0  # the group's applied position: files and strata complete through it
        self.counter = 0
        self.cur: str | None = None
        self.uploads: dict[str, str] = {}
        self.retired: set[str] = set()
        self.mid = False  # capture-mid: a checkpoint waits for the next put's file write
        self.nputs = 0  # application state: puts applied, which rotates the volume every third
        self.spaced: set[int] = set()  # entries whose first strata attempt answered no_space
        self.eio: set[int] = set()  # entries whose first blob push failed partway
        self.curlen = 0  # application state: the current volume's length
        self.failsync = False  # the next checkpoint's first file sync fails

    # --- strata ------------------------------------------------------------------------------
    def strata_verdict(self, pos: int, key: str, expected: int | None) -> str:
        if pos <= self.strata_applied:
            return "skipped"  # index.hh:803-806: positions at or below the applied one are skipped
        if expected is not None and self.versions.get(key, 0) != expected:
            return "refused"  # version_conflict: a committed no-op on every replica
        return "accepted"

    def strata_apply(self, pos: int, key: str, value: tuple, expected: int | None = None) -> str:
        v = self.strata_verdict(pos, key, expected)
        if v == "accepted":
            self.docs[key] = value
            self.versions[key] = self.versions.get(key, 0) + 1
        if v != "skipped":
            self.strata_applied = pos
        return v

    def strata_commit(self) -> None:
        self.head = (self.strata_applied, copy.deepcopy(self.docs), dict(self.versions))

    # --- apply, as the application does it, one atomic step per yield -----------------------
    def remove(self, name: str):
        if self.variant == "eager-delete":
            self.disk.unlink(name)
            yield
        else:
            self.retired.add(name)

    def allocate(self) -> str:
        name = f"vol-{self.counter}"
        self.counter += 1
        self.disk.create(name)
        return name

    def apply(self, pos: int, op: tuple):
        kind = op[0]
        if kind == "put":
            expected = op[3] if len(op) > 3 else None
            if self.variant == "verdict-gated" and self.strata_verdict(pos, op[1], expected) == "refused":
                self.strata_apply(pos, op[1], (), expected)
            else:
                if self.cur is None:
                    self.cur, self.curlen = self.allocate(), 0
                off = self.curlen  # positional: from the entry's starting application state (B10)
                if "eio" in op[4:] and pos not in self.eio:
                    self.eio.add(pos)
                    self.disk.write(self.cur, off, op[2][:len(op[2]) // 2])
                    yield  # ENOSPC partway: the replica stalls, then retries
                    if self.variant == "append-on-retry":
                        off = len(self.disk.data[self.cur])
                self.disk.write(self.cur, off, op[2])
                yield
                value = ("blob", self.cur, off, len(op[2]))
                if "nospace" in op[4:] and pos not in self.spaced:
                    # A retryable refusal (no_space, an owed verdict, a stage-1 throw): the position
                    # stays unless the variant reproduces strata 491a1bdc's no_space path (B7).
                    self.spaced.add(pos)
                    if self.variant == "nospace-moves":
                        self.strata_applied = pos
                    yield
                    if self.variant == "retry-whole":
                        yield from self.apply(pos, op)
                        return
                v = self.strata_apply(pos, op[1], value, expected)
                # The application state after the strata write (D18), whatever strata answered (N13).
                if self.variant != "appstate-gated" or v != "skipped":
                    self.nputs += 1
                    self.curlen = off + len(op[2])
                    if self.nputs % 3 == 0:
                        self.cur = None  # rotation at an entry the log decides (D12)
                if self.mid:
                    self.mid = False
                    yield from self.checkpoint()
        elif kind == "rotate":
            self.cur = None
        elif kind == "upload":
            self.uploads[op[1]] = self.allocate()
        elif kind == "uchunk":
            self.disk.write(self.uploads[op[1]], op[2], op[3])
            yield
        elif kind == "ufinal":
            name = self.uploads.pop(op[1])
            self.strata_apply(pos, op[2], ("blob", name, 0, len(self.disk.data[name])))
        elif kind == "uabort":
            yield from self.remove(self.uploads.pop(op[1]))
        elif kind == "chunk":
            name = f"stage-{op[1]}"
            self.disk.create(name)
            self.disk.write(name, op[2], op[3])
            yield
        elif kind == "final":
            name = f"stage-{op[1]}"
            if name not in self.disk.data:
                raise Failed(f"replay of final at {pos}: {name} is gone")
            self.strata_apply(pos, op[2], ("inline", bytes(self.disk.data[name])))
            yield from self.remove(name)
        self.applied = pos
        yield

    # --- the checkpoint -------------------------------------------------------------------
    def sync_files(self, live: list[str]):
        for n in live:
            if self.failsync:
                self.failsync = False
                self.disk.fsync_fails(n)
                yield
                if self.variant != "sync-retry":
                    raise Fenced(n)
            self.disk.fsync(n)
            yield
        self.disk.sync_dir()
        yield

    def checkpoint(self, publish: bool = True):
        live = [n for n in self.disk.data if n not in self.retired]
        # Entries strata never saw (chunks, NoOps) leave its position behind; the capture applies an
        # empty transaction at P_b so strata's head reaches it (a rejected one moves it too, index.hh:877-880).
        if self.variant != "no-mark":
            self.strata_applied = max(self.strata_applied, self.applied)
        bundle = {"pos": self.applied, "fs": {n: len(self.disk.data[n]) for n in live},
                  "app": {"counter": self.counter, "cur": self.cur, "uploads": dict(self.uploads),
                          "nputs": self.nputs, "curlen": self.curlen}}
        yield
        if self.variant not in ("publish-first", "sync-late"):
            yield from self.sync_files(live)
        if self.variant != "commit-after-publish":
            self.strata_commit()  # the head names everything strata applied, possibly past the bundle
            yield
        if self.variant == "sync-late":
            yield from self.sync_files(live)
        if not publish:
            return
        self.bundle = bundle  # the consensus checkpoint record's barrier
        yield
        if self.variant == "commit-after-publish":
            self.strata_commit()
            yield
        if self.variant == "publish-first":
            yield from self.sync_files(live)
        for n in sorted(self.retired):
            if n not in bundle["fs"]:
                self.disk.unlink(n)
                self.retired.discard(n)
                yield
        self.disk.sync_dir()
        yield

    def steps(self, script: list[tuple]):
        for action in script:
            if action[0] == "ckpt" and self.variant == "capture-mid":
                self.mid = True
            elif action[0] == "ckpt":
                try:
                    yield from self.checkpoint()
                except Fenced:
                    self.recover(power=False)  # the replica fences and reopens: recovery, no reboot
                    yield
            elif action[0] == "failsync":
                self.failsync = True
            elif action[0] == "ckpt-cancel":
                yield from self.checkpoint(publish=False)
            elif action[0] == "scommit":
                self.strata_commit()
                yield
            else:
                self.log.append(action)  # durable at append
                yield
                try:
                    yield from self.apply(len(self.log), action)
                except Fenced:  # capture-mid's checkpoint runs inside apply
                    self.recover(power=False)
                    yield

    # --- recovery ---------------------------------------------------------------------------
    def recover(self, power: bool, again: str = "") -> None:
        self.disk = self.disk.crash(power)
        p_h, docs, versions = self.head
        self.docs, self.versions = copy.deepcopy(docs), dict(versions)
        self.strata_applied = p_h
        b = self.bundle or {"pos": 0, "fs": {}, "app": {"counter": 0, "cur": None, "uploads": {}, "nputs": 0, "curlen": 0}}
        if b["pos"] > p_h:
            raise Failed(f"fail closed: P_b {b['pos']} > P_H {p_h}")
        for name, length in b["fs"].items():
            if name not in self.disk.data:
                raise Failed(f"named file {name} is gone")
            if len(self.disk.data[name]) < length:
                raise Failed(f"named file {name} is short: {len(self.disk.data[name])} < {length}")
            self.disk.truncate(name, length)
        for name in list(self.disk.data):
            if name not in b["fs"]:
                self.disk.unlink(name)
        self.disk.sync_dir()
        if again == "before-replay":
            return self.recover(power)
        self.retired, self.mid = set(), False
        app = b["app"]
        self.counter, self.cur, self.uploads = app["counter"], app["cur"], dict(app["uploads"])
        self.nputs, self.curlen, self.spaced, self.eio = app["nputs"], app["curlen"], set(), set()
        if self.variant == "counter-from-dir":
            vols = [int(n.split("-")[1]) for n in self.disk.data if n.startswith("vol-")]
            self.counter = max(vols) + 1 if vols else 0
        start = p_h if self.variant == "a1-literal" else b["pos"]
        gate = start if self.variant == "read-early" else max(p_h, start)
        self.applied = start
        for i in range(start, len(self.log)):
            if again == "mid-replay" and i >= (start + len(self.log)) // 2:
                return self.recover(power)
            if self.applied == gate:
                self.check_reads(p_h)
            for _ in self.apply(i + 1, self.log[i]):
                pass
        if self.applied == gate:
            self.check_reads(p_h)

    def resolve(self, v: tuple) -> bytes | str:
        if v[0] == "inline":
            return v[1]
        _, name, off, n = v
        f = self.disk.data.get(name)
        if f is None or len(f) < off + n:
            return f"MISSING {name}[{off}:{off + n}]"
        return bytes(f[off:off + n])

    def reads(self) -> tuple:
        return tuple(sorted((k, self.versions.get(k, 0), self.resolve(v)) for k, v in self.docs.items()))

    def check_reads(self, p_h: int) -> None:
        got, want = self.reads(), oracle(self.variant, self.log[:p_h]).reads()
        if got != want:
            raise Failed(f"read at applied {self.applied} (P_H {p_h}): {got} != {want}")

    def digest(self) -> tuple:
        live = set(self.uploads.values()) | ({self.cur} if self.cur else set())
        live |= {v[1] for v in self.docs.values() if v[0] == "blob"}
        live |= {n for n in self.disk.data if n.startswith("stage-") and n not in self.retired}
        files = tuple(sorted((n, bytes(self.disk.data.get(n, b"<gone>"))) for n in live))
        return self.reads(), files, self.counter, self.nputs, self.curlen


def oracle(variant: str, log: list[tuple]) -> Node:
    n = Node(variant if variant in SEMANTIC else "fixed")
    for i, op in enumerate(log):
        n.log.append(op)
        for _ in n.apply(i + 1, op):
            pass
    return n


def run(variant: str, name: str, script: list[tuple], power: bool, again: str = "") -> list[str]:
    total = sum(1 for _ in Node(variant).steps(script))
    failures = []
    for k in range(total + 1):
        node = Node(variant)
        it = node.steps(script)
        for _ in range(k):
            next(it)
        try:
            node.recover(power, again)
            got = node.digest()
        except Failed as e:
            failures.append(f"{name} {'power' if power else 'process'} {again} crash after step {k}: {e}")
            continue
        want = oracle(variant, node.log).digest()
        if got != want:
            failures.append(f"{name} {'power' if power else 'process'} {again} crash after step {k}: "
                            f"digest {got} != {want}")
    return failures


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--variant", default="all")
    args = ap.parse_args()
    variants = list(PASS + FAIL) if args.variant == "all" else [args.variant]
    ok = True
    for v in variants:
        agains = ("", "before-replay", "mid-replay") if v in PASS else ("",)
        fails = [f for name, s in SCRIPTS.items() for power in (True, False) for again in agains
                 for f in run(v, name, s, power, again)]
        points = sum(sum(1 for _ in Node(v).steps(s)) + 1 for s in SCRIPTS.values()) * 2 * len(agains)
        expect = "none" if v in PASS else "some"
        print(f"{v}: {len(fails)} failing crash points of {points} (expected {expect})")
        for f in fails[:3]:
            print(f"  {f}")
        ok &= (len(fails) == 0) if v in PASS else (len(fails) > 0)
    print("RESULT", "as expected" if ok else "UNEXPECTED")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
