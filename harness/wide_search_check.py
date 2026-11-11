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

"""wide_search_check.py check|gate BIN OUTDIR PORT [--indexes N] [--k K] [--runs R]: a search over many indexes.

A search reads its indexes in batches of at most K held at once (--concurrent-search-indexes) and merges them.

check: creates 3K+2 one-shard indexes (of different sizes and vocabularies) and runs the same searches on one server
  that reads them all in one batch and on one that reads K at a time, over the same data; the hits (ids, order,
  weights), totals and aggregations must be equal, for sorted and for relevance-ranked searches, by name list and by
  pattern (`_all`, `w*`, an exclusion). Then --max-search-indexes: a search over the cap, by list and by `_all`, is served,
  one over it answers 400 naming the option.
gate (the 7c gate row): creates N indexes (default 10,000), then runs R searches (default 30) across all of them and
  prints p50 and p99 latency in ms and the peak number of indexes searches held open (the metric
  xapiand_search_pinned_indexes_peak). With --pool P (the server's --database-pool-size; default its own), it also
  prints the cost of one open index: the resident memory the first search added over the indexes it left open
  (xapiand_resident_memory_bytes and xapiand_databases before and after). Run it under `quiet strict`; the servers use ports PORT and PORT + 1 and keep
  their data under OUTDIR; both are killed on exit.
"""

import argparse
import atexit
import concurrent.futures
import http.client
import json
import os
import shutil
import signal
import statistics
import subprocess
import sys
import time

ap = argparse.ArgumentParser()
ap.add_argument("mode", choices=["check", "gate"])
ap.add_argument("bin")
ap.add_argument("out")
ap.add_argument("port", type=int)
ap.add_argument("--indexes", type=int, default=10000)
ap.add_argument("--k", type=int, default=4)
ap.add_argument("--runs", type=int, default=30)
ap.add_argument("--pool", type=int, default=0)
args = ap.parse_args()
BIN, OUT, PORT = os.path.abspath(args.bin), os.path.abspath(args.out), args.port
DATA = os.path.join(OUT, "data")
failures: list[str] = []
nodes: list[subprocess.Popen[bytes]] = []


def reap() -> None:
    for node in nodes:
        if node.poll() is None:
            node.kill()


atexit.register(reap)


def call(method: str, path: str, body: object = None, port: int = PORT) -> tuple[int, bytes]:
    c = http.client.HTTPConnection("localhost", port, timeout=600)
    c.request(
        method, path, body=json.dumps(body) if body is not None else None, headers={"Content-Type": "application/json"}
    )
    r = c.getresponse()
    return r.status, r.read()


def start(tag: str, *flags: str) -> subprocess.Popen[bytes]:
    node = subprocess.Popen(
        [BIN, "--port", str(PORT), "--name", "wide", "-D", DATA, "--processors", "2", "--shards", "1", *flags],
        stdout=open(os.path.join(OUT, f"node-{tag}.log"), "wb"),
        stderr=subprocess.STDOUT,
    )
    nodes.append(node)
    for _ in range(240):
        try:
            if call("GET", "/")[0] == 200:
                return node
        except Exception:
            time.sleep(0.5)
    node.kill()
    raise SystemExit("the server did not start")


def stop(node: subprocess.Popen[bytes]) -> None:
    node.send_signal(signal.SIGTERM)
    node.wait(120)


def short(i: int) -> str:
    """A 4-character index name, so a 10,000-name path fits the 64 KiB a URL path can hold."""
    digits = "0123456789abcdefghijklmnopqrstuvwxyz"
    return "w" + digits[i // 1296 % 36] + digits[i // 36 % 36] + digits[i % 36]


def create(i: int) -> int:
    # Index i holds 5 documents (more, in some); n is unique per document, g one of 4 groups; the indexes differ in
    # vocabulary, so a relevance weight differs with the statistics it is read against.
    status = 0
    for j in range(5 + (i % 3 if args.mode == "check" else 0)):
        n = i * 8 + j
        doc = {
            "n": n,
            "g": f"g{(n // 2 + i) % 4}",
            "tag": "x",
            "text": "alpha beta" if (i + j) % 3 == 0 else "beta " + "gamma " * (1 + i % 4),
        }
        status = max(status, call("PUT", f"/{short(i)}/d{j}", doc)[0])
    return status


def create_all(count: int) -> None:
    t0 = time.time()
    with concurrent.futures.ThreadPoolExecutor(8) as pool:
        statuses = list(pool.map(create, range(count)))
    assert all(s < 300 for s in statuses), sorted(set(statuses))
    print(f"created {count} indexes in {round(time.time() - t0)} s", flush=True)


def normalise(raw: bytes) -> object:
    body = json.loads(raw)
    hits = [(h.get("_id"), h.get("n"), h.get("g"), h.get("#weight"), h.get("#docid")) for h in body.get("hits", [])]
    return {
        "total": body.get("total"),
        "count": body.get("count"),
        "hits": hits,
        "aggregations": body.get("aggregations"),
    }


AGGS = {
    "by_g": {"_values": {"_field": "g"}, "_aggs": {"s": {"_sum": {"_field": "n"}}}},
    "all": {"_sum": {"_field": "n"}},
    "most": {"_max": {"_field": "n"}},
}
SEARCHES = {
    "sorted ascending": {"_query": {"tag": "x"}, "_sort": "n", "_limit": 12},
    "sorted descending window": {"_query": {"tag": "x"}, "_sort": "-n", "_offset": 3, "_limit": 9},
    "aggregations": {"_query": {"tag": "x"}, "_sort": "n", "_limit": 5, "_aggs": AGGS},
    "group filter": {"_query": {"g": "g1"}, "_sort": "-n", "_limit": 20, "_aggs": AGGS},
    "relevance": {"_query": {"text": "gamma"}, "_limit": 25},
    "relevance window": {"_query": {"text": "alpha beta"}, "_offset": 4, "_limit": 12},
    "relevance or": {"_query": {"_or": [{"text": "alpha"}, {"text": "gamma"}]}, "_limit": 30},
}


# Patterns over the indexes w000 to w00d (14 of them): all, a prefix, and an exclusion.
PATTERNS = {
    "_all": "/_all/",
    "prefix": "/w00*/",
    "exclusion": "/w*,-w000,-w00a/",
}


def check() -> None:
    k = args.k
    n = 3 * k + 2
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(DATA)
    node = start("create", "--concurrent-search-indexes", str(n))
    create_all(n)
    stop(node)
    results: dict[str, dict[str, object]] = {}
    for tag, batch in (("all", n + 5), ("batched", k)):
        node = start(tag, "--concurrent-search-indexes", str(batch))
        results[tag] = {}
        for width in (1, k - 1, k, k + 1, n):
            for label, body in SEARCHES.items():
                path = "/" + ",".join(short(i) for i in range(width)) + "/?comments=true"
                status, raw = call("SEARCH", path, body)
                results[tag][f"{label} over {width}"] = normalise(raw) if status == 200 else status
        # The same indexes named by pattern.
        for label, body in SEARCHES.items():
            for name, path in PATTERNS.items():
                status, raw = call("SEARCH", path + "?comments=true", body)
                results[tag][f"{label} by {name}"] = normalise(raw) if status == 200 else status
        peak = metric("xapiand_search_pinned_indexes_peak")
        print(f"{tag}: batch {batch}, peak indexes held open by searches {peak:.0f}")
        if peak > batch or peak < min(batch, n):
            failures.append(f"{tag} peak {peak}")
        stop(node)
    for key, whole in results["all"].items():
        ok = whole == results["batched"][key] and isinstance(whole, dict)
        print(f"{'ok  ' if ok else 'FAIL'} {key}: {whole['total'] if isinstance(whole, dict) else whole} matches")
        if not ok:
            failures.append(key)
    for name, pattern in PATTERNS.items():
        got = results["all"][f"relevance by {name}"]
        counted = len(got["hits"]) if isinstance(got, dict) else -1
        ok = isinstance(got, dict) and got["total"] is not None and got["total"] > 0 and counted > 0
        print(f"{'ok  ' if ok else 'FAIL'} {pattern}: {got['total'] if isinstance(got, dict) else got} matches")
        if not ok:
            failures.append(f"pattern {name}")
        if isinstance(got, dict) and name != "exclusion":
            same = results["all"][f"relevance over {n}"]
            if got != same:
                failures.append(f"pattern {name} differs from the name list")
    sample = results["all"][f"aggregations over {n}"]
    assert isinstance(sample, dict)
    print(f"sample: first hit {sample['hits'][0]}, aggregations {json.dumps(sample['aggregations'])[:200]}")
    if not sample["aggregations"] or sample["hits"][0][1] is None:
        failures.append("the sample has no aggregations or no fields: the comparison would be vacuous")
    cap = 7
    node = start("cap", "--max-search-indexes", str(cap), "--concurrent-search-indexes", "3")
    for width, want in ((cap, 200), (cap + 1, 400)):
        path = "/" + ",".join(short(i) for i in range(width)) + "/"
        status, raw = call("SEARCH", path, SEARCHES["sorted ascending"])
        ok = status == want and (want == 200 or b"--max-search-indexes" in raw)
        print(f"{'ok  ' if ok else 'FAIL'} the cap of {cap}: a search over {width} indexes -> {status}")
        if not ok:
            failures.append(f"cap {width}")
    # A pattern reads every entry of the data directory (14 indexes and the node's own), so at a cap of 7 each is over it,
    # whatever it matches: the cap bounds the walk's work, not only the names.
    for pattern in (
        "/w000*,w001*,w002*,w003*,w004*,w005*,w006*/",
        "/_all/",
        "/w00*/",
        "/_all,-w007,-w008,-w009,-w00a,-w00b,-w00c,-w00d/",
        "/*,-*/",
    ):
        status, raw = call("SEARCH", pattern, SEARCHES["sorted ascending"])
        ok = status == 400 and b"--max-search-indexes" in raw and b"directory entries" in raw
        print(f"{'ok  ' if ok else 'FAIL'} the cap of {cap}: {pattern} -> {status}")
        if not ok:
            failures.append(f"cap pattern {pattern}")
    stop(node)
    # With room for the walk the same patterns are served; the names are still counted (an index costs its own entry and its directory's).
    cap = 100
    node = start("cap-walk", "--max-search-indexes", str(cap), "--concurrent-search-indexes", "3")
    for pattern, want in (
        ("/w000*,w001*,w002*,w003*,w004*,w005*,w006*/", 200),
        ("/_all/", 200),
        ("/w00*/", 200),
        ("/_all,-w007,-w008,-w009,-w00a,-w00b,-w00c,-w00d/", 200),
        ("/*,-*/", 200),
    ):
        status, raw = call("SEARCH", pattern, SEARCHES["sorted ascending"])
        ok = status == want
        print(f"{'ok  ' if ok else 'FAIL'} the cap of {cap}: {pattern} -> {status}")
        if not ok:
            failures.append(f"cap-walk pattern {pattern}")
    # Indexes nest: `tenant` above `tenant/a`, and `tenant/a/x` below it; each pattern finds each at its own depth.
    for path in ("/tenant/a/d", "/tenant/d", "/tenant/a/x/d"):
        status, raw = call("PUT", path, {"n": 1})
        if status >= 300:
            failures.append(f"nesting: PUT {path} -> {status}")
    for pattern, want in (
        ("/tenant*/", 1),
        ("/tenant/*/", 1),
        ("/tenant/*/*/", 1),
        ("/tenant/**/", 3),
        ("/**/x/", 1),
        ("/ten%3Fnt/", 1),
        ("/tenant/[a]/", 1),
        ("/tenant/[!a]/", 0),
    ):
        status, raw = call("COUNT", pattern + "?q=n:1")
        total = json.loads(raw).get("total") if status == 200 else None
        ok = total == want
        print(f"{'ok  ' if ok else 'FAIL'} nesting: COUNT {pattern} -> {status} {total} (want {want})")
        if not ok:
            failures.append(f"nesting {pattern}")
    stop(node)


def metric(name: str) -> float:
    for line in call("GET", "/:metrics")[1].decode().splitlines():
        if line.startswith((name + " ", name + "{")):
            return float(line.rsplit(" ", 1)[1])
    return -1.0


def gate() -> None:
    count = args.indexes
    shutil.rmtree(OUT, ignore_errors=True)
    os.makedirs(DATA)
    node = start("create")
    create_all(count)
    stop(node)
    node = start("search", *(["--database-pool-size", str(args.pool)] if args.pool else []))
    rss0, open0 = metric("xapiand_resident_memory_bytes"), metric("xapiand_databases")
    body = {"_query": {"tag": "x"}, "_sort": "n", "_limit": 10, "_aggs": {"all": {"_sum": {"_field": "n"}}}}
    path = "/w*/"  # a pattern: a list of 10,000 names is most of a URL, 100,000 would not fit
    times = []
    total = None
    for run in range(args.runs + 1):
        t0 = time.perf_counter()
        status, raw = call("SEARCH", path, body)
        dt = (time.perf_counter() - t0) * 1000
        assert status == 200, (status, raw[:200])
        total = json.loads(raw).get("total")
        if run:  # the first run opens every index
            times.append(dt)
        else:
            rss1, open1 = metric("xapiand_resident_memory_bytes"), metric("xapiand_databases")
            per = (rss1 - rss0) / (open1 - open0) if open1 > open0 else float("nan")
            print(
                f"open indexes {open0:.0f} -> {open1:.0f}, resident {rss0 / 2**20:.1f} -> {rss1 / 2**20:.1f} MiB: "
                f"{per / 1024:.1f} KiB per open index (first search {dt:.0f} ms)",
                flush=True,
            )
    times.sort()
    p50 = statistics.median(times)
    p99 = times[min(len(times) - 1, int(len(times) * 0.99))]
    print(
        f"gate: {count} indexes, {args.runs} searches, total {total} (want {count * 5}): p50 {p50:.0f} ms, p99 {p99:.0f} ms, "
        f"peak pinned indexes {metric('xapiand_search_pinned_indexes_peak'):.0f}, now {metric('xapiand_search_pinned_indexes'):.0f}"
    )
    if total != count * 5:
        failures.append("total")
    stop(node)


check() if args.mode == "check" else gate()
print("PASS" if not failures else "FAIL: " + ", ".join(failures))
sys.exit(1 if failures else 0)
