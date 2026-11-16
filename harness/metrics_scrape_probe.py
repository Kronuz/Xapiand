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

"""metrics_scrape_probe.py -- what one GET /:metrics holds the serving loop for.

Starts a node (--bin) with one HTTP loop plus the control loop and scrapes /:metrics -n times, 50 ms apart,
against GET / as the floor. With --spin US the node must carry harness/metrics_scrape_reactor_lag.patch (env
XAPIAND_LAG_SPIN): a sleep-free probe on each loop logs every gap over US microseconds between two of its runs,
so `loop turns over US` counts the loop turns (a scrape's own turn included) that held the loop that long.
Prints one JSON line.

  quiet strict NAME 10 "what" -- harness/metrics_scrape_probe.py --bin BIN --spin 3000 --limit 1048576
"""

from __future__ import annotations

import argparse
import http.client
import json
import os
import re
import resource
import statistics
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stall_load as sl


def timed(host: str, port: int, path: str, n: int) -> list[float]:
    conn = http.client.HTTPConnection(host, port, timeout=30)
    out = []
    for _ in range(n):
        t = time.perf_counter()
        conn.request("GET", path)
        resp = conn.getresponse()
        resp.read()
        assert resp.status == 200, resp.status
        out.append((time.perf_counter() - t) * 1e3)
        time.sleep(0.05)
    conn.close()
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--target", default="127.0.0.1:18341")
    ap.add_argument("--workdir", default=os.path.join(sl.REPO, ".scratch", "metrics-scrape"))
    ap.add_argument("--limit", type=int, default=1048576, help="RLIMIT_NOFILE soft limit the node inherits")
    ap.add_argument("--spin", type=int, default=0, help="XAPIAND_LAG_SPIN threshold in us (patched reactor_lag.h)")
    ap.add_argument("-n", type=int, default=40)
    args = ap.parse_args()

    hard = resource.getrlimit(resource.RLIMIT_NOFILE)[1]
    resource.setrlimit(resource.RLIMIT_NOFILE, (min(args.limit, hard), hard))
    if args.spin:
        os.environ["XAPIAND_LAG_SPIN"] = str(args.spin)
    ns = argparse.Namespace(target=args.target, bin=args.bin, workdir=args.workdir, processors=1, index_size="")
    node = sl.Node(ns)
    try:
        timed(node.host, node.port, "/:metrics", 3)
        time.sleep(1.0)  # the warm-up's own turns are not the run's
        mark = os.path.getsize(os.path.join(args.workdir, "node.log"))
        scrape = timed(node.host, node.port, "/:metrics", args.n)
        floor = timed(node.host, node.port, "/", args.n)
        time.sleep(0.5)
        with open(os.path.join(args.workdir, "node.log"), "rb") as f:
            f.seek(mark)
            log = f.read().decode(errors="replace")
        gaps = [int(m) for m in re.findall(r"SPIN loop=\d+ at_ns=\d+ gap_us=(\d+)", log)]
        row = {
            "limit": args.limit,
            "n": args.n,
            "scrape_ms": {"median": statistics.median(scrape), "min": min(scrape), "max": max(scrape)},
            "get_root_ms": {"median": statistics.median(floor), "max": max(floor)},
        }
        if args.spin:
            row["loop_turns_over_us"] = args.spin
            row["loop_turns_over"] = len(gaps)
            row["loop_turn_max_ms"] = max(gaps) / 1e3 if gaps else 0.0
        print(json.dumps(row), flush=True)
    finally:
        node.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
