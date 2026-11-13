# report.py OUT: per run of rc.sh, in order: t45 loaded numbers (and the census of its phases when sampled), lt
# index docs/s and query p99; then each arm's medians.
import glob, json, os, re, statistics as st, sys
from collections import defaultdict

out = sys.argv[1]
IDLE = {"kevent", "kevent64", "kevent_id", "__psynch_cvwait", "__semwait_signal", "__workq_kernreturn",
        "mach_msg2_trap", "mach_msg_trap", "__select", "poll", "__wait4", "read", "__recvfrom", "accept"}
LOCK = {"__ulock_wait", "__ulock_wait2", "__psynch_mutexwait", "__psynch_rw_rdlock", "__psynch_rw_wrlock"}


def runs(prefix):
    for f in glob.glob(os.path.join(out, f"{prefix}-*.json")):
        if m := re.search(r"-(\d+)-(\w+)\.json$", f):
            yield int(m[1]), m[2], f


def sample_busy(path):
    """Per thread name (digits stripped): samples, busy (not in a wait), lock-waiting, from a `sample` call graph."""
    acc = defaultdict(lambda: [0, 0, 0, 0])   # threads, samples, idle, lock
    name, total = None, 0
    in_graph = False
    for line in open(path, errors="replace"):
        if line.startswith("Call graph:"):
            in_graph = True
            continue
        if in_graph and line.startswith("Total number in stack"):
            break
        if not in_graph or not line.strip():
            continue
        if m := re.match(r"^\s+(\d+) Thread_\d+\s*:?\s*(.*)$", line):
            total = int(m[1])
            n = m[2]
            n = "main" if "main-thread" in n else re.sub(r"^DispatchQueue_\d+:\s*", "", n)
            n = re.sub(r"[-_.]*\d*[-_.]*$", "", n.split(" (")[0].strip()) or "unnamed"
            name = n
            acc[name][0] += 1
            acc[name][1] += total
            continue
        body = line.lstrip(" +!:|")
        if m := re.match(r"(\d+)\s+(\S+)", body):
            fn = m[2]
            if fn in IDLE:
                acc[name][2] += int(m[1])
            elif fn in LOCK:
                acc[name][3] += int(m[1])
    return acc


def census(arm_file, phase):
    p = arm_file[: -len(".json")] + ".ps.jsonl"
    if not os.path.exists(p):
        return None
    t0, t1 = phase["started"], phase["started"] + phase["seconds"]
    recs = [json.loads(x) for x in open(p) if x.strip()]
    w = [r for r in recs if t0 <= r["t"] <= t1]
    if not w:
        return None
    nr = sorted(r["node_r"] for r in w)
    res = {
        "n": len(w),
        "node_r_mean": round(st.mean(nr), 1), "node_r_p50": nr[len(nr) // 2], "node_r_p90": nr[int(len(nr) * 0.9)],
        "node_r_max": nr[-1], "other_r_mean": round(st.mean(r["other_r"] for r in w), 1),
        "node_cpu_mean": round(st.mean(r["node_cpu"] for r in w)), "threads": w[-1]["node_threads"],
    }
    busy = defaultdict(lambda: [0, 0, 0, 0])
    for r in w:
        if "sample" in r and os.path.exists(r["sample"]):
            for k, v in sample_busy(r["sample"]).items():
                b = busy[k]
                b[0] = max(b[0], v[0]); b[1] += v[1]; b[2] += v[2]; b[3] += v[3]
    if busy:
        # per name: threads, busy threads (samples not idle nor lock-waiting, over one thread's samples), lock-waiting
        res["busy"] = {k: (v[0], round((v[1] - v[2] - v[3]) / (v[1] / v[0]), 1) if v[1] else 0,
                           round(v[3] / (v[1] / v[0]), 1) if v[1] else 0)
                       for k, v in sorted(busy.items(), key=lambda kv: -(kv[1][1] - kv[1][2]))}
    return res


per = defaultdict(lambda: defaultdict(list))
for i, arm, f in sorted(runs("t45")):
    d = json.load(open(f))
    u, l = d["phases"][0], d["phases"][1]
    q, a = l["light"]["query"]["p99_us"] / 1e3, l["light"]["all"]["p99_us"] / 1e3 if "all" in l["light"] else None
    h = l["heavy"]
    print(f"t45 {i} {arm}: loaded light query p99 {q:.1f} ms (unloaded {u['light']['query']['p99_us']/1e3:.2f}), "
          f"heavy searches {h['searches']}, restored {h['restored_docs']} in {h['restore_requests']} requests")
    per[arm]["light_p99_ms"].append(q)
    per[arm]["heavy_searches"].append(h["searches"])
    per[arm]["restored"].append(h["restored_docs"])
    for name, ph in (("unloaded", u), ("loaded", l)):
        c = census(f, ph)
        if c:
            busy = c.pop("busy", None)
            print(f"   census {name}: {c}")
            if busy:
                print("     busy (threads, busy threads, lock-waiting threads): " + ", ".join(f"{k} {v}" for k, v in list(busy.items())[:12]))
for i, arm, f in sorted(runs("lt")):
    d = json.load(open(f))
    dps, p99 = d["index_phase"]["docs_per_sec"], d["query_phase"]["latency_ms"]["p99"]
    print(f"lt {i} {arm}: index {dps} docs/s, query p99 {p99} ms, qps {d['query_phase']['qps']}")
    per[arm]["restore_dps"].append(dps)
print("medians:")
for arm, ks in per.items():
    print(f"  {arm}: " + ", ".join(f"{k} {st.median(v):g} {v}" for k, v in ks.items()))
