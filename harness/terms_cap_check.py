#!/usr/bin/env python3
"""terms_cap_check.py BIN -- a `_terms` over 200,000 distinct values holds a bounded number of buckets.

Starts BIN twice (one engine each, hard timeout), loads documents whose `m` (keyword) and `n` (integer) have
DISTINCT unique values and 10 hot values (3% to 6% of the documents each), and asks for the top 10 of each with
`_shard_size` 100. The answer must hold the true top 10 with counts within 1% (the cap makes counts approximate past
it), and the strata server's resident size must not grow by more than MAX_GROWTH_MB (default 40) while it answers (the
Xapian engine's is printed: its file-backed pages move the number too much to bound). Exit 1 on a wrong answer or
on growth past the limit.

  python3 harness/terms_cap_check.py .scratch/xapiand
"""
import collections
import http.client
import json
import os
import random
import shutil
import subprocess
import sys
import tempfile
import threading
import time

BIN = os.path.abspath(sys.argv[1])
BASE = int(os.environ.get("PORT", "18985"))
DISTINCT = int(os.environ.get("DISTINCT", "200000"))
MAX_GROWTH_MB = float(os.environ.get("MAX_GROWTH_MB", "40"))
SCRATCH = os.environ.get("CHECK_TMP") or tempfile.mkdtemp(prefix="capcheck_", dir=os.path.expanduser("~/.scratch"))


def call(port, method, path, body=None, ctype="application/json"):
    c = http.client.HTTPConnection("localhost", port, timeout=600)
    c.request(method, path, body=body, headers={"Content-Type": ctype})
    r = c.getresponse()
    return r.status, r.read()


def server_pid(timeout_pid):
    """The server is `timeout`'s child."""
    out = subprocess.run(["pgrep", "-P", str(timeout_pid)], capture_output=True, text=True).stdout.split()
    return int(out[0])


def rss_mb(pid):
    out = subprocess.run(["ps", "-o", "rss=", "-p", str(pid)], capture_output=True, text=True).stdout.strip()
    return int(out) / 1024 if out else 0.0


def peak_while(pid, fn):
    peak, stop = [rss_mb(pid)], threading.Event()

    def watch():
        while not stop.is_set():
            peak[0] = max(peak[0], rss_mb(pid))
            time.sleep(0.02)
    t = threading.Thread(target=watch)
    t.start()
    try:
        return fn(), peak
    finally:
        stop.set()
        t.join()


def documents():
    r = random.Random(5)
    hot = [("hot%d" % j, 1000000 + j) for j in range(10)]
    values = [h for j, h in enumerate(hot) for _ in range(DISTINCT // 20 + j * DISTINCT // 200)]
    values += [("tail%07d" % i, 2000000 + i) for i in range(DISTINCT)]
    r.shuffle(values)
    return [{"_id": "d%d" % i, "m": m, "n": n} for i, (m, n) in enumerate(values)], values


def main():
    docs, values = documents()
    body = "".join(json.dumps(d) + "\n" for d in docs)
    want = {"m": collections.Counter(v[0] for v in values).most_common(10), "n": collections.Counter(v[1] for v in values).most_common(10)}
    nodes, bad = {}, 0
    try:
        for k, engine in enumerate(("strata",)):
            port = BASE + k
            data = os.path.join(SCRATCH, "data_" + engine)
            shutil.rmtree(data, ignore_errors=True)
            nodes[engine] = subprocess.Popen(["timeout", "1800", BIN, "--port", str(port), "--name", "cap" + engine, "-D", data],
                                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            for _ in range(240):
                try:
                    if call(port, "GET", "/")[0] == 200:
                        break
                except OSError:
                    time.sleep(0.5)
            schema = {"_settings": {"number_of_shards": 5, "number_of_replicas": 0}, "_schema": {"_type": "object", "m": {"_type": "keyword"}, "n": {"_type": "integer"}}}
            assert call(port, "PUT", "/cap/", json.dumps(schema))[0] in (200, 201, 204)
            restored = call(port, "RESTORE", "/cap/?commit", body, "application/x-ndjson")
            assert restored[0] in (200, 201, 204), restored
            for field in ("m", "n"):
                query = json.dumps({"_query": "*", "_limit": 0, "_check_at_least": 1000, "_aggs": {"a": {"_terms": {"_field": field, "_limit": 10, "_shard_size": 100}}}})
                call(port, "SEARCH", "/cap/?volatile", json.dumps({"_query": "*", "_limit": 1}))
                pid = server_pid(nodes[engine].pid)
                before = rss_mb(pid)
                (status, data), peak = peak_while(pid, lambda: call(port, "SEARCH", "/cap/?volatile", query))
                grown = peak[0] - before
                got = [(str(b["_key"]), b["_doc_count"]) for b in json.loads(data).get("aggregations", {}).get("a", [])]
                truth = {str(k_): c for k_, c in want[field]}
                right = status == 200 and len(got) == 10 and all(k_ in truth and 0.99 * truth[k_] <= c <= truth[k_] for k_, c in got) \
                    and [c for _, c in got] == sorted((c for _, c in got), reverse=True)
                small = grown <= MAX_GROWTH_MB or engine != "strata"
                print("%-6s %s: status %d, top 10 %s, resident growth %.1f MB%s" % (engine, field, status, "right" if right else "WRONG " + str(got)[:200], grown, "" if small else " TOO MUCH"))
                bad += (not right) + (not small)
    finally:
        for node in nodes.values():
            node.terminate()
            node.wait()
        for engine in nodes:
            shutil.rmtree(os.path.join(SCRATCH, "data_" + engine), ignore_errors=True)
    sys.exit(1 if bad else 0)


main()
