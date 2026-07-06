#!/usr/bin/env bash
#
# run_all.sh -- the one command that exercises Xapiand top to bottom.
#
# It orchestrates the per-layer harnesses (it does not reimplement them) and prints
# a single pass/fail/skip summary.  Each layer is independent; a layer whose
# prerequisites are missing (a baseline report, the oracle binary, GTest) is SKIPPED,
# not failed, with a note on how to enable it.
#
# Layers (run in this order; select a subset by naming them as arguments):
#   smoke    single-node index + search sanity (no prerequisites)          [always]
#   unit     C++ unit tests via ctest (needs a -DBUILD_TESTS=ON build)
#   e2e      doc-driven functional E2E vs the saved baseline (e2e_check.sh)
#   cluster  2-node discovery + remote/replication + distributed search (cluster_check.sh)
#   recovery WAL crash recovery: kill -9 after uncommitted writes -> restart replays (wal_recovery_check.sh)
#   load     quick functional bulk load (index_fortune)
#   stress   short concurrent soak with read-back verification (stress_fortune)
#   bench    index/query benchmark (loadtest.py)
#
# Usage:
#   harness/run_all.sh                 # smoke + e2e + cluster + recovery + load + stress + bench
#   harness/run_all.sh smoke cluster   # just those two
#   BIN=build/bin/xapiand harness/run_all.sh
#
# See TESTING.md for what each layer proves and how to (re)generate baselines.
set -u

HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HARNESS_DIR")"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8899}"

DEFAULT_LAYERS="smoke e2e cluster recovery load stress bench"
LAYERS="${*:-$DEFAULT_LAYERS}"

PASS=(); FAIL=(); SKIP=()
pass() { PASS+=("$1"); echo "  ✅ $1"; }
fail() { FAIL+=("$1"); echo "  ❌ $1"; }
skip() { SKIP+=("$1: $2"); echo "  ⏭️  $1 (skipped: $2)"; }

if [ ! -x "$BIN" ]; then
	echo "missing binary: $BIN"
	echo "build it: (cd build && cmake .. && make xapiand -j4)   or set BIN=..."
	exit 1
fi
echo "== run_all: binary $BIN =="
echo "== layers: $LAYERS =="

# ---- helpers ----------------------------------------------------------------
wait_http() {  # wait_http <port> <deadline_s>
	local deadline=$(( $(date +%s) + $2 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		[ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 "localhost:$1/" 2>/dev/null)" = "200" ] && return 0
		sleep 0.5
	done
	return 1
}

# ---- layers -----------------------------------------------------------------
# Stop a node started by a layer and WAIT for it to fully exit, so the next layer
# can rebind the shared port without racing a dying node (which would answer 500s
# / make the fresh node fail to bind).
stop_node() { kill "$1" 2>/dev/null; wait "$1" 2>/dev/null; }

run_smoke() {
	echo "== [smoke] single-node index + search =="
	local dir; dir="$(mktemp -d /tmp/xa_smoke.XXXXXX)"
	"$BIN" --port "$PORT" --name smoke --solo -D "$dir" >"$dir/node.log" 2>&1 &
	local pid=$!
	local ok=1
	if wait_http "$PORT" 30; then
		curl -s -m5 -XPUT "localhost:$PORT/smoke/1?commit=true" -H 'Content-Type: application/json' \
			-d '{"body":"the quick brown fox"}' -o /dev/null
		local n; n="$(curl -s -m8 -XPOST "localhost:$PORT/smoke/:search" -H 'Content-Type: application/json' \
			-d '{"_query":{"body":"brown"}}' | python3 -c 'import sys,json;print(json.load(sys.stdin).get("count",0))' 2>/dev/null)"
		[ "$n" = "1" ] && ok=0
	fi
	stop_node "$pid"
	rm -rf "$dir"
	[ "$ok" = 0 ] && pass "smoke" || fail "smoke"
}

run_unit() {
	echo "== [unit] ctest =="
	if [ ! -f "$REPO/build/CTestTestfile.cmake" ] && [ ! -d "$REPO/build/Testing" ]; then
		# Tests are only wired when configured with -DBUILD_TESTS=ON (needs GTest).
		if ! (cd "$REPO/build" && ctest -N >/dev/null 2>&1); then
			skip "unit" "no tests configured -- reconfigure with -DBUILD_TESTS=ON (needs GTest); see TESTING.md"
			return
		fi
	fi
	if (cd "$REPO/build" && ctest --output-on-failure); then pass "unit"; else fail "unit"; fi
}

run_e2e() {
	echo "== [e2e] functional (vs baseline) =="
	if [ ! -f "$HARNESS_DIR/e2e_baseline.json" ]; then
		skip "e2e" "committed baseline harness/e2e_baseline.json missing; refresh with e2e_baseline.sh"
		return
	fi
	# Pass signal is ASSERTION parity vs the committed, distilled baseline
	# (e2e_baseline.json): no assertion regressed on a request that exists in both.
	# e2e_diff.py matches requests by NAME (not position), so added/removed doc
	# examples never cascade into false diffs. Body-level identity (bodydiff, in
	# e2e_check.sh) is deliberately NOT the bar here -- on a branch that changes
	# behaviour on purpose (e.g. Xapian 2.0.0's stemming) bodies legitimately differ
	# while assertions still pass. When you intentionally change documented
	# behaviour, refresh the baseline: harness/e2e_baseline.sh.
	local out="/tmp/xa_e2e_ours.json"
	"$HARNESS_DIR/e2e_capture.sh" "$BIN" "$out" >/dev/null 2>&1
	if python3 "$HARNESS_DIR/e2e_diff.py" "$out" "$HARNESS_DIR/e2e_baseline.json" 2>/dev/null | grep -q '^GREEN'; then
		pass "e2e"
	else
		fail "e2e"
	fi
}

run_cluster() {
	echo "== [cluster] 2-node remote/replication/distributed-search =="
	if BIN="$BIN" "$HARNESS_DIR/cluster_check.sh" 2; then pass "cluster"; else fail "cluster"; fi
}

run_recovery() {
	echo "== [recovery] WAL crash recovery =="
	if BIN="$BIN" "$HARNESS_DIR/wal_recovery_check.sh"; then pass "recovery"; else fail "recovery"; fi
}

run_load() {
	echo "== [load] index_fortune bulk load =="
	local dir; dir="$(mktemp -d /tmp/xa_load.XXXXXX)"
	"$BIN" --port "$PORT" --name load --solo -D "$dir" >"$dir/node.log" 2>&1 &
	local pid=$!
	local ok=1
	if wait_http "$PORT" 30; then
		sh "$HARNESS_DIR/index_fortune" "http://localhost:$PORT/loadtest?commit=true" 1 200 -o /dev/null
		local n; n="$(curl -s "localhost:$PORT/loadtest/:info" | python3 -c 'import sys,json;print(json.load(sys.stdin).get("doc_count",0))' 2>/dev/null)"
		[ "${n:-0}" -ge 200 ] && ok=0 || echo "  (indexed ${n:-0}/200)"
	fi
	stop_node "$pid"; rm -rf "$dir"
	[ "$ok" = 0 ] && pass "load" || fail "load"
}

run_stress() {
	echo "== [stress] stress_fortune soak =="
	local dir; dir="$(mktemp -d /tmp/xa_stress.XXXXXX)"
	"$BIN" --port "$PORT" --name stress --solo -D "$dir" >"$dir/node.log" 2>&1 &
	local pid=$!
	local ok=1
	if wait_http "$PORT" 30; then
		python3 "$HARNESS_DIR/stress_fortune" --target "localhost:$PORT" \
			--workers 30 --databases 8 --duration 10 --commit 2>/dev/null && ok=0
	fi
	stop_node "$pid"; rm -rf "$dir"
	[ "$ok" = 0 ] && pass "stress" || fail "stress"
}

run_bench() {
	echo "== [bench] loadtest.py =="
	local dir; dir="$(mktemp -d /tmp/xa_bench.XXXXXX)"
	"$BIN" --port "$PORT" --name bench --solo -D "$dir" >"$dir/node.log" 2>&1 &
	local pid=$!
	local ok=1
	if wait_http "$PORT" 30; then
		python3 "$HARNESS_DIR/loadtest.py" --target "localhost:$PORT" \
			--dataset "$REPO/docs/assets/accounts.ndjson" --replicate 10 \
			--concurrency 16 --duration 4 --trials 1 --datadir "$dir" \
			--out "$dir/bench.json" && ok=0
	fi
	stop_node "$pid"; rm -rf "$dir"
	[ "$ok" = 0 ] && pass "bench" || fail "bench"
}

# ---- drive ------------------------------------------------------------------
for p in $(pgrep -f "port $PORT" 2>/dev/null); do :; done  # (informational; each layer uses its own dir)
for layer in $LAYERS; do
	case "$layer" in
		smoke)   run_smoke   ;;
		unit)    run_unit    ;;
		e2e)     run_e2e     ;;
		cluster) run_cluster ;;
		recovery) run_recovery ;;
		load)    run_load    ;;
		stress)  run_stress  ;;
		bench)   run_bench   ;;
		*) echo "unknown layer: $layer (valid: smoke unit e2e cluster recovery load stress bench)"; exit 2 ;;
	esac
done

echo
echo "===================== SUMMARY ====================="
echo "  passed:  ${#PASS[@]}   ${PASS[*]:-}"
echo "  failed:  ${#FAIL[@]}   ${FAIL[*]:-}"
echo "  skipped: ${#SKIP[@]}"
for s in "${SKIP[@]:-}"; do [ -n "$s" ] && echo "      - $s"; done
echo "==================================================="
[ "${#FAIL[@]}" -eq 0 ] && { echo "VERDICT: GREEN"; exit 0; } || { echo "VERDICT: RED (${#FAIL[@]} failed)"; exit 1; }
