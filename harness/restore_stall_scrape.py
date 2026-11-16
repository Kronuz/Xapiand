#!/usr/bin/env python3
"""What one GET /:metrics costs, against a trivial GET, at several RLIMIT_NOFILE soft limits.

The node inherits this process's soft limit. XapiandManager::server_metrics_impl polls every descriptor up to the
limit (system.cc get_open_files_per_proc), on the loop that serves the scrape.

  quiet strict NAME 10 "what" -- harness/restore_stall_scrape.py --bin BIN --limits 1048576,65536,10240,1024
"""

from __future__ import annotations

import argparse
import asyncio
import os
import resource
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stall_load as sl  # noqa: E402


async def timed(conn: sl.Conn, path: str, n: int) -> list[float]:
    out = []
    for _ in range(n):
        t = time.perf_counter()
        status, _ = await conn.request("GET", path, timeout=30)
        assert status == 200, status
        out.append((time.perf_counter() - t) * 1e3)
        await asyncio.sleep(0.05)
    return out


async def one(args: argparse.Namespace, limit: int) -> None:
    hard = resource.getrlimit(resource.RLIMIT_NOFILE)[1]
    resource.setrlimit(resource.RLIMIT_NOFILE, (limit, hard))
    ns = argparse.Namespace(target=args.target, bin=args.bin, workdir=args.workdir, processors=1, index_size="")
    node = sl.Node(ns)
    try:
        conn = sl.Conn(node.host, node.port)
        await timed(conn, "/:metrics", 3)
        m = await timed(conn, "/:metrics", args.n)
        g = await timed(conn, "/", args.n)
        print(f"RLIMIT_NOFILE {limit:8d}: GET /:metrics median {statistics.median(m):6.1f} ms (min {min(m):.1f}, max {max(m):.1f}); GET / median {statistics.median(g):.2f} ms", flush=True)
    finally:
        node.stop()
        resource.setrlimit(resource.RLIMIT_NOFILE, (min(1048576, hard), hard))


async def amain(args: argparse.Namespace) -> None:
    for limit in (int(x) for x in args.limits.split(",")):
        await one(args, limit)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--target", default="127.0.0.1:18340")
    ap.add_argument("--workdir", default=os.path.join(sl.REPO, ".scratch", "rs-scrape"))
    ap.add_argument("--limits", default="1048576,65536,10240,1024")
    ap.add_argument("-n", type=int, default=20)
    asyncio.run(amain(ap.parse_args()))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
