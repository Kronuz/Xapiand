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

"""lua_bounds.py BINARY [--port N] [--loops 4] [--out FILE]

Phase 7c D10 / T10 on a live node: every Lua bound answers a 400 and the node keeps serving, and
a script that runs long shares its reactor.

1. Bounds. One document per case is PUT with a script that crosses a bound (an infinite loop, a
   runaway callback, an exponential pattern, a 1 GiB string, a pcall loop that tries to swallow the
   instruction cap, a to-be-closed handler that replaces the bound's error, a quadratic plain
   find, a literal-run or class pattern, a loop of library calls over a large string, a vararg
   chain, a result past its byte or value budget); each must answer its status (400, or 413 for a
   result) inside the time limit, and a plain script must still answer 2xx and write its document
   after all of them.
2. Swallowers. Ten requests whose scripts try to outlive the cap through a closing variable run
   at once: fresh GETs must keep being answered, every request must end 400, and the node must stop
   gracefully when asked.
3. Flood. --flood-clients (default 16) clients re-send a request whose script makes one worst-case call
   (a quadratic pattern over 1 MiB, which ends at the per-call cap) for six seconds; every request must
   answer 400 and the light GETs' p99 must stay under --max-flood-p99-ms.
4. Foreign scripts. A local script named like a foreign reference must neither replace the cached
   foreign script nor stand in for it, in either order, and a rewritten foreign script must take
   effect.
5. Slices. A light GET is timed alone (the baseline), then while --loops (default 4) concurrent
   PUTs each run a long script (about 8 x 10^7 instructions, under the cap), and again while --loops
   concurrent PUTs each loop library calls over a large string. The PUTs must succeed; the light
   GETs' p50, p99 and max are reported with the node's reactor-lag histogram deltas.

Exits non-zero when a bound answers anything but its status, a script that should finish does not,
or a light GET takes longer than --max-light-ms while the scripts run.
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


SWALLOW_LATE = (
    "while true do {call}(function() local mt = {{}} local t = setmetatable({{}}, mt) "
    "mt.__close = function() error('swallowed') end local x <close> = t while true do end end{handler}) end"
)

# (name, script body, expected status)
CASES = [
    ("infinite loop", "while true do end", 400),
    ("loop that catches the cap", "while true do pcall(function() while true do end end) end", 400),
    ("closing variable swallows the cap (pcall)", SWALLOW_LATE.format(call="pcall", handler=""), 400),
    ("closing variable swallows the cap (xpcall)", SWALLOW_LATE.format(call="xpcall", handler=", function(m) return m end"), 400),
    ("closing variable declared with a close metamethod", "local x <close> = setmetatable({}, {__close = function() end})", 400),
    ("runaway gsub callback", "string.gsub('abc', '.', function() while true do end end)", 400),
    ("runaway __lt in a sort", "local mt = {__lt = function() while true do end end}; table.sort({setmetatable({}, mt), setmetatable({}, mt), setmetatable({}, mt)})", 400),
    ("runaway sort comparator", "table.sort({3, 2, 1}, function(a, b) while true do end end)", 400),
    ("exponential pattern", "string.rep('a', 200):find(string.rep('a*', 12) .. 'b')", 400),
    ("quadratic find", "string.find(string.rep('a', 20000), '.-b')", 400),
    ("pattern error caught in a loop", "local s = string.rep('a', 200); local p = string.rep('a*', 12) .. 'b'; while true do pcall(string.find, s, p) end", 400),
    ("quadratic plain find, 1 MiB in 512 KiB", "string.rep('a', 1048576):find(string.rep('a', 524288) .. 'b', 1, true)", 400),
    ("literal-run pattern, 128 KiB in 64 KiB", "string.rep('a', 131072):find(string.rep('a', 65536) .. '.b')", 400),
    ("bracket-class pattern, 128 KiB in 64 KiB", "string.rep('a', 131072):find('[' .. string.rep('x', 65536) .. ']')", 400),
    ("loop of plain finds over 31 MiB", "local s = string.rep('a', 31 * 1048576); for i = 1, 100000 do s:find('b', 1, true) end", 400),
    ("loop of utf8.len over 16 MiB", "local s = string.rep('a', 16 * 1048576); for i = 1, 100000 do utf8.len(s) end", 400),
    ("loop of upper over 16 MiB", "local s = string.rep('a', 16 * 1048576); for i = 1, 100000 do local _ = s:upper() end", 400),
    ("loop of string equality over 15 MiB", "local a = string.rep('a', 15 * 1048576); local b = string.rep('a', 15 * 1048576); for i = 1, 100000 do local _ = (a == b) end", 400),
    ("loop of table.concat over 2M strings", "local t = {} for i = 1, 2000000 do t[i] = 'ab' end for i = 1, 100000 do table.concat(t) end", 400),
    ("loop of front inserts", "local t = {} for i = 1, 2000000 do t[i] = i end for i = 1, 100000 do table.insert(t, 1, i) end", 400),
    ("native-sized sort of 1M numbers", "local t = {} for i = 1, 1000000 do t[i] = (i * 7919) % 1000003 end for i = 1, 100 do table.sort(t) end", 400),
    ("quadratic pattern over a 1 MiB subject (the per-call cap)", "string.rep('a', 1048576):find('.-b')", 400),
    ("upper over 8 MiB (refused before it runs)", "local _ = string.rep('a', 8 * 1048576):upper()", 400),
    ("gsub over 8 MiB (the sum of its charges passes the per-call cap)", "local _ = string.rep('a', 8 * 1048576):gsub('a', 'b')", 400),
    ("vararg chain", "local function f(...) return f(1, ...) end f()", 400),
    ("1 GiB string", "local s = string.rep('x', 1024 * 1024 * 1024)", 400),
    ("table growth", "local t = {} for i = 1, 100000000 do t[i] = 'k' .. i end", 400),
    ("script raises an error", "error('boom')", 400),
    ("result: one 1 MiB string shared 64 times", "local s = string.rep('x', 1048576) local t = {} for i = 1, 64 do t[i] = s end _doc.t = t", 413),
    ("result: one small object shared 500,000 times", "local o = {a = 1, b = 2} local t = {} for i = 1, 500000 do t[i] = o end _doc.t = t", 413),
    ("1,000,000 distinct short strings (the memory cap comes first)", "local t = {} for i = 1, 1000000 do t[i] = 'v' .. i end _doc.t = t", 400),
    ("result: 400,000 distinct short strings", "local t = {} for i = 1, 400000 do t[i] = 'v' .. i end _doc.t = t", 413),
]

# Finish with 2xx: a weak-table chain once stalled the collector for a minute; print is capped.
FINISH_CASES = [
    ("ephemeron chain (the table is never weak)", "local t = setmetatable({}, {}) local keys = {} for i = 1, 160000 do keys[i] = {} end for i = 1, 159999 do t[keys[i]] = keys[i + 1] end keys = {keys[1]} for i = 1, 1000000 do local j = {i} end"),
    ("print flood", "local big = string.rep('pppppppp', 1048576) for i = 1, 20 do print(big) end for i = 1, 100000 do print('line', i) end"),
]

LIB_LOOP = "local s = string.rep('abcdefgh', 1048576); for i = 1, 150 do s:find('b', 1, true) end; local u = 0; for i = 1, 100 do u = u + #s:sub(1, 1000000):upper() end; _doc.u = u"

# X4-R1: what a client can re-send as fast as the 400 returns; each makes one uninterruptible call that runs to the per-call cap.
FLOOD_SCRIPT = "local s = string.rep('a', 1048576) s:find('.-b')"

LONG_SCRIPT = "local s = 0; for i = 1, 40000000 do s = s + i end; _doc.s = s"


def foreign_scripts_check(binary, port, tmp):
    """A local script named like a foreign reference neither replaces nor stands in for the cached foreign script, and a
    rewritten foreign script takes effect. One processor is one reactor, so every request reaches the same (per-thread) cache."""
    failures = []
    node = subprocess.Popen([binary, "--port", str(port), "--name", "luaforeign", "--processors", "1", "-D", os.path.join(tmp, "data2")],
                            stdout=open(os.path.join(tmp, "node2.log"), "w"), stderr=subprocess.STDOUT)
    try:
        for _ in range(240):
            try:
                if request(port, "GET", "/", timeout=2)[0] == 200:
                    break
            except OSError:
                time.sleep(0.25)
        else:
            return ["the foreign-script node did not come up"]

        def put(path, body):
            return request(port, "PUT", path, body)[0]

        def owner(index, doc_id):
            status, text = request(port, "GET", f"/{index}/{doc_id}")
            return json.loads(text).get("owner") if status == 200 else status

        def local(name, body, **fields):
            return {**fields, "_script": {"_type": "script", "_lua": {"_name": name, "_body": body}}}

        def foreign(ref, **fields):
            return {**fields, "_script": {"_type": "foreign/object", "_foreign": ref}}

        put("/vault/multiplier?commit", {"_recurse": False, "script": {"_lua": {"_body": "_doc.owner = 'legit'"}}})
        for i in range(4):
            put(f"/victim/v{i}?commit", foreign("vault/multiplier", n=i))
        for i in range(4):
            put(f"/attacker/a{i}?commit", local("vault/multiplier", "_doc.owner = 'EVIL'", n=i))
        for i in range(4):
            put(f"/victim/w{i}?commit", foreign("vault/multiplier", n=i))
        seen = [owner("victim", f"w{i}") for i in range(4)]
        print(f"foreign script after a local one took its name: {seen}")
        if seen != ["legit"] * 4:
            failures.append(f"a local script replaced the foreign one: {seen}")
        for i in range(4):
            put(f"/attacker/b{i}?commit", local("vault/other", "_doc.owner = 'EVIL'", n=i))
        put("/vault/other?commit", {"_recurse": False, "script": {"_lua": {"_body": "_doc.owner = 'legit-other'"}}})
        for i in range(4):
            put(f"/victim/x{i}?commit", foreign("vault/other", n=i))
        seen = [owner("victim", f"x{i}") for i in range(4)]
        print(f"foreign script after a local one pre-seeded its name: {seen}")
        if seen != ["legit-other"] * 4:
            failures.append(f"a local script stood in for the foreign one: {seen}")
        put("/vault/multiplier?commit", {"_recurse": False, "script": {"_lua": {"_body": "_doc.owner = 'rewritten'"}}})
        time.sleep(1.5)   # past the cache's trust (one second)
        for i in range(4):
            put(f"/victim/z{i}?commit", foreign("vault/multiplier", n=i))
        seen = [owner("victim", f"z{i}") for i in range(4)]
        print(f"foreign script after its document was rewritten: {seen}")
        if seen != ["rewritten"] * 4:
            failures.append(f"a rewritten foreign script did not take effect: {seen}")
    finally:
        node.terminate()
        try:
            node.wait(timeout=20)
        except subprocess.TimeoutExpired:
            node.kill()
    return failures


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
    ap.add_argument("--flood-clients", type=int, default=16)
    ap.add_argument("--max-flood-p99-ms", type=float, default=100.0)
    ap.add_argument("--out")
    args = ap.parse_args()

    scratch = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), ".scratch")
    os.makedirs(scratch, exist_ok=True)
    tmp = tempfile.mkdtemp(prefix="lua_bounds_", dir=scratch)
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

        for n, (name, body, want) in enumerate(CASES):
            t0 = time.time()
            status, text = request(args.port, "PUT", f"/lua_case{n}/doc?commit", script_doc(body, x=1), timeout=args.max_bound_secs)
            took = time.time() - t0
            ok = status == want and took < args.max_bound_secs
            print(f"{'ok  ' if ok else 'FAIL'} {name}: {status} in {took:.2f} s {text[:90].decode(errors='replace') if status != want else ''}")
            report["bounds"].append({"case": name, "status": status, "secs": round(took, 3)})
            if not ok:
                failures.append(f"{name}: {status} (wanted {want}) in {took:.1f} s")
            alive = request(args.port, "GET", "/", timeout=5)[0]
            if alive != 200:
                failures.append(f"node not serving after {name}")

        log_before = os.path.getsize(os.path.join(tmp, "node.log"))
        for n, (name, body) in enumerate(FINISH_CASES):
            t0 = time.time()
            status, text = request(args.port, "PUT", f"/lua_finish{n}/doc?commit", script_doc(body, x=1), timeout=args.max_bound_secs)
            took = time.time() - t0
            ok = 200 <= status < 300 and took < args.max_bound_secs
            print(f"{'ok  ' if ok else 'FAIL'} {name}: {status} in {took:.2f} s")
            report["bounds"].append({"case": name, "status": status, "secs": round(took, 3)})
            if not ok:
                failures.append(f"{name}: {status} in {took:.1f} s")
        log_grown = os.path.getsize(os.path.join(tmp, "node.log")) - log_before
        print(f"log written by the print flood and the script errors: {log_grown} bytes")
        if log_grown > 512 * 1024:
            failures.append(f"the log grew {log_grown} bytes")

        # Ten swallowers at once, the shape that used to wedge every reactor.
        # Ten share the reactors, so each may wait for others' bounds: four bounds' time each.
        swallow_statuses = [None] * 10
        swallow_secs = [None] * 10

        def swallower(i):
            t0 = time.perf_counter()
            swallow_statuses[i] = request(args.port, "PUT", f"/lua_swallow{i}/doc?commit", script_doc(SWALLOW_LATE.format(call="pcall", handler=""), x=1), timeout=max(120, 4 * args.max_bound_secs))[0]
            swallow_secs[i] = round(time.perf_counter() - t0, 1)

        threads = [threading.Thread(target=swallower, args=(i,)) for i in range(10)]
        for t in threads:
            t.start()
        answered = 0
        for _ in range(20):
            try:
                answered += request(args.port, "GET", "/", timeout=2)[0] == 200
            except OSError:
                pass
        for t in threads:
            t.join()
        print(f"10 swallowing requests at once: fresh GETs answered {answered}/20; the requests answered {swallow_statuses} in {swallow_secs} s")
        if answered != 20:
            failures.append(f"only {answered} of 20 GETs were answered beside ten swallowing requests")
        if swallow_statuses != [400] * 10:
            failures.append(f"the swallowing requests answered {swallow_statuses}")

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

        lib_statuses = [None] * args.loops
        lib_timings = [0.0] * args.loops

        def lib_worker(i):
            t0 = time.time()
            lib_statuses[i] = request(args.port, "PUT", f"/lua_lib{i}/doc?commit", script_doc(LIB_LOOP, x=1), timeout=120)[0]
            lib_timings[i] = time.time() - t0

        def lib_during():
            threads = [threading.Thread(target=lib_worker, args=(i,)) for i in range(args.loops)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()

        lib_loaded, _, lib_lag = run_probe(0, lib_during)
        print(f"light GET beside {args.loops} library-call loops: n={len(lib_loaded)} p50={percentile(lib_loaded, .5):.2f} ms p99={percentile(lib_loaded, .99):.2f} ms max={max(lib_loaded):.2f} ms")
        print(f"the {args.loops} library-call loops answered {lib_statuses} in {[round(t, 2) for t in lib_timings]} s")
        report["slices"]["library_loops"] = {"n": len(lib_loaded), "p50_ms": percentile(lib_loaded, .5), "p99_ms": percentile(lib_loaded, .99),
                                             "max_ms": max(lib_loaded), "script_statuses": lib_statuses, "script_secs": lib_timings, "lag": lib_lag}
        if any(not 200 <= s < 300 for s in lib_statuses):
            failures.append(f"library-call loops answered {lib_statuses}")
        if max(lib_loaded) > args.max_light_ms:
            failures.append(f"a light GET took {max(lib_loaded):.1f} ms while library-call loops ran (limit {args.max_light_ms})")
        # X4-R1: clients re-sending a worst-case single call as fast as it answers. Every call ends at the per-call cap, so the
        # light GETs' p99 is bounded by the cap (about 20 ms) plus the host's load, not by the whole-call cap.
        flood_statuses = []
        flood_stop = time.time() + 6.0

        def flooder(i):
            while time.time() < flood_stop:
                try:
                    flood_statuses.append(request(args.port, "PUT", f"/lua_flood{i}/doc?commit", script_doc(FLOOD_SCRIPT, x=1), timeout=60)[0])
                except OSError:
                    flood_statuses.append(None)

        def flood_during():
            threads = [threading.Thread(target=flooder, args=(i,)) for i in range(args.flood_clients)]
            for t in threads:
                t.start()
            for t in threads:
                t.join()

        flooded, _, flood_lag = run_probe(0, flood_during)
        print(f"light GET beside {args.flood_clients} clients flooding the per-call cap: n={len(flooded)} p50={percentile(flooded, .5):.2f} ms p99={percentile(flooded, .99):.2f} ms max={max(flooded):.2f} ms; "
              f"{len(flood_statuses)} requests answered, {sorted(set(flood_statuses), key=str)}")
        report["slices"]["flood"] = {"clients": args.flood_clients, "n": len(flooded), "p50_ms": percentile(flooded, .5), "p99_ms": percentile(flooded, .99),
                                     "max_ms": max(flooded), "requests": len(flood_statuses), "lag": flood_lag}
        if set(flood_statuses) != {400}:
            failures.append(f"the flooding requests answered {sorted(set(flood_statuses), key=str)}")
        if percentile(flooded, .99) > args.max_flood_p99_ms:
            failures.append(f"light GET p99 {percentile(flooded, .99):.1f} ms beside the flood (limit {args.max_flood_p99_ms})")
        node.terminate()
        try:
            node.wait(timeout=20)
            print("graceful stop: the node exited")
        except subprocess.TimeoutExpired:
            failures.append("the node did not stop within 20 s of SIGTERM")

        failures.extend(foreign_scripts_check(args.binary, args.port + 1, tmp))
    finally:
        if node.poll() is None:
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
