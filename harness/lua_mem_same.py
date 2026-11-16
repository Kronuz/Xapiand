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

"""lua_mem_same.py BINARY OUT [--loops 4] [--processors 1]

The memory cap is per call: T10's library-call loops (harness/lua_bounds.py, LIB_LOOP) run --loops at once
on a node with --processors reactors, fewer than the loops, so calls of one script share a reactor and its
cached state. Every PUT must answer 2xx. Exits non-zero otherwise.
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
import threading
import time

# harness/lua_bounds.py's LIB_LOOP: an 8 MiB string and about 2 MB of garbage an iteration.
LIB_LOOP = (
    "local s = string.rep('abcdefgh', 1048576); for i = 1, 150 do s:find('b', 1, true) end; "
    "local u = 0; for i = 1, 100 do u = u + #s:sub(1, 1000000):upper() end; _doc.u = u"
)


def free_port() -> int:
    with socket.socket() as s:
        s.bind(("127.0.0.1", 0))
        return int(s.getsockname()[1])


def call(port: int, method: str, path: str, body: object = None, timeout: float = 120) -> tuple[int, bytes]:
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        data = json.dumps(body) if body is not None else None
        c.request(method, path, data, {"Content-Type": "application/json"})
        r = c.getresponse()
        return r.status, r.read()
    finally:
        c.close()


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("out")
    ap.add_argument("--loops", type=int, default=4)
    ap.add_argument("--processors", type=int, default=1)
    ap.add_argument("--rounds", type=int, default=3)
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
            "luasame",
            "--processors",
            str(args.processors),
            "-D",
            os.path.join(out, "data"),
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

    failures: list[str] = []
    for r in range(args.rounds):
        statuses: list[int] = [0] * args.loops
        timings: list[float] = [0.0] * args.loops

        def worker(i: int, r: int = r, statuses: list[int] = statuses, timings: list[float] = timings) -> None:
            t0 = time.time()
            doc = {"x": 1, "_script": {"_type": "script", "_lua": {"_body": LIB_LOOP}}}
            statuses[i], body = call(port, "PUT", f"/lua_same{r}_{i}/doc?commit", doc)
            timings[i] = time.time() - t0
            if not 200 <= statuses[i] < 300:
                print(f"round {r} loop {i} answered {statuses[i]}: {body[:300]!r}", flush=True)

        threads = [threading.Thread(target=worker, args=(i,)) for i in range(args.loops)]
        for t in threads:
            t.start()
        for t in threads:
            t.join()
        print(
            f"round {r}: {args.loops} library-call loops on {args.processors} reactor(s) answered {statuses} in {[round(t, 2) for t in timings]} s",
            flush=True,
        )
        if any(not 200 <= st < 300 for st in statuses):
            failures.append(f"round {r}: {statuses}")
    node.terminate()
    node.wait(60)
    shutil.rmtree(os.path.join(out, "data"), ignore_errors=True)
    for failure in failures:
        print("FAIL:", failure)
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
