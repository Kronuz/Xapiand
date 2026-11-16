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

"""lua_mem_seq.py BINARY OUT [--count 200] [--max-rss-mib 1024] [--flags "..."]

Memory of ended Lua calls (x7-gate security S1). One client PUTs --count documents one at a time, each with a
distinct script that builds about 56 MiB of strings and ends. The node's RSS is sampled every 20 requests.
Exits non-zero when RSS passes --max-rss-mib at any sample or keeps growing over the second half of the run
(the last sample more than --max-growth-mib above the one at half), or when a request does not answer 2xx. A run
stops at the first sample past --max-rss-mib.
"""

from __future__ import annotations

import argparse
import atexit
import http.client
import json
import os
import shutil
import socket
import subprocess
import sys
import time

BODY = "local t = {} for i = 1, 56 do t[i] = string.rep('x', 1048000 + i) end _doc.s = #t -- {tag}"


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def call(port: int, method: str, path: str, body: object = None, timeout: float = 300) -> tuple[int, bytes]:
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        data = json.dumps(body) if body is not None else None
        c.request(method, path, data, {"Content-Type": "application/json"})
        r = c.getresponse()
        return r.status, r.read()
    finally:
        c.close()


def rss_mib(pid: int) -> float:
    out = subprocess.run(
        ["ps", "-o", "rss=", "-p", str(pid)], capture_output=True, text=True, check=False
    ).stdout.strip()
    return int(out) / 1024 if out else float("nan")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("out")
    ap.add_argument("--count", type=int, default=200)
    ap.add_argument("--every", type=int, default=20)
    ap.add_argument("--max-rss-mib", type=float, default=1024)
    ap.add_argument("--max-growth-mib", type=float, default=128)
    ap.add_argument("--flags", default="")
    args = ap.parse_args()
    out = os.path.abspath(args.out)
    shutil.rmtree(out, ignore_errors=True)
    os.makedirs(out)
    port = free_port()
    log = open(os.path.join(out, "node.log"), "wb")
    node = subprocess.Popen(
        [
            os.path.abspath(args.binary),
            "--port",
            str(port),
            "--name",
            "luamem",
            "-D",
            os.path.join(out, "data"),
            *args.flags.split(),
        ],
        stdout=log,
        stderr=subprocess.STDOUT,
    )
    atexit.register(lambda: node.poll() is None and node.kill())
    for _ in range(240):
        try:
            if call(port, "GET", "/", timeout=2)[0] == 200:
                break
        except OSError:
            pass
        time.sleep(0.5)
    else:
        print("the node did not start")
        return 2

    samples: list[tuple[int, float]] = [(0, rss_mib(node.pid))]
    print(f"RSS at start {samples[0][1]:.0f} MiB", flush=True)
    statuses: dict[int, bytes] = {}
    for i in range(args.count):
        doc = {"x": 1, "_script": {"_type": "script", "_lua": {"_body": BODY.replace("{tag}", f"s{i}")}}}
        st, raw = call(port, "PUT", f"/seq{i}/doc?commit", doc)
        statuses.setdefault(st, raw[:200])
        if (i + 1) % args.every == 0:
            samples.append((i + 1, rss_mib(node.pid)))
            print(f"after {i + 1} distinct scripts: RSS {samples[-1][1]:.0f} MiB", flush=True)
            if samples[-1][1] > args.max_rss_mib:
                break  # past the bound: the run has failed, and the host's memory is shared
    node.terminate()
    node.wait(60)
    shutil.rmtree(os.path.join(out, "data"), ignore_errors=True)

    peak = max(rss for _, rss in samples)
    half = min(samples, key=lambda s: abs(s[0] - samples[-1][0] // 2))
    growth = samples[-1][1] - half[1]
    print(
        f"statuses {sorted(statuses)}; peak RSS {peak:.0f} MiB; growth over the second half {growth:.0f} MiB (from {half[0]} scripts)"
    )
    report = {
        "count": args.count,
        "samples": samples,
        "peak_mib": peak,
        "second_half_growth_mib": growth,
        "statuses": sorted(statuses),
    }
    with open(os.path.join(out, "lua_mem_seq.json"), "w") as f:
        json.dump(report, f)
    failures = []
    if any(not 200 <= st < 300 for st in statuses):
        failures.append(f"a request answered {statuses}")
    if peak > args.max_rss_mib:
        failures.append(f"RSS reached {peak:.0f} MiB, over the {args.max_rss_mib:.0f} MiB bound")
    if growth > args.max_growth_mib:
        failures.append(
            f"RSS grew {growth:.0f} MiB over the second half, more than {args.max_growth_mib:.0f} MiB: it does not level off"
        )
    for failure in failures:
        print("FAIL:", failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
