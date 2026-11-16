#!/usr/bin/env python3
"""restore_stall_probe: what a RESTORE stream does to a trivial GET and to the reactor loop, with the loop sampled.

Starts a node (--processors 1 by default), loads --docs `accounts`, then runs 20 s phases: idle (no traffic), trivial, trivial2 (A/A), restore (the trivial stream
plus a RESTORE client), trivial3 (A/A); the restore phase's RESTOREs are 5,000-document chunks into another index. `--sample` runs macOS `sample` on the node for the restore phase. With
`--spin US` the node runs the patched reactor_lag.h (harness/restore_stall_reactor_lag.patch, env XAPIAND_LAG_SPIN):
a sleep-free loop-iteration probe that logs every gap over US and signals the loop thread for a stack while it is
stalled; the lines (SPIN, STALL) are in <workdir>/node.log.

  quiet strict NAME 30 "what" -- harness/restore_stall_probe.py --bin BIN --docs 20000 --sample --spin 4000
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import shutil
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stall_load as sl


def pctl(values: list[int], p: float) -> float | None:
    if not values:
        return None
    s = sorted(values)
    return s[min(len(s) - 1, int(p * len(s)))] / 1000.0  # ms


def summary(values: list[int]) -> dict:
    return {
        "n": len(values),
        "p50_ms": pctl(values, 0.50),
        "p99_ms": pctl(values, 0.99),
        "p999_ms": pctl(values, 0.999),
        "max_ms": (max(values) / 1000.0) if values else None,
    }


async def phase(node: sl.Node, ctl: sl.Conn, name: str, args: argparse.Namespace, restore: bool = False) -> dict:
    """A trivial GET at --rate over --conns connections for --secs (the first --warmup dropped), with a client
    RESTOREing 5,000-document chunks back to back into another index when `restore`."""
    loop = asyncio.get_running_loop()
    before = await sl.scrape(ctl)
    t0 = loop.time()
    end_at = t0 + args.secs
    lat: list[int] = []
    errors = [0]
    pool: asyncio.Queue[sl.Conn] = asyncio.Queue()
    for _ in range(args.conns):
        pool.put_nowait(sl.Conn(node.host, node.port))

    async def one(sched: float, doc_id: int) -> None:
        conn = await pool.get()
        try:
            status, _ = await conn.request("GET", args.path.format(id=doc_id), timeout=60)
            if status != 200:
                errors[0] += 1
            elif sched - t0 >= args.warmup:
                lat.append(int((loop.time() - sched) * 1e6))
        except Exception:
            errors[0] += 1
            conn = sl.Conn(node.host, node.port)
        finally:
            pool.put_nowait(conn)

    async def generator() -> None:
        interval = 1.0 / args.rate
        i = 0
        tasks: list[asyncio.Task] = []
        while True:
            sched = t0 + i * interval
            if sched >= end_at:
                break
            delay = sched - loop.time()
            if delay > 0:
                await asyncio.sleep(delay)
            tasks.append(asyncio.ensure_future(one(sched, 1 + (i * 7919) % args.docs)))
            i += 1
        await asyncio.gather(*tasks)

    restored = [0.0, 0]

    async def restore_client() -> None:
        conn = sl.Conn(node.host, node.port)
        rows = sl.load_rows()
        first = 1
        while loop.time() < end_at:
            t = loop.time()
            status, _ = await conn.request(
                "RESTORE", "/restore-target/?commit", sl.restore_body(rows, first, 5000), sl.NDJSON_CT, timeout=300
            )
            if status >= 300:
                errors[0] += 1
            restored[0] += loop.time() - t
            restored[1] += 5000
            first += 5000
        await conn.close()

    cpu0 = node.cpu_s()
    jobs = [generator()]
    if restore:
        jobs.append(restore_client())
    await asyncio.gather(*jobs)
    cpu1 = node.cpu_s()
    after = await sl.scrape(ctl)
    lag = sl.lag_delta(before, after)
    out = {
        "phase": name,
        "trivial": summary(lat),
        "restored_docs": restored[1],
        "restore_docs_per_s": round(restored[1] / restored[0], 0) if restored[0] else None,
        "errors": errors[0],
        "node_cpu_s": round((cpu1 or 0) - (cpu0 or 0), 1),
        "lag_us": {
            k: {x: v[x] for x in ("wakes", "p50_us", "p99_us", "p999_us", "max_us_window")} for k, v in lag.items()
        },
    }
    print(json.dumps(out), flush=True)
    return out


_scrape = sl.scrape


async def scrape_logged(conn: sl.Conn) -> dict:
    # Each scrape's start and end on the steady clock (CLOCK_MONOTONIC_RAW, libc++'s steady_clock, the clock of the node's SPIN/STALL at_ns).
    t0 = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
    out = await _scrape(conn)
    print(json.dumps({"scrape": [t0, time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)]}), flush=True)
    return out


sl.scrape = scrape_logged


async def idle(node: sl.Node, ctl: sl.Conn, secs: float) -> dict:
    before = await sl.scrape(ctl)
    await asyncio.sleep(secs)
    after = await sl.scrape(ctl)
    lag = sl.lag_delta(before, after)
    out = {
        "phase": "idle",
        "secs": secs,
        "lag_us": {
            k: {x: v[x] for x in ("wakes", "p50_us", "p99_us", "p999_us", "max_us_window")} for k, v in lag.items()
        },
    }
    print(json.dumps(out), flush=True)
    return out


async def amain(args: argparse.Namespace) -> int:
    if args.spin:
        os.environ["XAPIAND_LAG_SPIN"] = str(args.spin)
    ns = argparse.Namespace(
        target=args.target, bin=args.bin, workdir=args.workdir, processors=args.processors, index_size=""
    )
    node = sl.Node(ns)
    results = []
    try:
        rows = sl.load_rows()
        t = await sl.load_accounts(node.host, node.port, rows, args.docs)
        await sl.settle_merges(node, "accounts")
        ctl = sl.Conn(node.host, node.port)
        print(json.dumps({"loaded_docs": args.docs, "load_s": round(t, 1), "pid": node.pid}), flush=True)
        if args.quiet_secs:
            # No request at all for this long: the node's SPIN gaps in the window are the loop's own (nothing scraped it).
            q0 = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
            await asyncio.sleep(args.quiet_secs)
            print(json.dumps({"quiet": [q0, time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)]}), flush=True)
        results.append(await idle(node, ctl, args.idle_secs))
        for name in args.phases.split(","):
            if name == "restore":
                sampler = None
                if args.sample:
                    sample_s = max(5, int(args.secs) - 4)
                    await asyncio.sleep(0)
                    sampler = await asyncio.create_subprocess_exec(
                        "sample",
                        str(node.pid),
                        str(sample_s),
                        "1",
                        "-file",
                        os.path.join(args.workdir, "restore.sample.txt"),
                        stdout=asyncio.subprocess.DEVNULL,
                        stderr=asyncio.subprocess.DEVNULL,
                    )
                results.append(await phase(node, ctl, "restore", args, restore=True))
                if sampler:
                    await sampler.wait()
            else:
                results.append(await phase(node, ctl, name, args))
        if args.out:
            with open(args.out, "w") as fh:
                json.dump(results, fh, indent=1)
    finally:
        os.makedirs(args.keep, exist_ok=True)
        for f in ("node.log", "restore.sample.txt"):
            if os.path.exists(os.path.join(args.workdir, f)):
                shutil.copy(os.path.join(args.workdir, f), os.path.join(args.keep, f))
        node.stop()
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--target", default="127.0.0.1:18330")
    ap.add_argument("--workdir", default=os.path.join(sl.REPO, ".scratch", "restore_stall"))
    ap.add_argument("--processors", type=int, default=1)
    ap.add_argument("--docs", type=int, default=20_000)
    ap.add_argument("--secs", type=float, default=20.0)
    ap.add_argument("--idle-secs", type=float, default=20.0)
    ap.add_argument("--warmup", type=float, default=2.0)
    ap.add_argument("--rate", type=float, default=200.0)
    ap.add_argument("--conns", type=int, default=16)
    ap.add_argument("--path", default="/accounts/{id}", help="the trivial GET's path; `/` reads no document")
    ap.add_argument("--phases", default="trivial,trivial2,restore,trivial3")
    ap.add_argument("--sample", action="store_true")
    ap.add_argument(
        "--quiet-secs", type=float, default=0.0, help="a stretch with no request at all, before the idle phase"
    )
    ap.add_argument("--spin", type=int, default=0, help="XAPIAND_LAG_SPIN threshold in us (patched reactor_lag.h)")
    ap.add_argument(
        "--keep",
        default=os.path.join(sl.REPO, ".scratch", "restore_stall_out"),
        help="where node.log and the sample go",
    )
    ap.add_argument("--out")
    return asyncio.run(amain(ap.parse_args()))


if __name__ == "__main__":
    raise SystemExit(main())
