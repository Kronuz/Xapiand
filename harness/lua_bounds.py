#!/usr/bin/env python3
"""lua_bounds.py BINARY [--port N] [--loops 4] [--out FILE]

Phase 7c D10 / T10 on a live node: every Lua bound answers a 400 and the node keeps serving, and
a script that runs long shares its reactor.

1. Bounds. One document per case is PUT with a script that crosses a bound (an infinite loop, a
   runaway callback, an exponential pattern, a 1 GiB string, a pcall loop that tries to swallow the
   instruction cap); each must answer 400 inside the time limit, and a plain script must still
   answer 2xx and write its document after all of them.
2. Slices. A light GET is timed alone (the baseline), then while --loops (default 4) concurrent
   PUTs each run a long script (about 8 x 10^7 instructions, under the cap). The PUTs must succeed;
   the light GETs' p50, p99 and max are reported with the node's reactor-lag histogram deltas.

Exits non-zero when a bound answers anything but 400, a script that should finish does not, or a
light GET takes longer than --max-light-ms while the scripts run.
"""
import argparse
import http.client
import json
import os
import shutil
import statistics
import subprocess
import sys
import tempfile
import threading
import time


def request(port, method, path, body=None, timeout=60):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    try:
        data = json.dumps(body) if body is not None else None
        conn.request(method, path, data, {"Content-Type": "application/json"} if data else {})
        resp = conn.getresponse()
        return resp.status, resp.read()
    finally:
        conn.close()


def script_doc(body, **fields):
    return {**fields, "_script": {"_type": "script", "_lua": {"_body": body}}}


CASES = [
    ("infinite loop", "while true do end"),
    ("loop that catches the cap", "while true do pcall(function() while true do end end) end"),
    ("runaway gsub callback", "string.gsub('abc', '.', function() while true do end end)"),
    ("runaway __lt in a sort", "local mt = {__lt = function() while true do end end}; table.sort({setmetatable({}, mt), setmetatable({}, mt), setmetatable({}, mt)})"),
    ("runaway sort comparator", "table.sort({3, 2, 1}, function(a, b) while true do end end)"),
    ("exponential pattern", "string.rep('a', 200):find(string.rep('a*', 12) .. 'b')"),
    ("quadratic find", "string.find(string.rep('a', 20000), '.-b')"),
    ("pattern error caught in a loop", "local s = string.rep('a', 200); local p = string.rep('a*', 12) .. 'b'; while true do pcall(string.find, s, p) end"),
    ("1 GiB string", "local s = string.rep('x', 1024 * 1024 * 1024)"),
    ("table growth", "local t = {} for i = 1, 100000000 do t[i] = 'k' .. i end"),
]

LONG_SCRIPT = "local s = 0; for i = 1, 40000000 do s = s + i end; _doc.s = s"


def lag_buckets(port):
    status, text = request(port, "GET", "/:metrics")
    out = {}
    for line in text.decode().splitlines():
        if line.startswith("xapiand_reactor_lag_seconds_count{") or line.startswith("xapiand_reactor_lag_seconds_sum{"):
            name, value = line.rsplit(" ", 1)
            out[name] = float(value)
    return out


def percentile(values, q):
    values = sorted(values)
    return values[min(len(values) - 1, int(len(values) * q))] if values else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("--port", type=int, default=18961)
    ap.add_argument("--loops", type=int, default=4)
    ap.add_argument("--max-bound-secs", type=float, default=30.0)
    ap.add_argument("--max-light-ms", type=float, default=50.0)
    ap.add_argument("--out")
    args = ap.parse_args()

    tmp = tempfile.mkdtemp(prefix="lua_bounds_", dir=os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".scratch"))
    node = subprocess.Popen(
        [args.binary, "--port", str(args.port), "--name", "luanode", "-D", os.path.join(tmp, "data")],
        stdout=open(os.path.join(tmp, "node.log"), "w"), stderr=subprocess.STDOUT)
    failures = []
    report = {"bounds": [], "slices": {}}
    try:
        for _ in range(240):
            if node.poll() is not None:
                print("node exited early; see", os.path.join(tmp, "node.log"))
                return 2
            try:
                if request(args.port, "GET", "/", timeout=2)[0] == 200:
                    break
            except OSError:
                time.sleep(0.25)
        else:
            print("node did not come up")
            return 2

        status, _ = request(args.port, "PUT", "/lua_ok/doc?commit", script_doc("_doc.x = _doc.x * 2", x=21))
        print(f"plain script: {status}")
        if not 200 <= status < 300:
            failures.append(f"plain script answered {status}")

        for n, (name, body) in enumerate(CASES):
            t0 = time.time()
            status, text = request(args.port, "PUT", f"/lua_case{n}/doc?commit", script_doc(body, x=1), timeout=args.max_bound_secs)
            took = time.time() - t0
            ok = status == 400 and took < args.max_bound_secs
            print(f"{'ok  ' if ok else 'FAIL'} {name}: {status} in {took:.2f} s {text[:90].decode(errors='replace') if status != 400 else ''}")
            report["bounds"].append({"case": name, "status": status, "secs": round(took, 3)})
            if not ok:
                failures.append(f"{name}: {status} in {took:.1f} s")
            alive = request(args.port, "GET", "/", timeout=5)[0]
            if alive != 200:
                failures.append(f"node not serving after {name}")

        status, text = request(args.port, "GET", "/lua_ok/doc")
        x = json.loads(text).get("x") if status == 200 else None
        print(f"plain script's document after the bounds: x = {x}")
        if x != 42:
            failures.append(f"the plain script's document read back x = {x}")
        status, _ = request(args.port, "PUT", "/lua_ok/doc2?commit", script_doc("_doc.x = _doc.x * 2", x=5))
        if not 200 <= status < 300:
            failures.append(f"a plain script after the bounds answered {status}")

        # Slices: light GETs alone, then beside long scripts.
        def probe(stop, latencies):
            conn = http.client.HTTPConnection("127.0.0.1", args.port, timeout=30)
            while not stop.is_set():
                t0 = time.perf_counter()
                conn.request("GET", "/lua_ok/doc")
                conn.getresponse().read()
                latencies.append((time.perf_counter() - t0) * 1000)
                time.sleep(0.002)
            conn.close()

        def run_probe(duration, during=None):
            stop, latencies = threading.Event(), []
            thread = threading.Thread(target=probe, args=(stop, latencies))
            before = lag_buckets(args.port)
            thread.start()
            results = during() if during else (time.sleep(duration), None)[1]
            stop.set()
            thread.join()
            after = lag_buckets(args.port)
            return latencies, results, {k: after[k] - before.get(k, 0) for k in after}

        base, _, base_lag = run_probe(3.0)
        statuses = [None] * args.loops
        timings = [0.0] * args.loops

        def worker(i):
            t0 = time.time()
            statuses[i] = request(args.port, "PUT", f"/lua_long{i}/doc?commit", script_doc(LONG_SCRIPT, x=1), timeout=120)[0]
            timings[i] = time.time() - t0

        def during():
            threads = [threading.Thread(target=worker, args=(i,)) for i in range(args.loops)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()

        loaded, _, loaded_lag = run_probe(0, during)
        for name, series in (("alone", base), (f"beside {args.loops} long scripts", loaded)):
            print(f"light GET {name}: n={len(series)} p50={percentile(series, .5):.2f} ms p99={percentile(series, .99):.2f} ms max={max(series):.2f} ms")
        print(f"the {args.loops} long scripts answered {statuses} in {[round(t, 2) for t in timings]} s")
        report["slices"] = {
            "alone": {"n": len(base), "p50_ms": percentile(base, .5), "p99_ms": percentile(base, .99), "max_ms": max(base)},
            "beside": {"n": len(loaded), "p50_ms": percentile(loaded, .5), "p99_ms": percentile(loaded, .99), "max_ms": max(loaded)},
            "script_statuses": statuses, "script_secs": timings, "lag_alone": base_lag, "lag_beside": loaded_lag,
        }
        if any(not 200 <= s < 300 for s in statuses):
            failures.append(f"long scripts answered {statuses}")
        if max(loaded) > args.max_light_ms:
            failures.append(f"a light GET took {max(loaded):.1f} ms while the scripts ran (limit {args.max_light_ms})")
    finally:
        node.terminate()
        try:
            node.wait(timeout=20)
        except subprocess.TimeoutExpired:
            node.kill()
        if args.out:
            with open(args.out, "w") as fh:
                json.dump(report, fh, indent=1)
        shutil.rmtree(tmp, ignore_errors=True)
    if failures:
        print("FAILED:", *failures, sep="\n  ")
        return 1
    print("PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
