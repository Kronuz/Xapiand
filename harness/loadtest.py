#!/usr/bin/env python3
"""
loadtest.py -- a dependency-free before/after performance driver for Xapiand.

Two phases against a running node:
  1. index:  bulk-RESTORE a dataset (replicated to a target size), measure docs/sec
             and on-disk bytes (the compression story).
  2. query:  replay a validated query mix at a fixed concurrency for a duration,
             measure QPS and latency percentiles.

Outputs a JSON result. Pair two results (baseline vs ours) with perfdiff.py to get
an "improving / regressing / within-noise" verdict. Queries are validated against
the accounts dataset; keep them returning hits so we time real work, not errors.

  python3 loadtest.py --target localhost:8880 --dataset docs/assets/accounts.ndjson \
      --replicate 20 --concurrency 16 --duration 8 --trials 3 \
      --datadir /tmp/xapiand_x --out harness/results/run.json
"""
import argparse
import http.client
import json
import os
import statistics
import sys
import time

# Validated SEARCH bodies (against the accounts dataset). Mix of query types.
QUERY_MIX = [
    ("match_all",   {}),
    ("term_gender", {"_query": {"gender": "male"}}),
    ("nested_name", {"_query": {"name.firstName": "Michael"}}),
    ("range_age",   {"_query": {"age": {"_in": {"_range": {"_from": 20, "_to": 30}}}}}),
    ("range_bal",   {"_query": {"balance": {"_in": {"_range": {"_from": 1000, "_to": 3000}}}}}),
    ("term_state",  {"_query": {"contact.state": "California"}}),
    ("bool_and",    {"_query": {"_and": [{"gender": "male"},
                                          {"age": {"_in": {"_range": {"_from": 25, "_to": 40}}}}]}}),
]


def connect(target):
    return http.client.HTTPConnection(target, timeout=30)


def request(conn, method, path, body=None, headers=None):
    conn.request(method, path, body=body, headers=headers or {})
    resp = conn.getresponse()
    data = resp.read()
    return resp.status, data


def build_bulk(dataset, replicate):
    with open(dataset, "rb") as f:
        lines = [ln for ln in f.read().split(b"\n") if ln.strip()]
    # Replicate to reach a workload size; auto-ids keep copies distinct.
    out = b"\n".join(lines * replicate) + b"\n"
    return out, len(lines) * replicate


def dir_bytes(path):
    total = 0
    for root, _dirs, files in os.walk(path):
        for fn in files:
            try:
                total += os.path.getsize(os.path.join(root, fn))
            except OSError:
                pass
    return total


def phase_index(target, index, dataset, replicate, datadir):
    body, ndocs = build_bulk(dataset, replicate)
    conn = connect(target)
    t0 = time.perf_counter()
    status, data = request(conn, "RESTORE", f"/{index}/?commit",
                           body=body, headers={"Content-Type": "application/x-ndjson"})
    elapsed = time.perf_counter() - t0
    conn.close()
    ok = status in (200, 201, 204)
    # Measure THIS index's own directory ({datadir}/{index}), not the cumulative
    # data dir -- otherwise leftover indexes from earlier runs inflate the number.
    index_dir = os.path.join(datadir, index) if datadir else None
    return {
        "docs": ndocs,
        "seconds": round(elapsed, 3),
        "docs_per_sec": round(ndocs / elapsed) if elapsed > 0 else 0,
        "restore_status": status,
        "ok": ok,
        "disk_bytes": dir_bytes(index_dir) if index_dir and os.path.isdir(index_dir) else None,
    }


def phase_query(target, index, concurrency, duration):
    import threading
    latencies = []
    errors = [0]
    lock = threading.Lock()
    stop_at = time.perf_counter() + duration

    def worker(wid):
        conn = connect(target)
        local = []
        local_err = 0
        i = 0
        while time.perf_counter() < stop_at:
            _label, q = QUERY_MIX[(wid + i) % len(QUERY_MIX)]
            i += 1
            body = json.dumps(q).encode()
            t0 = time.perf_counter()
            try:
                status, _data = request(conn, "SEARCH", f"/{index}/",
                                        body=body, headers={"Content-Type": "application/json"})
                dt = (time.perf_counter() - t0) * 1000.0
                if status == 200:
                    local.append(dt)
                else:
                    local_err += 1
            except Exception:
                local_err += 1
                conn = connect(target)
        conn.close()
        with lock:
            latencies.extend(local)
            errors[0] += local_err

    threads = [threading.Thread(target=worker, args=(w,)) for w in range(concurrency)]
    t0 = time.perf_counter()
    for t in threads:
        t.start()
    for t in threads:
        t.join()
    wall = time.perf_counter() - t0

    latencies.sort()

    def pct(p):
        if not latencies:
            return None
        k = min(len(latencies) - 1, int(round(p / 100.0 * (len(latencies) - 1))))
        return round(latencies[k], 3)

    n = len(latencies)
    return {
        "concurrency": concurrency,
        "duration": round(wall, 2),
        "total": n,
        "errors": errors[0],
        "qps": round(n / wall) if wall > 0 else 0,
        "latency_ms": {
            "p50": pct(50), "p90": pct(90), "p95": pct(95), "p99": pct(99),
            "max": round(latencies[-1], 3) if latencies else None,
            "mean": round(statistics.fmean(latencies), 3) if latencies else None,
        },
    }


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--target", default="localhost:8880")
    ap.add_argument("--index", default="bench_load")
    ap.add_argument("--dataset", default="docs/assets/accounts.ndjson")
    ap.add_argument("--replicate", type=int, default=20)
    ap.add_argument("--concurrency", type=int, default=16)
    ap.add_argument("--duration", type=int, default=8)
    ap.add_argument("--trials", type=int, default=3)
    ap.add_argument("--datadir", default=None)
    ap.add_argument("--label", default="")
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    result = {"target": args.target, "label": args.label, "index": args.index}
    idx_trials = []
    for tr in range(args.trials):
        # fresh index per trial so each is a clean bulk load (-> a noise band)
        ip = phase_index(args.target, f"{args.index}_t{tr}", args.dataset, args.replicate,
                         args.datadir if tr == 0 else None)
        idx_trials.append(ip)
        print(f"[index] trial {tr+1}/{args.trials}: {ip['docs']} docs in {ip['seconds']}s "
              f"= {ip['docs_per_sec']} docs/s"
              + (f", disk={ip['disk_bytes']}" if ip['disk_bytes'] else ""), flush=True)
    idx_sorted = sorted(idx_trials, key=lambda x: x["docs_per_sec"])
    result["index_phase"] = idx_sorted[len(idx_sorted) // 2]
    # disk from the first trial's dedicated measurement (one index's worth)
    result["index_phase"]["disk_bytes"] = idx_trials[0]["disk_bytes"]
    result["index_trials"] = idx_trials

    trials = []
    # Query the index actually populated by the index phase (trial 0's index).
    # The index phase writes to per-trial names ({index}_t{tr}); querying the bare
    # {index} name would hit a non-existent index (0 hits, and on a cluster a wasted
    # remote fan-out to missing shards), so results must target a populated one.
    query_index = f"{args.index}_t0"
    # Sanity guard: the query workload is meaningless against an empty/missing index
    # (a nonexistent index still answers SEARCH with HTTP 200 and 0 hits, so the
    # error count alone will not catch it).  Probe once and fail loudly.
    probe_conn = connect(args.target)
    _label, probe_q = QUERY_MIX[0]
    pstatus, pdata = request(probe_conn, "SEARCH", f"/{query_index}/",
                             body=json.dumps(probe_q).encode(),
                             headers={"Content-Type": "application/json"})
    probe_conn.close()
    try:
        pcount = json.loads(pdata).get("count", 0)
    except Exception:
        pcount = 0
    if pstatus != 200 or not pcount:
        print(f"[query] ABORT: query index '{query_index}' returned status={pstatus} "
              f"count={pcount} -- nothing to query (was it indexed?)", flush=True)
        sys.exit(2)
    for tr in range(args.trials):
        q = phase_query(args.target, query_index, args.concurrency, args.duration)
        trials.append(q)
        print(f"[query] trial {tr+1}/{args.trials}: {q['qps']} qps, "
              f"p50={q['latency_ms']['p50']}ms p95={q['latency_ms']['p95']}ms p99={q['latency_ms']['p99']}ms "
              f"(n={q['total']}, err={q['errors']})", flush=True)
    # median trial by qps
    trials_sorted = sorted(trials, key=lambda x: x["qps"])
    result["query_phase"] = trials_sorted[len(trials_sorted) // 2]
    result["query_trials"] = trials

    if args.out:
        os.makedirs(os.path.dirname(args.out), exist_ok=True)
        with open(args.out, "w") as f:
            json.dump(result, f, indent=2)
        print(f"wrote {args.out}", flush=True)
    else:
        print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
