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

# Driver of bulk_load.sh (see there). One engine process per trial, polled for RSS, killed on every exit.
import argparse, atexit, hashlib, http.client, json, os, platform, shutil, signal, statistics, subprocess, sys, tarfile, threading, time, urllib.request

EXPECT = {"accounts": 1000, "logs": 14005, "shakespeare": 111396}
ENV = os.environ
SCRATCH = ENV["BULK_SCRATCH"]
ap = argparse.ArgumentParser()
ap.add_argument("arm", choices=["xapiand", "xapiand-full", "es", "es-chunked"])
ap.add_argument("--trials", type=int, default=5)
ap.add_argument("--datasets", default="accounts,logs,shakespeare")
ap.add_argument("--bin", default=os.path.join(os.getcwd(), "build/bin/xapiand"))
ap.add_argument("--out", default=None)
ap.add_argument("--chunk", type=int, default=5000)
ap.add_argument("--port", type=int, default=0)
ap.add_argument("--fetch-only", action="store_true", help="download, verify and extract the pinned tarball, then stop")
a = ap.parse_args()
ES = a.arm.startswith("es")
procs = []

def reap():
    for p in procs:
        try: os.killpg(p.pid, signal.SIGKILL)
        except OSError: pass
        try: p.wait(timeout=10)
        except Exception: pass
atexit.register(reap)
for s in (signal.SIGTERM, signal.SIGINT, signal.SIGHUP):
    signal.signal(s, lambda *_: sys.exit(143))

def free_port():
    import socket
    s = socket.socket(); s.bind(("127.0.0.1", 0)); p = s.getsockname()[1]; s.close(); return p

def sh(*cmd, **kw): return subprocess.run(cmd, check=True, **kw)

def es_home():
    cache = os.path.join(SCRATCH, "cache"); os.makedirs(cache, exist_ok=True)
    tgz = os.path.join(cache, os.path.basename(ENV["ES_URL"]))
    home = os.path.join(cache, f"elasticsearch-{ENV['ES_VERSION']}")
    if os.path.isdir(home): return home
    if not os.path.exists(tgz):
        print(f"downloading {ENV['ES_URL']}", flush=True)
        urllib.request.urlretrieve(ENV["ES_URL"], tgz + ".part"); os.rename(tgz + ".part", tgz)
    h = hashlib.sha512()
    with open(tgz, "rb") as f:
        for b in iter(lambda: f.read(1 << 22), b""): h.update(b)
    if h.hexdigest() != ENV["ES_SHA512"]: raise SystemExit(f"SHA-512 mismatch for {tgz}: {h.hexdigest()}")
    tmp = home + ".part"; shutil.rmtree(tmp, ignore_errors=True)
    with tarfile.open(tgz) as t: t.extractall(tmp, filter="data")
    os.rename(os.path.join(tmp, f"elasticsearch-{ENV['ES_VERSION']}"), home); shutil.rmtree(tmp)
    return home

def datasets():
    d = os.path.join(SCRATCH, "data"); os.makedirs(d, exist_ok=True)
    out = {}
    for n in a.datasets.split(","):
        if ES:
            src, dst = os.path.join(ENV["BULK_DATASETS"], f"{n}-2.json.zst"), os.path.join(d, f"{n}-2.json")
        else:
            src, dst = os.path.join(ENV["BULK_DATASETS"], f"es-{n}.msgpack.zst"), os.path.join(d, f"es-{n}.msgpack")
        if not os.path.exists(dst): sh("zstd", "-dqf", src, "-o", dst)
        out[n] = dst
    return out

def es_bodies(path):
    # action lines are rebuilt as {"index":{"_id":X}}: Elasticsearch 9 has no _type and the index is in the URL
    lines = open(path, "rb").read().split(b"\n"); docs = []
    for i in range(0, len(lines) - 1, 2):
        act = json.loads(lines[i])["index"]
        m = {"_id": str(act["_id"])} if "_id" in act else {}
        docs.append(json.dumps({"index": m}).encode() + b"\n" + lines[i + 1] + b"\n")
    return docs

def rss_sum(pgid):
    out = subprocess.run(["ps", "-ax", "-o", "pgid=,rss="], capture_output=True, text=True).stdout
    return sum(int(r) for g, r in (l.split() for l in out.splitlines() if l.strip()) if int(g) == pgid)

class Peak(threading.Thread):
    def __init__(s, pgid): super().__init__(daemon=True); s.pgid, s.peak, s.stop = pgid, 0, False
    def run(s):
        while not s.stop: s.peak = max(s.peak, rss_sum(s.pgid)); time.sleep(0.02)

def call(port, method, path, body=None, ct="application/json", timeout=900):
    c = http.client.HTTPConnection("127.0.0.1", port, timeout=timeout)
    c.request(method, path, body=body, headers={"Content-Type": ct})
    r = c.getresponse(); data = r.read(); c.close()
    return r.status, data

def wait_up(port, p, path, bound=240):
    for _ in range(bound * 2):
        if p.poll() is not None: raise SystemExit(f"engine exited {p.returncode} before it answered")
        try:
            if call(port, "GET", path, timeout=5)[0] == 200: return
        except Exception: pass
        time.sleep(0.5)
    raise SystemExit("engine did not come up")

def start(root, port):
    shutil.rmtree(root, ignore_errors=True); os.makedirs(root)
    if ES:
        home = es_home(); pid = os.path.join(root, "pid")
        cmd = [os.path.join(home, "bin/elasticsearch"), "-p", pid, f"-Epath.data={root}/data", f"-Epath.logs={root}/logs",
               f"-Ehttp.port={port}", "-Ediscovery.type=single-node", "-Expack.security.enabled=false",
               "-Eingest.geoip.downloader.enabled=false"]
        env = {k: v for k, v in ENV.items() if k not in ("ES_JAVA_HOME", "JAVA_HOME")}
        env["ES_PATH_CONF"] = os.path.join(root, "conf"); shutil.copytree(os.path.join(home, "config"), env["ES_PATH_CONF"])
        data = os.path.join(root, "data")
    else:
        data = os.path.join(root, "data"); os.makedirs(data)
        cmd = [a.bin, "--port", str(port), "--name", "bulk", "-D", data, "--shards", "1"]
        if a.arm == "xapiand-full": cmd.append("--full-flush-ordering")
        env = dict(ENV)
    p = subprocess.Popen(cmd, env=env, stdout=open(os.path.join(root, "out.log"), "wb"), stderr=subprocess.STDOUT, start_new_session=True)
    procs.append(p)
    wait_up(port, p, "/_cluster/health?wait_for_status=green&timeout=60s" if ES else "/")
    return p, data

def stop(p):
    try: os.killpg(p.pid, signal.SIGTERM)
    except OSError: pass
    try: p.wait(timeout=60)
    except subprocess.TimeoutExpired: pass
    reap(); procs.clear()

def du(path): return int(subprocess.check_output(["du", "-sk", path], text=True).split()[0]) * 1024

def trial(name, path, bodies, n):
    port = a.port or free_port(); root = os.path.join(SCRATCH, "run", a.arm)
    p, data = start(root, port)
    try:
        base = rss_sum(p.pid); pk = Peak(p.pid); pk.start()
        idx = name
        if ES:
            st, b = call(port, "PUT", f"/{idx}", json.dumps({"settings": {"number_of_shards": 1, "number_of_replicas": 0}}).encode())
            assert st == 200, (st, b)
            reqs = [("/%s/_bulk?refresh=wait_for" % idx, b"".join(bodies))] if a.arm == "es" else [
                ("/%s/_bulk%s" % (idx, "?refresh=wait_for" if j + a.chunk >= len(bodies) else ""), b"".join(bodies[j:j + a.chunk]))
                for j in range(0, len(bodies), a.chunk)]
            t = time.perf_counter()
            for u, body in reqs:
                st, b = call(port, "POST", u, body, "application/x-ndjson")
                assert st == 200 and not json.loads(b)["errors"], (st, b[:300])
            e = time.perf_counter() - t
            st, b = call(port, "GET", f"/{idx}/_count"); got = json.loads(b)["count"]
        else:
            body = open(path, "rb").read()
            t = time.perf_counter()
            st, b = call(port, "RESTORE", f"/{idx}/?commit", body, "application/x-msgpack")
            e = time.perf_counter() - t
            assert st in (200, 201, 204), (st, b[:300])
            st, b = call(port, "GET", f"/{idx}/:count"); got = json.loads(b).get("total", -1)
        pk.stop = True; pk.join()
        assert got == n, f"{name}: counted {got}, wanted {n}"
        size = du(data)
        r = {"seconds": e, "docs_per_s": n / e, "peak_rss_mb": pk.peak / 1024, "idle_rss_mb": base / 1024, "disk_mb": size / 2**20}
        print(f"  {a.arm} {name} trial: {e:.3f} s  {n / e:,.0f} docs/s  peak RSS {r['peak_rss_mb']:.0f} MB  disk {r['disk_mb']:.1f} MB", flush=True)
        return r
    finally:
        stop(p)

def med(rows, k): return statistics.median(r[k] for r in rows)

if a.fetch_only:
    print(es_home()); sys.exit(0)
ds = datasets(); res = {"arm": a.arm, "host": platform.platform(), "trials": a.trials, "bin": None if ES else a.bin,
                        "es_version": ENV["ES_VERSION"] if ES else None, "datasets": {}}
if not ES: print(sh(a.bin, "--version", capture_output=True, text=True).stdout.splitlines()[0], flush=True)
for name, path in ds.items():
    n = EXPECT[name]; bodies = es_bodies(path) if ES else None
    assert not ES or len(bodies) == n, (name, len(bodies))
    rows = [trial(name, path, bodies, n) for _ in range(a.trials)]
    ss = sorted(r["seconds"] for r in rows)
    res["datasets"][name] = {"docs": n, "trials": rows, "median_s": statistics.median(ss), "min_s": ss[0], "max_s": ss[-1],
                             "docs_per_s": n / statistics.median(ss), "peak_rss_mb": med(rows, "peak_rss_mb"), "disk_mb": med(rows, "disk_mb")}
    d = res["datasets"][name]
    print(f"{a.arm} {name}: median {d['median_s']:.3f} s (min {d['min_s']:.3f}, max {d['max_s']:.3f}) = {d['docs_per_s']:,.0f} docs/s, "
          f"peak RSS {d['peak_rss_mb']:.0f} MB, disk {d['disk_mb']:.1f} MB", flush=True)
if a.out: json.dump(res, open(a.out, "w"), indent=1)
