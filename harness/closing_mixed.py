#!/usr/bin/env python3
"""closing_mixed.py -- concurrent writes and searches on one index of any system, then an audit (T7's crash leg).

Reached as `engine_mixed_load.py --system SYS --kill-at SECS ...`. Writers PUT single documents, bulk-load batches of
20, replace and (where the system can) delete their own documents; searchers run term queries. Every acknowledged write
is recorded with its time. With --kill-at the system gets SIGKILL that many seconds in, under load, restarts on the same
data, and the audit follows engine_crash_check.sh:

  lost      a write acknowledged more than --grace seconds before the kill, missing or with the wrong value
  late      a write acknowledged inside the grace window, missing (reported with the age of the oldest; the documented
            window of an acknowledgement before the write is durable, D5)
  unwritten a document present that no writer ever sent
  duplicated an id present more than once

SIGKILL tests a process crash, not power loss. Without --kill-at the run ends cleanly and the audit has no grace.

With --stream-corpus (T7 as the design runs it, CB-R16) the writes are the C-logs write stream: batches of --write-batch
documents at --write-rate a second, each carrying `stream` (t7w<writer>) and `seq`, appended to an index that already
holds the base (a pristine copy under --keep, or --base-corpus loaded first). The audit covers only the marked documents
and checks the sent-id log against them; the base's count must be unchanged. The log of every id sent is written to
--sent-log.
"""

from __future__ import annotations

import argparse
import itertools
import json
import os
import random
import re
import shlex
import threading
import time
from collections import Counter
from collections.abc import Iterator
from dataclasses import dataclass, field
from typing import Any

import loadtest as lt
import systems as sy

BULK = 20
MARKED = re.compile(r"w\d+-\d+")  # the ids the writers send; anything else in the index is not audited
GRACE = {"xapiand": 2.0, "es": 5.0, "quickwit": 5.0}  # D5's process-crash windows


@dataclass
class Record:
    """What a writer knows about one id: the value of its last acknowledged write (None: deleted), when it was
    acknowledged, and whether a later write is still unacknowledged."""

    value: int | None
    acked: float
    pending: bool = False


@dataclass
class Model:
    lock: threading.Lock = field(default_factory=threading.Lock)
    records: dict[str, Record] = field(default_factory=dict)
    attempted: set[str] = field(default_factory=set)
    stats: Counter[str] = field(default_factory=Counter)
    failures: list[str] = field(default_factory=list)

    def attempt(self, ids: list[str]) -> None:
        with self.lock:
            self.attempted.update(ids)
            for i in ids:
                if i in self.records:
                    self.records[i].pending = True

    def ack(self, ids: list[str], values: list[int | None], at: float) -> None:
        with self.lock:
            for i, v in zip(ids, values, strict=True):
                self.records[i] = Record(v, at)

    def count(self, key: str) -> None:
        with self.lock:
            self.stats[key] += 1

    def fail(self, what: str) -> None:
        with self.lock:
            if len(self.failures) < 10:
                self.failures.append(what)


def writer(
    w: int,
    system: sy.System,
    schema: sy.Schema,
    index: str,
    target: str,
    model: Model,
    stop: threading.Event,
    ending: threading.Event,
    seed: int,
) -> None:
    rng = random.Random(seed * 100 + w)
    client = sy.Client(target, timeout=60)
    i = 0
    mine: list[str] = []

    def doc(doc_id: str, n: int) -> dict[str, Any]:
        return {"id": doc_id, "w": w, "n": n, "txt": f"writer {w} doc {n}"}

    while not stop.is_set():
        op = rng.random()
        ids: list[str] = []
        try:
            if op < 0.5 or not mine:
                i += 1
                ids = [f"w{w}-{i}"]
                model.attempt(ids)
                status, _ = client.call(system.put_request(index, ids[0], doc(ids[0], i)))
                values: list[int | None] = [i]
                what = "put"
                if status < 300:
                    mine.append(ids[0])
            elif op < 0.75:
                ids = [f"w{w}-{i + k + 1}" for k in range(BULK)]
                i += BULK
                model.attempt(ids)
                lines = [json.dumps(doc(d, int(d.split("-")[1]))).encode() for d in ids]
                req = system.bulk_request(index, schema, lines)
                status, body = client.call(req)
                status = status if system.parse_bulk(status, body, BULK).ok else 599
                values = [int(d.split("-")[1]) for d in ids]
                what = "bulk"
                if status < 300:
                    mine.extend(ids)
            elif op < 0.9 and system.supports_delete:
                victim = rng.choice(mine[-200:])
                ids = [victim]
                model.attempt(ids)
                n = int(victim.split("-")[1]) + 1_000_000  # a replace gives the id a new value
                status, _ = client.call(system.put_request(index, victim, doc(victim, n)))
                values = [n]
                what = "replace"
            elif system.supports_delete:
                victim = mine.pop(rng.randrange(max(0, len(mine) - 200), len(mine)))
                ids = [victim]
                model.attempt(ids)
                status, _ = client.call(system.delete_request(index, victim))
                values = [None]
                what = "delete"
            else:
                continue
            now = time.time()
            if status < 300:
                model.ack(ids, values, now)
                model.count(f"write.{what}.ok")
            elif not ending.is_set():
                model.count(f"write.{what}.{status}")
                model.fail(f"{what} {ids[:1]}: HTTP {status}")
        except (OSError, ValueError) as e:
            client.close()
            if not ending.is_set():
                model.fail(f"write exception {e!r}")
            time.sleep(0.05)


def searcher(
    s: int,
    system: sy.System,
    schema: sy.Schema,
    index: str,
    target: str,
    model: Model,
    stop: threading.Event,
    ending: threading.Event,
) -> None:
    rng = random.Random(1000 + s)
    client = sy.Client(target, timeout=60)
    while not stop.is_set():
        q = sy.Query("term", "txt", (rng.choice(["writer", "doc", str(rng.randrange(50))]),))
        try:
            status, _ = client.call(system.search_request(index, schema, q))
            model.count(f"search.{'ok' if status < 300 else status}")
            if status >= 500 and not ending.is_set():
                model.fail(f"search HTTP {status}")
        except (OSError, ValueError):
            client.close()
            time.sleep(0.05)


def audit(
    got: list[tuple[str, int]], model: Model, killed: float | None, grace: float, poke_ids: set[str]
) -> dict[str, Any]:
    present = Counter(i for i, _ in got)
    value = dict(got)
    lost: list[str] = []
    late: list[float] = []
    for doc_id, rec in model.records.items():
        settled = killed is None or rec.acked <= killed - grace
        have = doc_id in present
        right = (value.get(doc_id) == rec.value) if rec.value is not None else not have
        if right:
            continue
        if settled and not rec.pending:
            lost.append(doc_id)
        else:
            late.append((killed or rec.acked) - rec.acked)
    unwritten = [i for i in present if i not in model.attempted and i not in poke_ids]
    return {
        "lost_beyond_grace": len(lost),
        "lost_ids": lost[:5],
        "late_inside_grace": len(late),
        "oldest_late_age_s": round(max(late), 3) if late else None,
        "unwritten_present": len(unwritten),
        "duplicated": sum(1 for c in present.values() if c > 1),
        "recovered_docs": len(present),
        "acknowledged_ids": len(model.records),
    }


def send_with_startup_retry(
    client: sy.Client, req: sy.Request, stop: threading.Event, wait: float = 20.0
) -> tuple[int, bytes]:
    """The request, retried for up to `wait` seconds while the system answers 503: Quickwit refuses ingests for a moment
    after a start, before its ingester has joined the router."""
    deadline = time.time() + wait
    while True:
        status, body = client.call(req)
        if status != 503 or time.time() > deadline or stop.is_set():
            return status, body
        stop.wait(0.2)


def stream_writer(
    k: int,
    system: sy.System,
    schema: sy.Schema,
    index: str,
    target: str,
    model: Model,
    stop: threading.Event,
    ending: threading.Event,
    lines: Iterator[bytes],
    pull: threading.Lock,
    rate: float,
    batch: int,
) -> None:
    """Appends marked batches of the write corpus at `rate` documents a second (open loop: the schedule does not wait
    for an acknowledgement, only for this writer's previous request to return)."""
    client = sy.Client(target, timeout=60)
    stream = f"t7w{k}"
    seq = 0
    due = time.time()
    while not stop.is_set():
        if (wait := due - time.time()) > 0:
            stop.wait(wait)
            continue
        due += batch / rate
        with pull:
            raw = list(itertools.islice(lines, batch))
        if not raw:
            return
        ids: list[str] = []
        payload: list[bytes] = []
        for line in raw:
            seq += 1
            doc = json.loads(line)
            doc["stream"], doc["seq"] = stream, seq
            ids.append(f"{stream}-{seq}")
            payload.append(json.dumps(doc).encode())
        model.attempt(ids)
        try:
            status, body = send_with_startup_retry(client, system.bulk_request(index, schema, payload), stop)
            ok = status < 300 and system.parse_bulk(status, body, len(payload)).ok
        except (OSError, ValueError) as e:
            client.close()
            ok, status = False, 599
            if not ending.is_set():
                model.fail(f"stream write exception {e!r}")
        if ok:
            model.ack(ids, list(range(seq - len(ids) + 1, seq + 1)), time.time())
            model.count("write.stream.ok")
        elif not ending.is_set():
            model.count(f"write.stream.{status}")
            model.fail(f"stream write {ids[0]}: HTTP {status}")


def write_sent_log(path: str, model: Model, order: list[str]) -> None:
    """One line per id sent: the id, whether it was acknowledged, and when."""
    with open(path, "w") as fh:
        for doc_id in order:
            rec = model.records.get(doc_id)
            fh.write(f"{doc_id}\t{'ack' if rec else 'unacked'}\t{rec.acked if rec else ''}\n")


def passed(report: dict[str, Any], died: bool, failures: list[str]) -> bool:
    """Nothing lost beyond the window, nothing present that was not sent, nothing duplicated, the base unchanged."""
    return (
        not died
        and not failures
        and report["lost_beyond_grace"] == 0
        and report["unwritten_present"] == 0
        and report["duplicated"] == 0
        and report.get("base_unchanged", True)
    )


def run_stream(args: argparse.Namespace) -> int:
    system = sy.make(args.system, args.target)
    args.grace = args.grace or GRACE[args.system]
    schema = sy.Schema.load(args.schema)
    index = args.index
    system.start(
        sy.Launch(
            args.bin,
            args.out,
            int(args.target.rpartition(":")[2]),
            cpus=args.cpus,
            prefix=tuple(shlex.split(args.launch_prefix)),
            durability=args.durability,
            keep=args.keep,
        )
    )
    ctl = sy.Client(args.target, timeout=120)
    if args.keep:
        system.commit(ctl, index)
    else:
        lt.load_index(system, args.target, index, schema, args.base_corpus, args.base_docs, 4, 1000)
    base_before = system.wait_count(ctl, index, schema, 0)
    model = Model()
    stop, ending = threading.Event(), threading.Event()
    lines = lt.read_corpus(args.stream_corpus, 0)
    pull = threading.Lock()
    per_writer = args.write_rate / args.writers
    threads = [
        threading.Thread(
            target=stream_writer,
            args=(
                k,
                system,
                schema,
                index,
                args.target,
                model,
                stop,
                ending,
                lines,
                pull,
                per_writer,
                args.write_batch,
            ),
        )
        for k in range(args.writers)
    ] + [
        threading.Thread(target=searcher, args=(s, system, schema, index, args.target, model, stop, ending))
        for s in range(args.searchers)
    ]
    t0 = time.time()
    for t in threads:
        t.start()
    killed: float | None = None
    deadline = t0 + (args.kill_at or args.secs)
    while time.time() < deadline and system.proc is not None and system.proc.poll() is None:
        time.sleep(0.1)
    died = system.proc is not None and system.proc.poll() is not None
    if args.kill_at:
        ending.set()
        killed = time.time()
        system.stop(kill=True)
    stop.set()
    for t in threads:
        t.join(timeout=120)
    restart_s = None
    poke_ids: set[str] = set()
    if args.kill_at:
        restart_s = system.restart(kill=True)
        ctl.close()
        ctl = sy.Client(args.target, timeout=120)
        poke = sy.Client(args.target, timeout=120)
        send_with_startup_retry(
            poke,
            system.bulk_request(index, schema, [json.dumps({"stream": "t7poke", "seq": 0}).encode()], last=True),
            threading.Event(),
        )
        poke.close()
        poke_ids.add("t7poke-0")
    system.commit(ctl, index)
    time.sleep(1.0 + (system.recovery_wait() if args.kill_at else 0.0))
    got: list[tuple[str, int]] = []
    for k in range(args.writers):
        got.extend((f"t7w{k}-{n}", n) for n in system.dump_stream(ctl, index, f"t7w{k}"))
    total = system.wait_count(ctl, index, schema, 0)
    poked = len(system.dump_stream(ctl, index, "t7poke")) if poke_ids else 0  # the poke may not be searchable yet
    base_after = total - len(got) - poked
    report = audit(got, model, killed, args.grace if args.kill_at else 0.0, poke_ids)
    report["base_docs_before"], report["base_docs_after"] = base_before, base_after
    report["base_unchanged"] = base_before == base_after
    ok = passed(report, died, model.failures)
    sent_log = args.sent_log or os.path.join(os.path.dirname(os.path.abspath(args.out)), "sent.tsv")
    write_sent_log(sent_log, model, sorted(model.attempted))
    result = {
        "system": args.system,
        "mode": "stream",
        "secs": round(time.time() - t0, 1),
        "kill_at": args.kill_at or None,
        "grace_s": args.grace if args.kill_at else None,
        "restart_to_ready_s": None if restart_s is None else round(restart_s, 3),
        "node_died_before_kill": died,
        "stats": dict(sorted(model.stats.items())),
        "first_failures": model.failures[:5],
        "audit": report,
        "sent_log": sent_log,
        "ok": ok,
        "note": "SIGKILL is a process crash, not power loss" if args.kill_at else None,
    }
    system.stop()
    text = json.dumps(result, indent=1)
    if args.result:
        with open(args.result, "w") as fh:
            fh.write(text + "\n")
    print(text)
    return 0 if ok else 1


def run(args: argparse.Namespace) -> int:
    if args.stream_corpus:
        return run_stream(args)
    system = sy.make(args.system, args.target)
    args.grace = args.grace or GRACE[args.system]
    schema = sy.Schema.load("mix")
    index = "mix"
    system.start(
        sy.Launch(
            args.bin,
            args.out,
            int(args.target.rpartition(":")[2]),
            cpus=args.cpus,
            prefix=tuple(shlex.split(args.launch_prefix)),
            durability=args.durability,
        )
    )
    ctl = sy.Client(args.target, timeout=120)
    system.setup(ctl, index, schema)
    model = Model()
    stop, ending = threading.Event(), threading.Event()
    threads = [
        threading.Thread(target=writer, args=(w, system, schema, index, args.target, model, stop, ending, args.seed))
        for w in range(args.writers)
    ] + [
        threading.Thread(target=searcher, args=(s, system, schema, index, args.target, model, stop, ending))
        for s in range(args.searchers)
    ]
    t0 = time.time()
    for t in threads:
        t.start()
    killed: float | None = None
    deadline = t0 + (args.kill_at or args.secs)
    while time.time() < deadline and system.proc is not None and system.proc.poll() is None:
        time.sleep(0.1)
    died = system.proc is not None and system.proc.poll() is not None
    if args.kill_at:
        ending.set()
        killed = time.time()
        system.stop(kill=True)
    stop.set()
    for t in threads:
        t.join(timeout=120)
    poke_ids: set[str] = set()
    restart_s = None
    if args.kill_at:
        restart_s = system.restart(kill=True)
        ctl.close()
        ctl = sy.Client(args.target, timeout=120)
        # A write opens a shard writable and replays its log (replay can be lazy), so poke before listing.
        poke = sy.Client(args.target, timeout=120)
        poke_ids.add("poke")
        poke_doc = json.dumps({"id": "poke", "w": 0, "n": 0, "txt": "poke"}).encode()
        poke.call(system.bulk_request(index, schema, [poke_doc], last=True))  # last: Quickwit commits what is pending
        poke.close()
    system.commit(ctl, index)
    time.sleep(1.0 + (system.recovery_wait() if args.kill_at else 0.0))
    got = [(i, n) for i, n in system.dump(ctl, index) if MARKED.fullmatch(i)]  # only the marked write stream is audited
    report = audit(got, model, killed, args.grace if args.kill_at else 0.0, poke_ids)
    ok = passed(report, died, model.failures)
    result = {
        "system": args.system,
        "secs": round(time.time() - t0, 1),
        "kill_at": args.kill_at or None,
        "grace_s": args.grace if args.kill_at else None,
        "restart_to_ready_s": None if restart_s is None else round(restart_s, 3),
        "node_died_before_kill": died,
        "stats": dict(sorted(model.stats.items())),
        "first_failures": model.failures[:5],
        "audit": report,
        "ok": ok,
        "note": "SIGKILL is a process crash, not power loss" if args.kill_at else None,
    }
    system.stop()
    text = json.dumps(result, indent=1)
    if args.result:
        with open(args.result, "w") as fh:
            fh.write(text + "\n")
    print(text)
    return 0 if ok else 1


def add_arguments(ap: argparse.ArgumentParser) -> None:
    g = ap.add_argument_group("system mode (T7's crash leg)")
    g.add_argument("--system", choices=sorted(sy.ADAPTERS))
    g.add_argument("--kill-at", type=float, default=0.0, help="SIGKILL this many seconds in, then restart and audit")
    g.add_argument(
        "--grace", type=float, default=2.0, help="writes acknowledged within this long of the kill may be lost"
    )
    g.add_argument("--target", default="127.0.0.1:18990")
    g.add_argument("--cpus", type=int, default=8)
    g.add_argument("--launch-prefix", default="")
    g.add_argument("--durability", choices=("async", "request"), default="async")
    g.add_argument("--result", help="write the JSON here")
    st = ap.add_argument_group("the marked write stream (T7 on the C-logs copy)")
    st.add_argument("--stream-corpus", help="the write corpus (C-logs writes); switches to the stream mode")
    st.add_argument("--index", default="logs", help="the index written (stream mode)")
    st.add_argument("--schema", default="logs", help="its schema, with the stream and seq fields")
    st.add_argument("--keep", action="store_true", help="start on the data already in OUTDIR/data (a pristine copy)")
    st.add_argument("--base-corpus", help="without --keep: load this base first")
    st.add_argument("--base-docs", type=int, default=0)
    st.add_argument("--write-rate", type=float, default=5000.0, help="documents a second, over all writers")
    st.add_argument("--write-batch", type=int, default=100)
    st.add_argument("--sent-log", help="the log of every id sent (default: beside OUTDIR)")
