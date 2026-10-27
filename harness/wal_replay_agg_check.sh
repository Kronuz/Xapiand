#!/usr/bin/env bash
# wal_replay_agg_check.sh -- after kill -9 and WAL replay, numeric aggregations and sorts
# must see the replayed values without a further write.
#
# Steps (exit non-zero on a failed assertion):
#   1. Start a solo node, RESTORE N docs with a numeric "age" without commit, kill -9.
#   2. Restart on the same data dir (WAL replay).
#   3. Assert _avg(age) and _max(age) equal the values computed from the input.
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8907}"
NDOCS="${NDOCS:-200}"
RUNDIR="${RUNDIR:-$REPO/.scratch/xwalagg_$$}"
mkdir -p "$RUNDIR"
PID=""
fails=0
cleanup() { [ -n "$PID" ] && kill -9 "$PID" 2>/dev/null; rm -rf "$RUNDIR"; }
trap cleanup EXIT
fail() { echo "  FAIL: $*"; fails=$((fails + 1)); }
pass() { echo "  ok: $*"; }

start_node() {
	"$BIN" --port "$PORT" --solo --name walaggnode -D "$RUNDIR/n" >>"$RUNDIR/$1" 2>&1 &
	PID=$!
	for _ in $(seq 80); do
		kill -0 "$PID" 2>/dev/null || return 1
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	return 1
}

echo "== [wal-replay-agg] kill -9 after uncommitted numeric writes -> aggregations after replay =="
python3 -c "
import json
for i in range($NDOCS):
    print(json.dumps({'_id': 'u%d' % i, 'age': i % 40, 'balance': i * 1.5}))
" > "$RUNDIR/docs.ndjson"
expect_avg="$(python3 -c "print(sum(i % 40 for i in range($NDOCS)) / $NDOCS)")"

start_node n1.log || { fail "node1 startup"; exit 1; }
curl -s -m20 -o /dev/null -XRESTORE "localhost:$PORT/agg/" -H 'Content-Type: application/x-ndjson' --data-binary @"$RUNDIR/docs.ndjson"
sleep 0.5
kill -9 "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; PID=""

start_node n2.log || { fail "node2 (restart) startup"; exit 1; }
res="$(curl -s -m8 -XPOST "localhost:$PORT/agg/:search?volatile" \
	-d '{"_query":"*","_limit":0,"_aggs":{"a":{"_avg":{"_field":"age"}},"m":{"_max":{"_field":"age"}}}}' \
	| python3 -c 'import sys,json
try:
    a = json.load(sys.stdin)["aggregations"]; print(a["a"]["_avg"], a["m"]["_max"])
except Exception:
    print("? ?")')"
set -- $res
if python3 -c "import sys; sys.exit(0 if abs(float('$1') - $expect_avg) < 1e-6 and float('$2') == 39 else 1)" 2>/dev/null; then
	pass "avg=$1 max=$2 after replay"
else
	fail "avg=$1 max=$2 after replay, expected avg=$expect_avg max=39"
fi

[ "$fails" = 0 ] && { echo "  PASS"; exit 0; }
echo "  FAILED ($fails assertion(s))"; exit 1
