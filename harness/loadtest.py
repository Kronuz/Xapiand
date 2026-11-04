#!/usr/bin/env python3
"""loadtest.py -- a dependency-free performance driver for Xapiand, Elasticsearch and Quickwit (--system).

Legacy mode (Xapiand only, no --corpus): bulk-RESTORE the accounts dataset replicated to a target size, then replay
a validated query mix at a fixed concurrency; baseline_7c.sh and perfdiff.py use it.

  loadtest.py --target localhost:8880 --dataset docs/public/assets/accounts.ndjson --replicate 20 \\
      --concurrency 16 --duration 8 --trials 3 --datadir DIR --out harness/results/run.json

Closing mode (--corpus; docs/design/closing-benchmark.md, T1 to T4 and T7): load a corpus through several clients in
batches, then run the neutral queries at concurrency 1 (each query's fastest of --runs runs, range bounds varied per
run), optionally a closed-loop throughput phase, and a restart phase.

  loadtest.py --system es --target 127.0.0.1:19200 --corpus logs.ndjson.bz2 --schema logs --limit 1000000 \\
      --clients 8 --batch-docs 1000 --queries kept.jsonl --latency --runs 10 --out es.json
  loadtest.py --system quickwit --bin quickwit --workdir DIR --target 127.0.0.1:17280 --tune ...

--bin starts the system (in --workdir, under --launch-prefix, for example a systemd-run scope) and stops it on exit;
--target alone uses a running one. Every result embeds the effective settings read back from the system, the binary's
sha256 and `file` output, each phase's wall time and, on Linux, the cgroup's CPU and memory counters.
"""

from __future__ import annotations

import argparse
import bz2
import contextlib
import hashlib
import json
import math
import os
import queue
import shlex
import statistics
import subprocess
import sys
import threading
import time
from collections.abc import Iterator
from typing import Any, override

import systems as sy

# Validated SEARCH bodies (against the accounts dataset). Mix of query types.
QUERY_MIX: list[tuple[str, dict[str, Any]]] = [
    ("match_all", {}),
    ("term_gender", {"_query": {"gender": "male"}}),
    ("nested_name", {"_query": {"name.firstName": "Michael"}}),
    ("range_age", {"_query": {"age": {"_in": {"_range": {"_from": 20, "_to": 30}}}}}),
    ("range_bal", {"_query": {"balance": {"_in": {"_range": {"_from": 1000, "_to": 3000}}}}}),
    ("term_state", {"_query": {"contact.state": "California"}}),
    (
        "bool_and",
        {"_query": {"_and": [{"gender": "male"}, {"age": {"_in": {"_range": {"_from": 25, "_to": 40}}}}]}},
    ),
]


def log(msg: str) -> None:
    print(f"[loadtest {time.strftime('%H:%M:%S')}] {msg}", file=sys.stderr, flush=True)


def pct(sorted_values: list[float], p: float) -> float | None:
    if not sorted_values:
        return None
    k = min(len(sorted_values) - 1, max(0, math.ceil(round(p / 100.0 * len(sorted_values), 9)) - 1))
    return sorted_values[k]


def geomean(values: list[float]) -> float | None:
    vals = [v for v in values if v > 0]
    return math.exp(sum(math.log(v) for v in vals) / len(vals)) if vals else None


def rounded(v: float | None, digits: int = 3) -> float | None:
    return None if v is None else round(v, digits)


# ---------------------------------------------------------------------------------------------
# Legacy: one RESTORE of the accounts dataset, a closed-loop query mix


def build_bulk(dataset: str, replicate: int) -> tuple[bytes, int]:
    with open(dataset, "rb") as f:
        lines = [ln for ln in f.read().split(b"\n") if ln.strip()]
    # Replicate to reach a workload size; auto-ids keep copies distinct.
    return b"\n".join(lines * replicate) + b"\n", len(lines) * replicate


def phase_index_legacy(target: str, index: str, dataset: str, replicate: int, datadir: str | None) -> dict[str, Any]:
    body, ndocs = build_bulk(dataset, replicate)
    client = sy.Client(target, timeout=600)
    t0 = time.perf_counter()
    status, _ = client.call(sy.Request("RESTORE", f"/{index}/?commit", body, sy.NDJSON_CT))
    elapsed = time.perf_counter() - t0
    client.close()
    # This index's own directory, not the cumulative data dir; settled, since a sharded index is still flushing its
    # .__N shards when RESTORE ?commit returns.
    index_dir = os.path.join(datadir, index) if datadir else None
    return {
        "docs": ndocs,
        "seconds": round(elapsed, 3),
        "docs_per_sec": round(ndocs / elapsed) if elapsed > 0 else 0,
        "restore_status": status,
        "ok": status in (200, 201, 204),
        "disk_bytes": sy.dir_bytes_settled(index_dir, 15.0, 0.75) if index_dir and os.path.isdir(index_dir) else None,
    }


def phase_query(
    target: str, requests: list[tuple[str, sy.Request]], concurrency: int, duration: float
) -> dict[str, Any]:
    """A closed loop: `concurrency` clients cycle through `requests` for `duration` seconds."""
    latencies: list[float] = []
    errors = [0]
    lock = threading.Lock()
    stop_at = time.perf_counter() + duration

    def worker(wid: int) -> None:
        client = sy.Client(target, timeout=30)
        local: list[float] = []
        local_err = 0
        i = 0
        while time.perf_counter() < stop_at:
            _label, req = requests[(wid + i) % len(requests)]
            i += 1
            t0 = time.perf_counter()
            try:
                status, _data = client.call(req)
                dt = (time.perf_counter() - t0) * 1000.0
                if status == 200:
                    local.append(dt)
                else:
                    local_err += 1
            except Exception:
                local_err += 1
                client.close()
        client.close()
        with lock:
            latencies.extend(local)
            errors[0] += local_err

    threads = [threading.Thread(target=worker, args=(w,)) for w in range(concurrency)]
    t0 = time.perf_counter()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    wall = time.perf_counter() - t0
    latencies.sort()
    n = len(latencies)
    return {
        "concurrency": concurrency,
        "duration": round(wall, 2),
        "total": n,
        "errors": errors[0],
        "qps": round(n / wall) if wall > 0 else 0,
        "latency_ms": {
            "p50": rounded(pct(latencies, 50)),
            "p90": rounded(pct(latencies, 90)),
            "p95": rounded(pct(latencies, 95)),
            "p99": rounded(pct(latencies, 99)),
            "max": rounded(latencies[-1]) if latencies else None,
            "mean": rounded(statistics.fmean(latencies)) if latencies else None,
        },
    }


def run_trials(
    target: str, requests: list[tuple[str, sy.Request]], args: argparse.Namespace
) -> tuple[dict[str, Any], list[dict[str, Any]]]:
    trials: list[dict[str, Any]] = []
    for tr in range(args.trials):
        q = phase_query(target, requests, args.concurrency, args.duration)
        trials.append(q)
        lat = q["latency_ms"]
        log(
            f"[query] trial {tr + 1}/{args.trials}: {q['qps']} qps, p50={lat['p50']}ms p95={lat['p95']}ms "
            f"p99={lat['p99']}ms (n={q['total']}, err={q['errors']})"
        )
    return sorted(trials, key=lambda x: x["qps"])[len(trials) // 2], trials


def legacy(args: argparse.Namespace) -> dict[str, Any]:
    result: dict[str, Any] = {"target": args.target, "label": args.label, "index": args.index}
    idx_trials: list[dict[str, Any]] = []
    for tr in range(args.trials):
        # A fresh index per trial so each is a clean bulk load (a noise band).
        ip = phase_index_legacy(
            args.target, f"{args.index}_t{tr}", args.dataset, args.replicate, args.datadir if tr == 0 else None
        )
        idx_trials.append(ip)
        log(
            f"[index] trial {tr + 1}/{args.trials}: {ip['docs']} docs in {ip['seconds']}s = {ip['docs_per_sec']} docs/s"
            + (f", disk={ip['disk_bytes']}" if ip["disk_bytes"] else "")
        )
    median = sorted(idx_trials, key=lambda x: x["docs_per_sec"])[len(idx_trials) // 2]
    median["disk_bytes"] = idx_trials[0]["disk_bytes"]  # one index's worth, from the first trial
    result["index_phase"] = median
    result["index_trials"] = idx_trials
    # Query the index the index phase populated (trial 0's). A nonexistent index still answers SEARCH with 200 and
    # no hits, so probe once and fail loudly.
    query_index = f"{args.index}_t0"
    requests = [
        (label, sy.Request("SEARCH", f"/{query_index}/", json.dumps(body).encode())) for label, body in QUERY_MIX
    ]
    client = sy.Client(args.target, timeout=30)
    pstatus, pdata = client.call(requests[0][1])
    client.close()
    try:
        pcount = json.loads(pdata).get("count", 0)
    except ValueError:
        pcount = 0
    if pstatus != 200 or not pcount:
        log(f"[query] ABORT: '{query_index}' returned status={pstatus} count={pcount}: nothing to query")
        sys.exit(2)
    result["query_phase"], result["query_trials"] = run_trials(args.target, requests, args)
    return result


# ---------------------------------------------------------------------------------------------
# Closing mode: corpus, loader, latency, throughput, restart


def read_corpus(path: str, limit: int) -> Iterator[bytes]:
    opener = bz2.open if path.endswith(".bz2") else open
    with opener(path, "rb") as fh:
        for n, line in enumerate(fh):
            if limit and n >= limit:
                return
            if line.strip():
                yield line


def batches(lines: Iterator[bytes], size: int) -> Iterator[list[bytes]]:
    batch: list[bytes] = []
    for ln in lines:
        batch.append(ln)
        if len(batch) == size:
            yield batch
            batch = []
    if batch:
        yield batch


class Sampler(threading.Thread):
    """Peak of the cgroup's memory.current (or the process group's RSS) over a phase."""

    def __init__(self, system: sy.System, every: float = 0.25) -> None:
        super().__init__(daemon=True)
        self.system, self.every = system, every
        self.peak = 0
        self.halt = threading.Event()
        self.cg = system.cgroup()

    def sample(self) -> int:
        if self.cg is not None:
            raw = self.cg.read("memory.current")
            return int(raw) if raw else 0
        return (self.system.rss_kb() or 0) * 1024

    @override
    def run(self) -> None:
        while not self.halt.is_set():
            self.peak = max(self.peak, self.sample())
            self.halt.wait(self.every)

    def finish(self) -> int:
        self.halt.set()
        self.join()
        self.peak = max(self.peak, self.sample())
        if self.cg is not None and (raw := self.cg.read("memory.peak")):
            self.peak = max(self.peak, int(raw))
        return self.peak


def load_index(
    system: sy.System,
    target: str,
    index: str,
    schema: sy.Schema,
    corpus: str,
    limit: int,
    clients: int,
    batch_docs: int,
    bulk_mode: bool = True,
    resume: str | None = None,
) -> dict[str, Any]:
    """T1/T2: create the index, load the corpus through `clients` senders in batches of `batch_docs`, timed from the
    first request to the point where every document is searchable. With `resume` (a state file) the load goes in
    segments, each acknowledged in full before the next starts, and a rerun skips the segments already in."""
    ctl = sy.Client(target)
    done_batches = done_docs = 0
    if resume and os.path.exists(resume):
        with open(resume) as fh:
            state = json.load(fh)
        done_batches, done_docs = int(state["batches"]), int(state["docs"])
        log(f"[index] resuming {index} after {done_batches} batches ({done_docs} documents)")
    t_create = time.perf_counter()
    if not done_batches:
        system.setup(ctl, index, schema, bulk=bulk_mode)
    create_s = time.perf_counter() - t_create
    cg = system.cgroup()
    cg0 = cg.snapshot() if cg else None
    sampler = Sampler(system)
    sampler.start()
    work: queue.Queue[list[bytes] | None] = queue.Queue(maxsize=clients * 2)
    sent = [done_docs, done_batches]
    failures: list[str] = []
    lock = threading.Lock()

    def sender() -> None:
        client = sy.Client(target)
        while (batch := work.get()) is not None:
            try:
                status, body = client.call(system.bulk_request(index, schema, batch))
                res = system.parse_bulk(status, body, len(batch))
            except OSError as e:
                client.close()
                res = sy.Bulk(False, 0, repr(e))
            with lock:
                sent[0] += res.indexed
                sent[1] += 1
                if not res.ok and len(failures) < 5:
                    failures.append(res.error)
            work.task_done()
        work.task_done()
        client.close()

    threads = [threading.Thread(target=sender) for _ in range(clients)]
    t0 = time.perf_counter()
    for t in threads:
        t.start()
    pending: list[bytes] | None = None
    segment = clients * 16
    for n, batch in enumerate(batches(read_corpus(corpus, limit), batch_docs)):
        if n < done_batches:
            continue
        if pending is not None:
            work.put(pending)
            if resume and (n - done_batches) % segment == 0:
                work.join()  # a segment is in only when every sender has acknowledged its batches
                if not failures:
                    with open(resume, "w") as fh:
                        json.dump({"batches": sent[1], "docs": sent[0]}, fh)
        pending = batch
    for _ in threads:
        work.put(None)
    for t in threads:
        t.join()
    # The final batch goes alone and flags the commit (X `?commit`, Q `commit=force`) once everything else is in.
    if pending is not None:
        status, body = ctl.call(system.bulk_request(index, schema, pending, last=True))
        res = system.parse_bulk(status, body, len(pending))
        sent[0] += res.indexed
        sent[1] += 1
        if not res.ok:
            failures.append(res.error)
    t_load = time.perf_counter() - t0
    system.commit(ctl, index, bulk=bulk_mode)
    got = system.wait_count(ctl, index, schema, sent[0])
    elapsed = time.perf_counter() - t0
    peak = sampler.finish()
    merges = system.settle_merges(ctl, index)  # every build ends with its merges finished (C7)
    system.durable(ctl, index)  # a flush and a sync, so `du` reads final bytes
    cg1 = cg.snapshot() if cg else None
    t_settle = time.perf_counter()
    disk = system.disk_bytes()
    if resume and os.path.exists(resume):
        os.unlink(resume)
    docs = sent[0]
    cpu_s = None
    if cg0 and cg1 and cg0["cpu_usage_usec"] is not None and cg1["cpu_usage_usec"] is not None:
        cpu_s = (cg1["cpu_usage_usec"] - cg0["cpu_usage_usec"]) / 1e6
    ctl.close()
    return {
        "docs": docs,
        "requests": sent[1],
        "clients": clients,
        "batch_docs": batch_docs,
        "seconds": round(elapsed, 3),
        "send_seconds": round(t_load, 3),
        "docs_per_sec": round(docs / elapsed) if elapsed > 0 else 0,
        "searchable_count": got,
        "ok": not failures and got == docs,
        "failures": failures,
        "cpu_seconds_per_1000_docs": rounded(cpu_s / docs * 1000, 4) if cpu_s is not None and docs else None,
        "segments_after_merges": merges,
        "peak_memory_bytes": peak,
        "memory_source": "cgroup" if cg else "rss",
        "disk_bytes": disk,
        "bytes_per_doc": round(disk / docs, 2) if disk is not None and docs else None,
        "phase_seconds": {
            "create": round(create_s, 3),
            "send": round(t_load, 3),
            "commit_and_visible": round(elapsed - t_load, 3),
            "settle_disk": round(time.perf_counter() - t_settle, 3),
        },
    }


def tune(
    system: sy.System, target: str, schema: sy.Schema, corpus: str, docs: int, min_gain: float = 0.10
) -> dict[str, Any]:
    """D4: the bulk size found by doubling from 100 documents until throughput levels, then senders doubled until it
    levels again (each step loads `docs` documents into a fresh index)."""
    steps: list[dict[str, Any]] = []

    def trial(clients: int, batch: int) -> float:
        res = load_index(system, target, f"tune_c{clients}_b{batch}", schema, corpus, docs, clients, batch)
        steps.append({k: res[k] for k in ("clients", "batch_docs", "docs_per_sec", "ok")})
        log(f"[tune] {clients} clients x {batch} docs: {res['docs_per_sec']} docs/s")
        return float(res["docs_per_sec"])

    batch, best = 100, trial(1, 100)
    while batch < 100_000:
        rate = trial(1, batch * 2)
        if rate < best * (1 + min_gain):
            break
        batch, best = batch * 2, rate
    clients = 1
    while clients < 64:
        rate = trial(clients * 2, batch)
        if rate < best * (1 + min_gain):
            break
        clients, best = clients * 2, rate
    return {"batch_docs": batch, "clients": clients, "docs_per_sec": best, "steps": steps}


def latency_phase(
    system: sy.System,
    target: str,
    index: str,
    schema: sy.Schema,
    queries: list[sy.Query],
    runs: int,
    seed: int,
    warm_secs: float = 0.0,
) -> dict[str, Any]:
    """T4: every query at concurrency 1 as a top-10 and a count (an aggregation only as a search). A warm-up runs
    first, untimed (`warm_secs` seconds cycling the queries, or one pass); then each query runs `runs` times. A range
    template draws its bounds per run and its statistic is the median of its draws; any other query keeps its fastest
    run. Per class: the geometric mean of those, and p50 and p99 over all runs. The client floor and the server's CPU
    per query (cgroup) come beside."""
    client = sy.Client(target)
    cg = system.cgroup()
    warm_end = time.monotonic() + warm_secs
    warmed = False
    while not warmed or time.monotonic() < warm_end:
        warmed = True
        for q in queries:
            for cmd in ("search",) if q.agg else ("search", "count"):
                system.run(client, index, schema, q.as_command(cmd))
    floor_req = system.floor_request(index)
    floor: list[float] = []
    for _ in range(max(runs * 10, 100)):
        t0 = time.perf_counter()
        client.call(floor_req)
        floor.append((time.perf_counter() - t0) * 1000.0)
    floor.sort()
    cpu0 = cg.snapshot()["cpu_usage_usec"] if cg else None
    timed = 0
    best: dict[tuple[str, str], list[float]] = {}
    every: dict[tuple[str, str], list[float]] = {}
    errors: dict[tuple[str, str], int] = {}
    per_query: list[dict[str, Any]] = []
    for q in queries:
        for cmd in ("search",) if q.agg else ("search", "count"):
            times: list[float] = []
            key = (q.cls, cmd)
            for run in range(runs):
                req = system.search_request(index, schema, q.vary(run, seed).as_command(cmd))
                t0 = time.perf_counter()
                try:
                    status, _ = client.call(req)
                except OSError:
                    client.close()
                    status = 599
                dt = (time.perf_counter() - t0) * 1000.0
                if status < 300:
                    times.append(dt)
                else:
                    errors[key] = errors.get(key, 0) + 1
            timed += len(times)
            stat = (statistics.median(times) if q.range else min(times)) if times else None
            if stat is not None:
                best.setdefault(key, []).append(stat)
                every.setdefault(key, []).extend(times)
            per_query.append(
                {"query": q.label, "command": cmd, "statistic_ms": rounded(stat), "template": bool(q.range)}
            )
    cpu1 = cg.snapshot()["cpu_usage_usec"] if cg else None
    client.close()
    classes: dict[str, Any] = {}
    for key in sorted(set(best) | set(errors)):
        allv = sorted(every.get(key, []))
        classes[f"{key[0]}/{key[1]}"] = {
            "queries": len(best.get(key, [])),
            "geomean_fastest_ms": rounded(geomean(best.get(key, [])), 4),
            "p50_ms": rounded(pct(allv, 50), 4),
            "p99_ms": rounded(pct(allv, 99), 4),
            "errors": errors.get(key, 0),
        }
    return {
        "runs": runs,
        "seed": seed,
        "warm_secs": warm_secs,
        "classes": classes,
        "per_query": per_query,
        "client_floor_ms": {
            "p50": rounded(pct(floor, 50), 4),
            "min": rounded(floor[0], 4),
            "p99": rounded(pct(floor, 99), 4),
        },
        "server_cpu_us_per_query": rounded((cpu1 - cpu0) / timed, 1)
        if cpu0 is not None and cpu1 is not None and timed
        else None,
    }


def closed_loop(
    system: sy.System, target: str, index: str, schema: sy.Schema, queries: list[sy.Query], args: argparse.Namespace
) -> dict[str, Any]:
    requests = [(q.label, system.search_request(index, schema, q.as_command("search"))) for q in queries]
    median, trials = run_trials(target, requests, args)
    return {"query_phase": median, "query_trials": trials}


def recover_phase(
    system: sy.System, target: str, index: str, schema: sy.Schema, probe: sy.Query, kill: bool, drop: bool
) -> dict[str, Any]:
    """T7: stop the system (SIGKILL when `kill`), drop the page cache, start it, and poll `probe` every 10 ms; the
    time to the first answer that finds documents and that search's latency."""
    system.stop(kill=kill)
    dropped = sy.drop_caches() if drop else False
    client = sy.Client(target, timeout=5)
    t0 = time.monotonic()
    system.spawn()
    first_s = first_ms = None
    answer_count = None
    while time.monotonic() - t0 < 600:
        if system.proc is not None and system.proc.poll() is not None:
            raise SystemExit(f"{system.name} exited with {system.proc.returncode} while recovering")
        s = time.perf_counter()
        try:
            ans = system.run(client, index, schema, probe)
        except (OSError, ValueError):
            client.close()
            time.sleep(0.01)
            continue
        if ans.ok and ans.count:
            first_ms = (time.perf_counter() - s) * 1000
            first_s = time.monotonic() - t0
            answer_count = ans.count
            break
        time.sleep(0.01)
    return {
        "kill": kill,
        "page_cache_dropped": dropped,
        "start_to_first_answer_s": rounded(first_s, 3),
        "first_search_ms": rounded(first_ms, 3),
        "first_count": answer_count,
    }


def binary_info(path: str) -> dict[str, Any]:
    real = os.path.abspath(path)
    h = hashlib.sha256()
    with open(real, "rb") as fh:
        for chunk in iter(lambda: fh.read(1 << 20), b""):
            h.update(chunk)
    kind = subprocess.run(["file", "-b", real], capture_output=True, text=True, check=False).stdout.strip()
    return {"path": real, "sha256": h.hexdigest(), "file": kind}


def closing(args: argparse.Namespace) -> dict[str, Any]:
    t_run = time.perf_counter()
    system = sy.make(args.system, args.target)
    schema = sy.Schema.load(args.schema)
    result: dict[str, Any] = {"system": args.system, "target": args.target, "label": args.label, "index": args.index}
    phases: dict[str, float] = {}
    if args.bin:
        t0 = time.perf_counter()
        system.start(
            sy.Launch(
                args.bin,
                args.workdir,
                int(args.target.rpartition(":")[2]),
                cpus=args.cpus,
                memory_bytes=args.memory_bytes,
                prefix=tuple(shlex.split(args.launch_prefix)),
                durability=args.durability,
                memory_lock=args.memory_lock,
                shards=args.shards,
                keep=args.keep,
            )
        )
        phases["start"] = round(time.perf_counter() - t0, 3)
        result["binary"] = binary_info(args.bin)
    elif args.workdir:
        system.datadir = os.path.join(os.path.abspath(args.workdir), "data")
    try:
        ctl = sy.Client(args.target)
        eff = system.effective(ctl, None)
        result["effective_before"] = eff
        problems = system.assert_effective(eff, args.cpus)
        if problems:
            result["settings_problems"] = problems
            if args.assert_settings:
                raise SystemExit(f"effective settings differ from what was asked: {problems}")
        if args.tune:
            result["tune"] = tune(system, args.target, schema, args.corpus, args.tune_docs)
            return result
        t0 = time.perf_counter()
        if args.keep:
            result["segments_at_start"] = system.segments(ctl, args.index)
        else:
            result["index_phase"] = load_index(
                system,
                args.target,
                args.index,
                schema,
                args.corpus,
                args.limit,
                args.clients,
                args.batch_docs,
                True,
                args.resume,
            )
            phases["load"] = round(time.perf_counter() - t0, 3)
            ip = result["index_phase"]
            log(f"[index] {ip['docs']} docs in {ip['seconds']}s = {ip['docs_per_sec']} docs/s, disk {ip['disk_bytes']}")
            if not ip["ok"]:
                raise SystemExit(
                    f"the load failed: {ip['failures']} (searchable {ip['searchable_count']} of {ip['docs']})"
                )
        result["effective_after"] = system.effective(ctl, args.index)
        queries: list[sy.Query] = sy.load_queries(args.queries) if args.queries else []
        if queries and args.latency:
            t0 = time.perf_counter()
            result["cache_before"] = system.cache_counters(ctl)
            result["latency"] = latency_phase(
                system, args.target, args.index, schema, queries, args.runs, args.seed, args.warm_secs
            )
            result["cache_after"] = system.cache_counters(ctl)
            if result["cache_after"].get("result_hits", 0) != result["cache_before"].get("result_hits", 0):
                raise SystemExit("a result cache answered during the latency phase: the timings are void")
            phases["latency"] = round(time.perf_counter() - t0, 3)
        if queries and args.duration > 0:
            t0 = time.perf_counter()
            result.update(closed_loop(system, args.target, args.index, schema, queries, args))
            phases["closed_loop"] = round(time.perf_counter() - t0, 3)
        if args.restart and queries:
            probe = next((q for q in queries if q.cls == "term"), queries[0]).as_command("count")
            t0 = time.perf_counter()
            result["restart"] = recover_phase(system, args.target, args.index, schema, probe, False, args.drop_caches)
            phases["restart"] = round(time.perf_counter() - t0, 3)
        cg = system.cgroup()
        if cg is not None:
            result["cgroup"] = cg.snapshot()
        ctl.close()
    finally:
        phases["total"] = round(time.perf_counter() - t_run, 3)
        result["phase_seconds"] = phases
        if args.bin:
            system.stop()
    return result


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--system", choices=sorted(sy.ADAPTERS), default="xapiand")
    ap.add_argument("--target", default="localhost:8880")
    ap.add_argument("--index", default="bench_load")
    ap.add_argument("--label", default="")
    ap.add_argument("--out", default=None)
    ap.add_argument("--concurrency", type=int, default=16, help="closed-loop clients")
    ap.add_argument("--duration", type=float, default=8, help="closed-loop seconds per trial (0: skip)")
    ap.add_argument("--trials", type=int, default=3)
    legacy_group = ap.add_argument_group("legacy mode (Xapiand, accounts)")
    legacy_group.add_argument("--dataset", default="docs/public/assets/accounts.ndjson")
    legacy_group.add_argument("--replicate", type=int, default=20)
    legacy_group.add_argument("--datadir", default=None)
    cl = ap.add_argument_group("closing mode")
    cl.add_argument("--corpus", help="ndjson or ndjson.bz2, one document a line")
    cl.add_argument("--schema", help="a name in harness/closing/schemas.json")
    cl.add_argument("--limit", type=int, default=0, help="the corpus's first N documents (0: all)")
    cl.add_argument("--clients", type=int, default=4, help="bulk senders")
    cl.add_argument("--batch-docs", type=int, default=1000)
    cl.add_argument("--queries", help="neutral queries, one JSON line each (the D8-kept set)")
    cl.add_argument("--latency", action="store_true", help="T4: concurrency-1 latency per class")
    cl.add_argument("--runs", type=int, default=10)
    cl.add_argument("--warm-secs", type=float, default=0.0, help="warm up by time (one pass when 0)")
    cl.add_argument("--resume", help="a state file: the load goes in acknowledged segments and a rerun continues")
    cl.add_argument("--seed", type=int, default=7)
    cl.add_argument("--restart", action="store_true", help="T7: clean restart and time to the first answer")
    cl.add_argument("--drop-caches", action="store_true", help="drop the page cache before the restart (Linux, root)")
    cl.add_argument("--tune", action="store_true", help="find the bulk size and sender count (D4), then stop")
    cl.add_argument("--tune-docs", type=int, default=50_000)
    ln = ap.add_argument_group("starting the system")
    ln.add_argument("--bin", help="start this binary (the system is stopped on exit)")
    ln.add_argument("--workdir", help="its directory (the data is in WORKDIR/data)")
    ln.add_argument("--cpus", type=int, default=8)
    ln.add_argument("--memory-bytes", type=int, default=0, help="E's heap; 0 leaves it to the system")
    ln.add_argument("--launch-prefix", default="", help="for example 'systemd-run --scope -p AllowedCPUs=0-7'")
    ln.add_argument("--durability", choices=("async", "request"), default="async", help="E's translog durability")
    ln.add_argument("--memory-lock", action="store_true", help="E: bootstrap.memory_lock")
    ln.add_argument("--shards", type=int, default=1, help="S and X: shards per index (an arm setting: 1 and 5)")
    ln.add_argument(
        "--keep", action="store_true", help="start on the data already in WORKDIR/data and skip the load (T4, T5, T7)"
    )
    ln.add_argument("--assert-settings", action="store_true", help="fail when the read-back settings differ (D2)")
    args = ap.parse_args()
    if args.bin and not args.workdir:
        ap.error("--bin needs --workdir")
    if args.corpus or args.keep:
        if not args.schema:
            ap.error("--corpus and --keep need --schema")
        result = closing(args)
    else:
        if args.system != "xapiand":
            ap.error("the accounts mix is Xapiand's; give --corpus and --schema for Elasticsearch or Quickwit")
        result = legacy(args)
    result["argv"] = sys.argv[1:]
    text = json.dumps(result, indent=2)
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w") as f:
            f.write(text + "\n")
        log(f"wrote {args.out}")
    else:
        print(text)
    with contextlib.suppress(Exception):
        sys.stdout.flush()


if __name__ == "__main__":
    main()
