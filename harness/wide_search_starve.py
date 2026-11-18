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

"""wide_search_starve.py BIN OUT PORT [--indexes N] [--clients C,...] [--secs S] [--create-only | --reuse]: S2's probe.

Can one client's concurrent wide searches starve every other client? Starts a node at its default reactor count, creates
N small indexes and a one-document `light` index, then times a light GET alone and beside C client threads that loop a
wide search over all N indexes, by explicit name list (no walk gate) and by pattern (/w*/, behind the walk gate). Prints
light GET p50/p99/max, the wide searches' statuses and p50, and the worst loops' lag over each phase; the verdict is T5's
bound on the name-list phases: light GET p99 within 2x its alone p99 + 1 ms. --create-only creates the indexes under OUT
and stops; --reuse times the indexes an earlier run left there, so a light job creates and a strict one times.
"""

import argparse
import atexit
import concurrent.futures
import http.client
import json
import os
import shutil
import subprocess
import sys
import threading
import time
from typing import Any

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import stall_load

ap = argparse.ArgumentParser()
ap.add_argument("bin")
ap.add_argument("out")
ap.add_argument("port", type=int)
ap.add_argument("--indexes", type=int, default=2000)
ap.add_argument("--clients", default="16,64")
ap.add_argument("--secs", type=float, default=12.0)
ap.add_argument("--create-only", action="store_true")
ap.add_argument("--reuse", action="store_true")
args = ap.parse_args()
OUT = os.path.abspath(args.out)
DATA = os.path.join(OUT, "data")
if not args.reuse:
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(DATA)
node = subprocess.Popen(
    [os.path.abspath(args.bin), "--port", str(args.port), "--name", "starve", "-D", DATA, "--shards", "1"],
    stdout=open(os.path.join(OUT, "node.log"), "ab"),
    stderr=subprocess.STDOUT,
)
atexit.register(lambda: node.poll() is None and node.kill())


def call(method: str, path: str, body: object = None, timeout: float = 120) -> tuple[int, bytes]:
    c = http.client.HTTPConnection("127.0.0.1", args.port, timeout=timeout)
    try:
        c.request(
            method,
            path,
            body=json.dumps(body) if body is not None else None,
            headers={"Content-Type": "application/json"},
        )
        r = c.getresponse()
        return r.status, r.read()
    finally:
        c.close()


for _ in range(240):
    try:
        if call("GET", "/")[0] == 200:
            break
    except Exception:
        time.sleep(0.5)
else:
    raise SystemExit("node did not start")


def short(i: int) -> str:
    d = "0123456789abcdefghijklmnopqrstuvwxyz"
    return "w" + d[i // 1296 % 36] + d[i // 36 % 36] + d[i % 36]


def create(i: int) -> int:
    s = 0
    for j in range(3):
        s = max(s, call("PUT", f"/{short(i)}/d{j}", {"n": i * 8 + j, "tag": "x"})[0])
    return s


if not args.reuse:
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        st = list(pool.map(create, range(args.indexes)))
    assert all(s < 300 for s in st), sorted(set(st))
    call("PUT", "/light/doc1?commit", {"n": 1})
    print(f"created {args.indexes} indexes in {time.time() - t0:.0f} s", flush=True)
    if args.create_only:
        node.terminate()
        node.wait(120)
        sys.exit(0)

BODY = {"_query": {"tag": "x"}, "_sort": "n", "_limit": 10}
NAMES = "/" + ",".join(short(i) for i in range(args.indexes)) + "/"
PATTERN = "/w*/"


def light_probe(stop: threading.Event, lat: list[float]) -> None:
    c = http.client.HTTPConnection("127.0.0.1", args.port, timeout=60)
    while not stop.is_set():
        t = time.perf_counter()
        c.request("GET", "/light/doc1")
        r = c.getresponse()
        r.read()
        lat.append((time.perf_counter() - t) * 1000)
        time.sleep(0.005)


def pct(xs: list[float], q: float) -> float:
    xs = sorted(xs)
    return xs[min(len(xs) - 1, int(len(xs) * q))] if xs else float("nan")


def phase(label: str, clients: int, path: str) -> dict[str, Any]:
    stop = threading.Event()
    lat: list[float] = []
    statuses: list[int | str] = []
    wide_ms: list[float] = []
    before = stall_load.parse_metrics(call("GET", "/:metrics")[1].decode())

    def wide() -> None:
        c = http.client.HTTPConnection("127.0.0.1", args.port, timeout=300)
        while not stop.is_set():
            t = time.perf_counter()
            try:
                c.request("SEARCH", path, body=json.dumps(BODY), headers={"Content-Type": "application/json"})
                r = c.getresponse()
                r.read()
                statuses.append(r.status)
            except Exception as e:
                statuses.append(type(e).__name__)
                c = http.client.HTTPConnection("127.0.0.1", args.port, timeout=300)
            wide_ms.append((time.perf_counter() - t) * 1000)

    ws = [threading.Thread(target=wide) for _ in range(clients)]
    for w in ws:
        w.start()
    time.sleep(1.0 if clients else 0)
    pt = threading.Thread(target=light_probe, args=(stop, lat))
    pt.start()
    time.sleep(args.secs)
    stop.set()
    pt.join()
    for w in ws:
        w.join()
    after = stall_load.parse_metrics(call("GET", "/:metrics")[1].decode())
    lag = stall_load.lag_delta(before, after)
    worst = sorted(lag.items(), key=lambda kv: -(kv[1]["max_us_window"] or 0))[:3]
    counts = {str(s): statuses.count(s) for s in set(statuses)}
    print(
        f"{label}: light GET n={len(lat)} p50={pct(lat, 0.5):.2f} p99={pct(lat, 0.99):.2f} max={max(lat):.2f} ms; "
        f"wide searches {counts}, p50 {pct(wide_ms, 0.5):.0f} ms; worst loops "
        + ", ".join(f"{k} p99 {v['p99_us']} max<= {v['max_us_window']} us" for k, v in worst),
        flush=True,
    )
    return {
        "label": label,
        "clients": clients,
        "names": path == NAMES,
        "light_p99_ms": pct(lat, 0.99),
        "light_max_ms": max(lat),
        "wide": counts,
        "wide_p50_ms": pct(wide_ms, 0.5),
    }


results = [phase("alone", 0, NAMES)]
call("SEARCH", NAMES, BODY)  # warm: open what the pool keeps
for c in [int(x) for x in args.clients.split(",")]:
    results.append(phase(f"{c} clients x names", c, NAMES))
    results.append(phase(f"{c} clients x pattern", c, PATTERN))
json.dump(results, open(os.path.join(OUT, "starve.json"), "w"), indent=1)
node.terminate()
node.wait(120)
bound = 2 * results[0]["light_p99_ms"] + 1
misses = [r["label"] for r in results[1:] if r["names"] and r["light_p99_ms"] > bound]
print(
    f"T5 bound {bound:.2f} ms (2x alone p99 + 1 ms) on the name-list phases:",
    "PASS" if not misses else "FAIL: " + ", ".join(misses),
)
sys.exit(1 if misses else 0)
