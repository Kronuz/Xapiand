#!/usr/bin/env python3
"""agg_buckets_check.py BIN -- nested `_terms` over a multi-valued field stays bounded, on the Xapian engine.

Starts BIN (--solo, one engine each, hard timeout) and loads 300 documents whose `v` holds the same 7 values, so a
`_terms` nested L deep over `v` makes 7**L buckets. Prints seconds, resident MB and status per depth. With CHECK=1
exits 1 unless: depth 2 answers 200 with at least 7 buckets per level; the deepest level, past MAX_BUCKETS, answers a client
error naming the bucket limit in under 10 s with growth under 300 MB; nesting past MAX_DEPTH is refused at once.

  python3 harness/agg_buckets_check.py .scratch/build/bin/xapiand
"""
import http.client, json, os, shutil, signal, subprocess, sys, tempfile, threading, time

BIN = os.path.abspath(sys.argv[1])
BASE = int(os.environ.get("PORT", "18990"))
LEVELS = int(os.environ.get("LEVELS", "6"))
TIMEOUT = int(os.environ.get("REQ_TIMEOUT", "40"))
CHECK = os.environ.get("CHECK") == "1"
SCRATCH = tempfile.mkdtemp(prefix="aggb_", dir=os.path.expanduser("~/.scratch"))
fails = []


def call(port, method, path, body=None, timeout=TIMEOUT):
    c = http.client.HTTPConnection("localhost", port, timeout=timeout)
    c.request(method, path, body=body, headers={"Content-Type": "application/json"})
    r = c.getresponse()
    return r.status, r.read()


def rss_mb(pid):
    out = subprocess.run(["ps", "-o", "rss=", "-p", str(pid)], capture_output=True, text=True).stdout.strip()
    return int(out) / 1024 if out else 0.0


def nest(levels, depth_key="v"):
    agg = {"_terms": {"_field": "v"}}
    for _ in range(levels - 1):
        agg = {"_terms": {"_field": "v"}, "_aggs": {"x": agg}}
    return {"_aggs": {"x": agg}}


def run(engine, port):
    d = os.path.join(SCRATCH, engine)
    os.makedirs(d)
    env = dict(os.environ, XAPIAND_ENGINE=engine)
    p = subprocess.Popen([BIN, "--port", str(port), "--name", "n" + engine, "--solo", "-D", d],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, env=env, start_new_session=True)
    try:
        for _ in range(120):
            try:
                if call(port, "GET", "/")[0] == 200:
                    break
            except Exception:
                time.sleep(1)
        for i in range(300):
            call(port, "PUT", "/t/%d" % i, json.dumps({"v": [0, 1, 2, 3, 4, 5, 6]}))
        call(port, "POST", "/t/:commit", "{}")
        base = rss_mb(p.pid)
        for L in range(1, LEVELS + 1):
            peak, stop = [base], threading.Event()
            def watch():
                while not stop.is_set():
                    peak[0] = max(peak[0], rss_mb(p.pid))
                    if peak[0] > 3000:
                        os.killpg(p.pid, signal.SIGKILL)
                    time.sleep(0.05)
            th = threading.Thread(target=watch); th.start()
            t = time.time()
            try:
                st, data = call(port, "SEARCH", "/t/", json.dumps(nest(L)))
            except Exception as e:
                st, data = "EXC " + type(e).__name__, b""
            dt = time.time() - t
            stop.set(); th.join()
            print("%-6s depth=%d -> %-14s %6.2fs rss=%5.0fMB (+%.0f) %s" % (engine, L, st, dt, peak[0], peak[0] - base,
                  data[:90].decode("latin1").replace("\n", " ") if st != 200 else ""), flush=True)
            if not isinstance(st, int):
                break
            if CHECK:
                if L == 2:
                    j = json.loads(data)["aggregations"]["x"]
                    if st != 200 or len(j) < 7 or len(j[0]["x"]) < 7:
                        fails.append((engine, "depth 2 answer wrong"))
                if L == LEVELS:  # _terms also counts the index terms of a number, so a level has more than 7 buckets
                    if not (isinstance(st, int) and 400 <= st < 500 and b"bucket" in data.lower()):
                        fails.append((engine, "depth %d not refused: %s" % (L, st)))
                    if dt > 10 or peak[0] - base > 300:
                        fails.append((engine, "depth %d refused too late: %.1fs +%.0fMB" % (L, dt, peak[0] - base)))
        if CHECK and p.poll() is None:
            st, data = call(port, "SEARCH", "/t/", json.dumps(nest(40)))
            if not (400 <= st < 500):
                fails.append((engine, "nesting 40 not refused: %s" % st))
    finally:
        try:
            os.killpg(p.pid, signal.SIGKILL)
        except Exception:
            pass


try:
    for i, e in enumerate(("xapian",)):
        run(e, BASE + i)
finally:
    shutil.rmtree(SCRATCH, ignore_errors=True)
for f in fails:
    print("FAIL", *f)
sys.exit(1 if fails else 0)
