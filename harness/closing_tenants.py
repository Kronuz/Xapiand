#!/usr/bin/env python3
"""closing_tenants.py -- the many-index rows of the closing benchmark (docs/design/closing-benchmark.md, T8 to T10).

A fixed total of documents (the first N x P of a corpus) is cut into N tenants of P documents and loaded in one of the
shapes a system documents for multi-tenancy:

  per-index  one index per tenant (every system); Elasticsearch stops creating at --budget-s and reports what it reached
  shared     one index with a `tenant` field (Elasticsearch filters on it, Quickwit partitions on it)

Then, per shape: T8 query every tenant once, idle, and read memory and descriptors over the idle baseline;
T9 restart, drop the page cache, query --cold random tenants once each; T10 one search across every tenant, --runs times.
It is reached as `wide_search_check.py tenants BIN OUTDIR PORT --system SYS ...`.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import json
import os
import random
import shlex
import sys
import threading
import time
from collections.abc import Iterator
from typing import Any

import loadtest as lt
import systems as sy


def tenant_name(i: int) -> str:
    return f"t{i:05x}"


def tenant_batches(corpus: str, tenants: int, per_tenant: int) -> Iterator[tuple[int, list[bytes]]]:
    """The corpus's first tenants x per_tenant documents, cut in order: tenant i holds documents i*P to (i+1)*P - 1."""
    batch: list[bytes] = []
    i = 0
    for ln in lt.read_corpus(corpus, tenants * per_tenant):
        batch.append(ln)
        if len(batch) == per_tenant:
            yield i, batch
            batch = list[bytes]()
            i += 1
    if batch:
        yield i, batch


def snapshot(system: sy.System) -> dict[str, Any]:
    cg = system.cgroup()
    snap: dict[str, Any] = {"rss_bytes": (system.rss_kb() or 0) * 1024, "fds": system.fd_count()}
    if cg is not None:
        c = cg.snapshot()
        snap["memory_current"] = c["memory_current"]
        snap["anon"] = c["memory_stat"].get("anon")
        snap["file"] = c["memory_stat"].get("file")
    return snap


def per_tenant(after: dict[str, Any], before: dict[str, Any], n: int) -> dict[str, Any]:
    out: dict[str, Any] = {}
    for key in ("rss_bytes", "memory_current", "anon", "file", "fds"):
        if after.get(key) is not None and before.get(key) is not None and n:
            out[f"{key}_per_tenant"] = round((after[key] - before[key]) / n, 2)
    return out


def load_per_index(
    system: sy.System, target: str, schema: sy.Schema, args: argparse.Namespace
) -> tuple[int, list[str], float]:
    """Create and load one index per tenant with --workers senders; returns the tenants reached, failures, seconds."""
    deadline = time.monotonic() + args.budget_s
    reached = [0]
    failures: list[str] = []
    lock = threading.Lock()
    local = threading.local()

    def job(item: tuple[int, list[bytes]]) -> None:
        if time.monotonic() > deadline:
            return
        i, docs = item
        client: sy.Client | None = getattr(local, "client", None)
        if client is None:
            client = local.client = sy.Client(target)
        index = tenant_name(i)
        system.setup(client, index, schema)
        status, body = client.call(system.bulk_request(index, schema, docs, last=True))
        res = system.parse_bulk(status, body, len(docs))
        with lock:
            if res.ok:
                reached[0] += 1
            elif len(failures) < 5:
                failures.append(f"{index}: {res.error}")

    t0 = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(args.workers) as pool:
        list(pool.map(job, tenant_batches(args.corpus, args.tenants, args.per_tenant)))
    return reached[0], failures, time.monotonic() - t0


def load_shared(
    system: sy.System, target: str, schema: sy.Schema, index: str, args: argparse.Namespace
) -> tuple[int, list[str], float]:
    """One index holding every tenant's documents, each carrying its tenant name."""
    ctl = sy.Client(target)
    system.setup(ctl, index, schema, bulk=False)
    failures: list[str] = []
    docs_sent = [0]
    lock = threading.Lock()
    pending = list(tenant_batches(args.corpus, args.tenants, args.per_tenant))

    def job(chunk: list[tuple[int, list[bytes]]]) -> None:
        client = sy.Client(target)
        lines = [
            ln.rstrip()[:-1] + b',"tenant":"' + tenant_name(i).encode() + b'"}' for i, docs in chunk for ln in docs
        ]
        status, body = client.call(system.bulk_request(index, schema, lines))
        res = system.parse_bulk(status, body, len(lines))
        with lock:
            docs_sent[0] += res.indexed
            if not res.ok and len(failures) < 5:
                failures.append(res.error)
        client.close()

    per_request = max(1, args.batch_docs // args.per_tenant)
    chunks = [pending[k : k + per_request] for k in range(0, len(pending), per_request)]
    t0 = time.monotonic()
    with concurrent.futures.ThreadPoolExecutor(args.workers) as pool:
        list(pool.map(job, chunks))
    system.commit(ctl, index)
    system.wait_count(ctl, index, schema, docs_sent[0])
    ctl.close()
    return len(pending), failures, time.monotonic() - t0


def timed_queries(
    system: sy.System, client: sy.Client, schema: sy.Schema, queries: list[tuple[str, sy.Query]]
) -> tuple[list[float], int]:
    lat: list[float] = []
    errors = 0
    for index, q in queries:
        t0 = time.perf_counter()
        try:
            status, _ = client.call(system.search_request(index, schema, q))
        except OSError:
            client.close()
            status = 599
        if status < 300:
            lat.append((time.perf_counter() - t0) * 1000)
        else:
            errors += 1
    return lat, errors


def stats(lat: list[float]) -> dict[str, Any]:
    s = sorted(lat)
    return {"n": len(s), "p50_ms": lt.rounded(lt.pct(s, 50), 3), "p99_ms": lt.rounded(lt.pct(s, 99), 3)}


def run_shape(system: sy.System, schema: sy.Schema, shape: str, args: argparse.Namespace) -> dict[str, Any]:
    target = args.target
    out: dict[str, Any] = {"shape": shape, "tenants": args.tenants, "per_tenant": args.per_tenant}
    phases: dict[str, float] = {}
    ctl = sy.Client(target)
    if isinstance(system, sy.Elasticsearch) and shape == "per-index":
        limit = args.tenants + 1000
        body = json.dumps({"persistent": {"cluster.max_shards_per_node": limit}}).encode()
        status, _ = ctl.call(sy.Request("PUT", "/_cluster/settings", body))
        out["es_max_shards_per_node"] = limit if status < 300 else "not raised"
    text_field = next(k for k, v in schema.fields.items() if v == "text")
    base = snapshot(system)
    out["baseline"] = base
    if shape == "per-index":
        reached, failures, secs = load_per_index(system, target, schema, args)
        names = [tenant_name(i) for i in range(reached)]  # the tenants created in order; a budget stops at a prefix
        queries = [(n, sy.Query("tenant_term", text_field, ("the",))) for n in names]
        wide = [("t*", sy.Query("wide", text_field, ("the",)))]
    else:
        reached, failures, secs = load_shared(system, target, schema, "shared", args)
        names = [tenant_name(i) for i in range(reached)]
        queries = [("shared", sy.Query("tenant_term", text_field, ("the",), filter=("tenant", n))) for n in names]
        wide = [("shared", sy.Query("wide", text_field, ("the",)))]
    phases["create_and_load"] = round(secs, 3)
    out.update(
        {
            "tenants_reached": reached,
            "failures": failures,
            "create_rate_per_s": round(reached / secs, 2) if secs else None,
        }
    )
    if failures or reached == 0:
        out["phase_seconds"] = phases
        return out
    # T8: open the tenants in order, one query each, and read memory and descriptors at a quarter, half and all of
    # N_min (the count every shape reached; --n-min, else this shape's own), then idle 60 s at N_min, then the rest.
    n_min = min(args.n_min or reached, reached)
    marks = sorted({max(1, n_min // 4), max(1, n_min // 2), n_min, reached})
    lat: list[float] = []
    errors = 0
    readings: list[dict[str, Any]] = []
    done = 0
    t0 = time.monotonic()
    for mark in marks:
        got, errs = timed_queries(system, ctl, schema, queries[done:mark])
        lat.extend(got)
        errors += errs
        done = mark
        if mark == n_min:
            time.sleep(args.idle_s)
        snap = snapshot(system)
        readings.append({"tenants_open": mark, "at_n_min": mark == n_min, **snap, **per_tenant(snap, base, mark)})
    phases["query_every_tenant"] = round(time.monotonic() - t0, 3)
    out["t8_query_every_tenant"] = {**stats(lat), "errors": errors}
    out["t8_n_min"] = n_min
    out["t8_readings"] = readings
    # T9: restart, drop the page cache, one query to each of --cold random tenants
    rng = random.Random(args.seed)
    picks = rng.sample(range(reached), min(args.cold, reached))
    ctl.close()
    system.stop()
    out["page_cache_dropped"] = sy.drop_caches()
    t0 = time.monotonic()
    system.spawn()
    system.wait_up()
    phases["restart_to_ready"] = round(time.monotonic() - t0, 3)
    ctl = sy.Client(target)
    lat, errors = timed_queries(system, ctl, schema, [queries[i] for i in picks])
    out["t9_cold_first_query"] = {**stats(lat), "errors": errors}
    # T10: one search across every tenant
    sampler = lt.Sampler(system)
    sampler.start()
    wide_lat: list[float] = []
    for run in range(args.runs + 1):
        got, errs = timed_queries(system, ctl, schema, wide)
        if run and not errs:  # the first run opens every index
            wide_lat.extend(got)
    out["t10_wide_search"] = {**stats(wide_lat), "peak_memory_bytes": sampler.finish(), "target": wide[0][0]}
    out["phase_seconds"] = phases
    ctl.close()
    return out


def run(args: argparse.Namespace) -> int:
    system = sy.make(args.system, args.target)
    if isinstance(system, sy.Quickwit):
        system.max_partitions = args.tenants
    result: dict[str, Any] = {"system": args.system, "argv": sys.argv[1:], "shapes": []}
    system.start(
        sy.Launch(
            args.bin,
            args.out,
            int(args.target.rpartition(":")[2]),
            cpus=args.cpus,
            memory_bytes=args.memory_bytes,
            prefix=tuple(shlex.split(args.launch_prefix)),
            extra=tuple(shlex.split(args.extra)),
        )
    )
    result["binary"] = lt.binary_info(args.bin)
    try:
        for shape in args.shape:
            name = "tenants" if shape == "per-index" else "tenants-shared"
            result["shapes"].append(run_shape(system, sy.Schema.load(name), shape, args))
            lt.log(f"{shape}: {json.dumps(result['shapes'][-1])[:300]}")
            if shape != args.shape[-1]:  # a fresh system for the next shape
                system.stop()
                assert system.launch is not None
                system.start(system.launch)
    finally:
        system.stop()
    text = json.dumps(result, indent=1)
    if args.result:
        with open(args.result, "w") as fh:
            fh.write(text + "\n")
    else:
        print(text)
    return 0 if all(not s["failures"] and s["tenants_reached"] for s in result["shapes"]) else 1


def add_arguments(ap: argparse.ArgumentParser) -> None:
    g = ap.add_argument_group("tenants mode (T8 to T10)")
    g.add_argument("--system", choices=sorted(sy.ADAPTERS), default="xapiand")
    g.add_argument("--shape", action="append", choices=("per-index", "shared"), help="repeatable; default per-index")
    g.add_argument("--tenants", type=int, default=10_000)
    g.add_argument("--per-tenant", type=int, default=100)
    g.add_argument("--corpus", help="the wiki corpus (ndjson or ndjson.bz2)")
    g.add_argument("--workers", type=int, default=8)
    g.add_argument("--batch-docs", type=int, default=1000, help="shared shape: documents a request")
    g.add_argument("--budget-s", type=float, default=1800.0, help="per-index creation stops after this long")
    g.add_argument("--idle-s", type=float, default=60.0)
    g.add_argument(
        "--n-min", type=int, default=0, help="N_min: the tenant count every shape reached (default: this shape's)"
    )
    g.add_argument("--cold", type=int, default=1000)
    g.add_argument("--seed", type=int, default=7)
    g.add_argument("--cpus", type=int, default=8)
    g.add_argument("--memory-bytes", type=int, default=0)
    g.add_argument("--launch-prefix", default="")
    g.add_argument(
        "--extra", default="", help="more options for the system's command line, e.g. --database-pool-size N"
    )
    g.add_argument("--result", help="write the JSON here")


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("bin")
    ap.add_argument("out")
    ap.add_argument("port", type=int)
    ap.add_argument("--runs", type=int, default=30)
    add_arguments(ap)
    args = ap.parse_args(argv)
    args.target = f"127.0.0.1:{args.port}"
    args.shape = args.shape or ["per-index"]
    if not args.corpus:
        ap.error("--corpus is required")
    return run(args)


if __name__ == "__main__":
    os.environ.setdefault("PYTHONUNBUFFERED", "1")
    sys.exit(main())
