#!/usr/bin/env python3
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

"""reactor_offload_probe.py -- does slow request work stall a trivial GET on the same reactor?

Starts one node with --processors N (N reactors), loads --docs accounts, then runs phases of --secs each:

  trivial     open-loop GET /accounts/<id> at --rate/s (latency from the scheduled time)
  trivial2    the same again (the A/A: the noise floor of the comparison)
  mixed       the same trivial stream plus --slow closed-loop clients running slow queries
              (an aggregation over every document, a wildcard of 100 hits, an aggregation by state)
  mixed2      the same again
  slow-alone  the slow clients only (the slow queries' own latency)
  trivial3    the trivial stream alone again, after the slow phases
  restore     the trivial stream plus one client RESTOREing 5,000-document chunks into another index (index writes)

Each phase prints the trivial p50/p99/p99.9/max, the slow queries' p50/p99 and each reactor loop's lag delta
(`xapiand_reactor_lag_seconds`, a 1 ms probe timer: lateness is how long a turn held the loop).

  quiet strict NAME 30 "what" -- harness/reactor_offload_probe.py --bin BIN --processors 1 --docs 300000
"""

from __future__ import annotations

import argparse
import asyncio
import json
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stall_load as sl  # noqa: E402

SLOW: list[tuple[str, dict]] = [
    ("agg_age", {"_query": "*", "_limit": 0, "_aggs": {"ages": {"_terms": {"_field": "age"}}}}),
    ("wildcard", {"_query": {"favoriteFruit": {"_wildcard": "ba"}}, "_limit": 100}),
    (
        "agg_state",
        {"_query": {"gender": "male"}, "_limit": 0, "_aggs": {"by_state": {"_values": {"_field": "contact.state"}}}},
    ),
]


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


async def phase(
    node: sl.Node, ctl: sl.Conn, name: str, args: argparse.Namespace, trivial: bool, slow: int, restore: bool = False
) -> dict:
    loop = asyncio.get_running_loop()
    before = await sl.scrape(ctl)
    t0 = loop.time()
    end_at = t0 + args.secs
    lat: list[int] = []
    errors = [0]
    slow_lat: dict[str, list[int]] = {}
    pool: asyncio.Queue[sl.Conn] = asyncio.Queue()
    for _ in range(args.conns):
        pool.put_nowait(sl.Conn(node.host, node.port))

    async def one(sched: float, doc_id: int) -> None:
        conn = await pool.get()
        try:
            status, _ = await conn.request("GET", f"/accounts/{doc_id}", timeout=60)
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

    async def slow_client(n: int) -> None:
        conn = sl.Conn(node.host, node.port)
        i = n
        while loop.time() < end_at:
            kind, q = SLOW[i % len(SLOW)]
            i += 1
            t = loop.time()
            try:
                status, _ = await conn.request("SEARCH", "/accounts/", json.dumps(q).encode(), timeout=120)
                if status != 200:
                    errors[0] += 1
                elif t - t0 >= args.warmup:
                    slow_lat.setdefault(kind, []).append(int((loop.time() - t) * 1e6))
            except Exception:
                errors[0] += 1
                conn = sl.Conn(node.host, node.port)
        await conn.close()

    restored = [0.0, 0]

    async def restore_client() -> None:
        # Index writes: 20,000-document RESTORE chunks into their own index, back to back for the phase.
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
    jobs = [slow_client(n) for n in range(slow)]
    if restore:
        jobs.append(restore_client())
    if trivial:
        jobs.append(generator())
    await asyncio.gather(*jobs)
    cpu1 = node.cpu_s()
    after = await sl.scrape(ctl)
    lag = sl.lag_delta(before, after)
    out = {
        "phase": name,
        "trivial": summary(lat) if trivial else None,
        "slow": {k: summary(v) for k, v in sorted(slow_lat.items())},
        "restored_docs": restored[1],
        "restore_docs_per_s": round(restored[1] / restored[0], 0) if restored[0] else None,
        "errors": errors[0],
        "node_cpu_s": round((cpu1 or 0) - (cpu0 or 0), 1),
        "lag_us": {k: {x: v[x] for x in ("wakes", "p50_us", "p99_us", "p999_us", "max_us_window")} for k, v in lag.items()},
    }
    print(json.dumps(out), flush=True)
    return out


async def amain(args: argparse.Namespace) -> int:
    ns = argparse.Namespace(
        target=args.target, bin=args.bin, workdir=args.workdir, processors=args.processors, index_size=""
    )
    node = sl.Node(ns)
    try:
        rows = sl.load_rows()
        t = await sl.load_accounts(node.host, node.port, rows, args.docs)
        await sl.settle_merges(node, "accounts")
        ctl = sl.Conn(node.host, node.port)
        m = await sl.scrape(ctl)
        print(json.dumps({"loaded_docs": args.docs, "load_s": round(t, 1), "loops": sorted(m["lag"])}), flush=True)
        for kind, q in SLOW:  # the slow queries once each, unloaded, to warm the page cache and size them
            c = sl.Conn(node.host, node.port)
            t1 = time.perf_counter()
            status, body = await c.request("SEARCH", "/accounts/", json.dumps(q).encode(), timeout=300)
            print(json.dumps({"warm": kind, "status": status, "ms": round((time.perf_counter() - t1) * 1e3, 1), "bytes": len(body)}), flush=True)
            await c.close()
        results = [
            await phase(node, ctl, "trivial", args, True, 0),
            await phase(node, ctl, "trivial2", args, True, 0),
            await phase(node, ctl, "mixed", args, True, args.slow),
            await phase(node, ctl, "mixed2", args, True, args.slow),
            await phase(node, ctl, "slow-alone", args, False, args.slow),
            await phase(node, ctl, "trivial3", args, True, 0),
            await phase(node, ctl, "restore", args, True, 0, restore=True),
        ]
        if args.out:
            with open(args.out, "w") as fh:
                json.dump(results, fh, indent=1)
    finally:
        node.stop()
    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--bin", required=True)
    ap.add_argument("--target", default="127.0.0.1:18310")
    ap.add_argument("--workdir", default=os.path.join(sl.REPO, ".scratch", "reactor_offload"))
    ap.add_argument("--processors", type=int, default=1)
    ap.add_argument("--docs", type=int, default=300_000)
    ap.add_argument("--secs", type=float, default=20.0)
    ap.add_argument("--warmup", type=float, default=2.0)
    ap.add_argument("--rate", type=float, default=200.0)
    ap.add_argument("--conns", type=int, default=16)
    ap.add_argument("--slow", type=int, default=4, help="closed-loop slow clients in the mixed phases")
    ap.add_argument("--out")
    return asyncio.run(amain(ap.parse_args()))


if __name__ == "__main__":
    raise SystemExit(main())
