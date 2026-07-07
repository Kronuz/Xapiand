#!/usr/bin/env bash
#
# cluster_bench.sh -- benchmark the SAME load twice: once on a single --solo node
# (no Remote protocol) and once on a 2-node cluster (index sharded across nodes, so
# every search runs the two-phase Remote match and every write replicates).  Prints
# both sets of numbers and the delta, so the cost of the distributed data plane is
# explicit.
#
# It drives harness/loadtest.py (index throughput + query QPS/latency) against each.
#
# Usage:
#   harness/cluster_bench.sh
#   REPLICATE=20 CONCURRENCY=16 DURATION=5 TRIALS=2 harness/cluster_bench.sh
#   BIN=build/bin/xapiand harness/cluster_bench.sh
set -u

HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HARNESS_DIR")"
BIN="${BIN:-$REPO/build/bin/xapiand}"
DATASET="${DATASET:-$REPO/docs/public/assets/accounts.ndjson}"
REPLICATE="${REPLICATE:-20}"
CONCURRENCY="${CONCURRENCY:-16}"
DURATION="${DURATION:-5}"
TRIALS="${TRIALS:-2}"

HTTP1=8880; REMOTE1=9880; REPLICA1=7880
HTTP2=8881; REMOTE2=9881; REPLICA2=7881
DISCOVERY=58880
IFACE="${DISCOVERY_IFACE:-127.0.0.1}"
RUN="${RUNDIR:-$REPO/build/xbench_$$}"
OUTDIR="$HARNESS_DIR/results"

[ -x "$BIN" ] || { echo "missing binary: $BIN"; exit 1; }
mkdir -p "$RUN" "$OUTDIR"
PIDS=()
cleanup() { for p in "${PIDS[@]:-}"; do [ -n "$p" ] && kill "$p" 2>/dev/null; done; sleep 1; rm -rf "$RUN"; }
trap cleanup EXIT

wait_ready() {  # wait_ready <http_port> <deadline_s>
	local d=$(( $(date +%s) + $2 ))
	while [ "$(date +%s)" -lt "$d" ]; do
		[ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 "localhost:$1/" 2>/dev/null)" = "200" ] && return 0
		sleep 0.5
	done
	return 1
}
wait_members() {  # wait_members <http_port> <n> <deadline_s>
	local d=$(( $(date +%s) + $3 ))
	while [ "$(date +%s)" -lt "$d" ]; do
		local c; c="$(curl -s --max-time 3 "localhost:$1/" 2>/dev/null | \
			python3 -c "import sys,json
try: print(len(json.load(sys.stdin).get('nodes',[])))
except Exception: print(0)" 2>/dev/null)"
		[ "${c:-0}" -ge "$2" ] && return 0
		sleep 1
	done
	return 1
}

run_loadtest() {  # run_loadtest <target> <label> <datadir> <out>
	python3 "$HARNESS_DIR/loadtest.py" --target "$1" --dataset "$DATASET" \
		--replicate "$REPLICATE" --concurrency "$CONCURRENCY" --duration "$DURATION" \
		--trials "$TRIALS" --datadir "$3" --label "$2" --out "$4"
}

echo "== cluster_bench: dataset=$(basename "$DATASET") x$REPLICATE  c=$CONCURRENCY  ${DURATION}s x$TRIALS trials =="

# ---- 1. SOLO (no Remote protocol) -------------------------------------------
echo
echo "== [1/2] SOLO (single node, no Remote protocol) =="
"$BIN" --port "$HTTP1" --name solo --solo -D "$RUN/solo" >"$RUN/solo.log" 2>&1 &
PIDS+=($!)
wait_ready "$HTTP1" 30 || { echo "solo node didn't come up"; exit 1; }
run_loadtest "localhost:$HTTP1" "solo" "$RUN/solo" "$OUTDIR/bench_solo.json"
kill "${PIDS[-1]}" 2>/dev/null; wait "${PIDS[-1]}" 2>/dev/null
sleep 1

# ---- 2. CLUSTER (2 nodes, Remote protocol in the loop) ----------------------
echo
echo "== [2/2] CLUSTER (2 nodes, index sharded -> Remote protocol + replication) =="
"$BIN" --port "$HTTP1" --xapian-port "$REMOTE1" --replica-port "$REPLICA1" \
	--discovery-port "$DISCOVERY" --discovery-interface "$IFACE" \
	--name node1 --primary-node node1 -D "$RUN/node1" >"$RUN/node1.log" 2>&1 &
PIDS+=($!)
wait_ready "$HTTP1" 60 || { echo "node1 didn't come up"; exit 1; }
"$BIN" --port "$HTTP2" --xapian-port "$REMOTE2" --replica-port "$REPLICA2" \
	--discovery-port "$DISCOVERY" --discovery-interface "$IFACE" \
	--name node2 --primary-node node1 -D "$RUN/node2" >"$RUN/node2.log" 2>&1 &
PIDS+=($!)
if ! wait_members "$HTTP1" 2 90; then
	echo "cluster didn't form (discovery needs a multicast-capable interface; see cluster_check.sh)."
	echo "solo numbers were still captured in $OUTDIR/bench_solo.json"
	exit 1
fi
# let membership + shard setup settle before the first (slow) writes
sleep 5
run_loadtest "localhost:$HTTP1" "cluster2" "$RUN/node1" "$OUTDIR/bench_cluster.json"

# ---- compare ----------------------------------------------------------------
echo
echo "== RESULTS: solo vs 2-node cluster =="
python3 - "$OUTDIR/bench_solo.json" "$OUTDIR/bench_cluster.json" <<'PY'
import json, sys
def load(p):
    d = json.load(open(p))
    ip = d.get("index_phase", {})
    qp = d.get("query_phase", {})
    lat = qp.get("latency_ms", {})
    return dict(idx=ip.get("docs_per_sec", 0), disk=ip.get("disk_bytes", 0),
                qps=qp.get("qps", 0), p50=lat.get("p50", 0),
                p95=lat.get("p95", 0), p99=lat.get("p99", 0))
solo, clus = load(sys.argv[1]), load(sys.argv[2])
def pct(a, b): return "n/a" if not a else "{:+.0f}%".format((b/a - 1) * 100)
rows = [
    ("index throughput (docs/s)", "{:.0f}".format(solo["idx"]), "{:.0f}".format(clus["idx"]), pct(solo["idx"], clus["idx"])),
    ("query throughput (qps)",    "{:.0f}".format(solo["qps"]), "{:.0f}".format(clus["qps"]), pct(solo["qps"], clus["qps"])),
    ("query p50 (ms)",            "{:.3f}".format(solo["p50"]), "{:.3f}".format(clus["p50"]), pct(solo["p50"], clus["p50"])),
    ("query p95 (ms)",            "{:.3f}".format(solo["p95"]), "{:.3f}".format(clus["p95"]), pct(solo["p95"], clus["p95"])),
    ("query p99 (ms)",            "{:.3f}".format(solo["p99"]), "{:.3f}".format(clus["p99"]), pct(solo["p99"], clus["p99"])),
    ("on-disk (bytes, 1 shard)",  "{}".format(solo["disk"]),   "{}".format(clus["disk"]),    ""),
]
w = max(len(r[0]) for r in rows)
print("  {:<{w}}  {:>12}  {:>12}  {:>8}".format("metric", "solo", "cluster(2)", "delta", w=w))
print("  " + "-" * (w + 40))
for name, a, b, d in rows:
    print("  {:<{w}}  {:>12}  {:>12}  {:>8}".format(name, a, b, d, w=w))
print()
print("  Note: the cluster query path runs the two-phase Remote match across nodes;")
print("  index throughput also pays replication + first-touch shard creation.")
PY
echo
echo "saved: $OUTDIR/bench_solo.json, $OUTDIR/bench_cluster.json"
