#!/usr/bin/env python3
"""search_fanout_check.py BIN OUTDIR PORT: a search over many indexes (X2b R3).

A search keeps every index it reads open for its whole run, so one that names more shards than
--max-indexes-per-search answers 400 with the limit in the message; at the limit it is served (one hazard
record per index, none shared). Run it under a hold: it creates 1,011 one-shard indexes (about a minute).

  server A: --database-pool-size 1100 --max-indexes-per-search 1010  1,010 -> 200, 1,011 -> 400
  server B: the defaults (limit = the pool size, 200)                200 -> 200, 201 and 1,010 -> 400
"""
import concurrent.futures
import http.client
import json
import os
import shutil
import signal
import subprocess
import sys
import time

BIN, OUT, PORT = sys.argv[1], sys.argv[2], int(sys.argv[3])
N = 1011
DATA = os.path.join(OUT, "data")
os.makedirs(OUT, exist_ok=True)
shutil.rmtree(DATA, ignore_errors=True)
os.makedirs(DATA)
failures: list[str] = []


def call(c: http.client.HTTPConnection, method: str, path: str, body: object = None) -> tuple[int, bytes]:
    c.request(method, path, body=json.dumps(body) if body is not None else None, headers={"Content-Type": "application/json"})
    r = c.getresponse()
    return r.status, r.read()


def start(tag: str, *flags: str) -> subprocess.Popen[bytes]:
    node = subprocess.Popen(
        [BIN, "--port", str(PORT), "--name", "fan", "-D", DATA, "--processors", "1", "--shards", "1", *flags],
        stdout=open(os.path.join(OUT, f"node-{tag}.log"), "wb"),
        stderr=subprocess.STDOUT,
    )
    for _ in range(240):
        try:
            call(http.client.HTTPConnection("localhost", PORT, timeout=10), "GET", "/")
            return node
        except Exception:
            time.sleep(0.5)
    node.kill()
    raise SystemExit("the server did not start")


def stop(node: subprocess.Popen[bytes]) -> None:
    node.send_signal(signal.SIGTERM)
    node.wait(120)


def search(k: int) -> tuple[int, str]:
    c = http.client.HTTPConnection("localhost", PORT, timeout=300)
    path = "/" + ",".join(f"mi{i}" for i in range(k)) + "/"
    s, r = call(c, "SEARCH", path, {"_query": {"tag": "x"}, "_limit": 2000})
    return s, (str(json.loads(r).get("total")) if s == 200 else r.decode())


def expect(label: str, k: int, status: int, mention: str | None = None) -> None:
    s, text = search(k)
    ok = s == status and (mention is None or mention in text)
    print(f"{'ok  ' if ok else 'FAIL'} {label}: search over {k} indexes -> {s} {text[:140]}")
    if not ok:
        failures.append(label)


def create(i: int) -> int:
    c = http.client.HTTPConnection("localhost", PORT, timeout=300)
    return call(c, "PUT", f"/mi{i}/d", {"tag": "x"})[0]


node = start("a", "--database-pool-size", "1100", "--max-indexes-per-search", "1010")
t0 = time.time()
with concurrent.futures.ThreadPoolExecutor(8) as pool:
    statuses = list(pool.map(create, range(N)))
assert all(s < 300 for s in statuses), sorted(set(statuses))
print(f"created {N} indexes in {round(time.time() - t0)} s")
expect("at the limit", 1010, 200)
expect("limit + 1", 1011, 400, "1010")
stop(node)

node = start("b")
expect("default limit (the pool size)", 200, 200)
expect("default limit + 1", 201, 400, "200")
expect("the 1,010-index search", 1010, 400, "--max-indexes-per-search")
stop(node)

print("PASS" if not failures else "FAIL: " + ", ".join(failures))
sys.exit(1 if failures else 0)
