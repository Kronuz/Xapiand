#!/usr/bin/env bash
# baseline_7c.sh BIN BIN_TRACE OUTDIR [--stall-secs 30] [--accounts-docs 1000000]
#
# The pre-7c baseline (docs/perf/phase7c-baseline.md): the rows step X7 compares against, one command each.
#   BIN        a RelWithDebInfo -fno-omit-frame-pointer build of the commit under test
#   BIN_TRACE  the same commit built with -DHTTP_TRACE_HOP and the http trace/hop branch (profile_light only)
# Run it under a hold that keeps the host to itself:
#   QUIET_PAUSE=1 quiet strict NAME 90 "pre-7c baseline" -- harness/baseline_7c.sh BIN BIN_TRACE OUTDIR
# It starts, and removes, every node it uses.
set -u
[ $# -ge 3 ] || { sed -n 2,12p "$0"; exit 2; }
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); TRACE=$(cd "$(dirname "$2")" && pwd)/$(basename "$2"); OUT=$(mkdir -p "$3" && cd "$3" && pwd); shift 3
SECS=30; DOCS=1000000
while [ $# -gt 0 ]; do case $1 in --stall-secs) SECS=$2; shift 2 ;; --accounts-docs) DOCS=$2; shift 2 ;; *) echo "unknown $1" >&2; exit 2 ;; esac; done
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(dirname "$HERE")
T() { date +%s; }
step() { echo "== $* ($(date +%H:%M:%S))"; }
echo "commit $(git -C "$REPO" rev-parse --short HEAD), $(sysctl -n hw.ncpu 2>/dev/null || nproc) CPUs, host $(hostname -s)" | tee "$OUT/host.txt"

# T4, T5, T8 and T1's census: one load of accounts, the whole stall_load sequence.
step "stall_load --suite (T4, T5, T8)"; t0=$(T)
"$HERE/stall_load.py" --bin "$BIN" --suite --census --accounts-docs "$DOCS" --duration "$SECS" --out "$OUT/stall_suite.json" 2> "$OUT/stall_suite.log"; echo "exit $? in $(( $(T) - t0 )) s"

# T11: dispatch cost per light request, CPU from frames and wall from the trace stamps.
step "profile_light (T11)"; t0=$(T)
"$HERE/profile_light.sh" "$TRACE" "$OUT/light" --secs 20 --label pre7c 2> "$OUT/profile_light.log"; echo "exit $? in $(( $(T) - t0 )) s"

# T12: RESTORE docs/s on 20,000 accounts (the smoke of profile_bulk).
step "profile_bulk (T12 RESTORE)"; t0=$(T)
"$HERE/profile_bulk.sh" "$BIN" "$OUT/bulk" --mult 20 --label restore20k > "$OUT/profile_bulk.log" 2>&1; echo "exit $? in $(( $(T) - t0 )) s"

# T12: saturated query QPS (loadtest.py, 16 clients, 3 trials), on a node this script owns.
step "loadtest (T12 QPS)"; t0=$(T)
DATA="$OUT/loadtest.data"; rm -rf "$DATA"; mkdir -p "$DATA"
XAPIAND_ENGINE=strata "$BIN" --port 18993 --solo --name bench -D "$DATA" > "$OUT/loadtest.node.log" 2>&1 &
NODE=$!
cleanup() { kill "$NODE" 2>/dev/null; wait "$NODE" 2>/dev/null; rm -rf "$DATA"; }
trap cleanup EXIT
for _ in $(seq 1 120); do curl -s "localhost:18993/" > /dev/null && break; sleep 0.5; done
python3 "$HERE/loadtest.py" --target localhost:18993 --dataset "$REPO/docs/public/assets/accounts.ndjson" --replicate 20 \
	--concurrency 16 --duration 8 --trials 3 --datadir "$DATA" --out "$OUT/loadtest.json" > "$OUT/loadtest.log" 2>&1; echo "exit $? in $(( $(T) - t0 )) s"
echo finished
