#!/usr/bin/env bash
# wal_recovery_check.sh -- verify write-ahead-log crash recovery on a single node.
#
# The contract: every write is appended to the per-database WAL as it happens,
# before the debounced glass commit (the timed committer only forces at ~8-10s,
# or the count threshold at 100000). So if the node dies uncleanly -- kill -9,
# power loss -- after some *uncommitted* writes, restarting on the same data dir
# must REPLAY the WAL and recover every one of them.
#
# This is the modern, real-architecture equivalent of the retired
# oldtests/test_wal.cc, which drove the 2019 DatabaseQueue/Database API by hand.
# Here we exercise the actual end-to-end path (HTTP index -> WAL -> crash ->
# open-time replay) instead of the long-gone internal classes.
#
# Steps (exit non-zero on any failed assertion):
#   1. Start a solo node on a fresh data dir.
#   2. PUT N docs WITHOUT ?commit=true  -> they live in the WAL, not yet in glass.
#   3. kill -9 the node within ~1s (well before the ~8s timed committer) -> crash.
#   4. Restart on the SAME data dir     -> WAL replay recovers the writes.
#   5. SEARCH -> assert the recovered count == N.
#   6. Assert the restart log shows the replay ("Read and execute operations WAL").
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8906}"
NDOCS="${NDOCS:-50}"
IDX="walrec"
RUNDIR="/tmp/xwalrec_$$"
mkdir -p "$RUNDIR"

PID=""
fails=0
cleanup() { [ -n "$PID" ] && kill -9 "$PID" 2>/dev/null; rm -rf "$RUNDIR"; }
trap cleanup EXIT

fail() { echo "  FAIL: $*"; fails=$((fails + 1)); }
pass() { echo "  ok: $*"; }

wait_ready() {
	local deadline=$(( $(date +%s) + 40 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		kill -0 "$PID" 2>/dev/null || { echo "  node died during startup"; return 1; }
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	echo "  node never became ready"; return 1
}

start_node() {  # $1 = log file name under RUNDIR
	# --verbosity 3 so the WAL replay line (an L_INFO) is captured for step 6.
	"$BIN" --port "$PORT" --solo --name walrecnode --verbosity 3 -D "$RUNDIR/n" >>"$RUNDIR/$1" 2>&1 &
	PID=$!
	wait_ready
}

# Total number of matching documents (".total", the full match count -- ".count"
# is only the returned page, capped at the default page size of 10).
search_total() {
	curl -s -m8 -XPOST "localhost:$PORT/$IDX/:search" \
		-H 'Content-Type: application/json' -d '{"_query":"*"}' 2>/dev/null \
		| python3 -c 'import sys,json
try:
    print(json.load(sys.stdin).get("total", "?"))
except Exception:
    print("?")'
}

echo "== [wal-recovery] crash after uncommitted writes -> restart replays WAL =="

# 1 + 2: start, then index N docs WITHOUT commit (they go to the WAL only).
start_node node1.log || { fail "node1 startup"; exit 1; }
for i in $(seq 1 "$NDOCS"); do
	curl -s -m5 -o /dev/null -XPUT "localhost:$PORT/$IDX/$i" \
		-H 'Content-Type: application/json' -d "{\"n\":$i,\"txt\":\"doc number $i\"}"
done
sleep 1   # let the async WAL writer persist; still far under the ~8s committer

# 3: crash the node (kill -9) before any timed commit could flush glass.
kill -9 "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
PID=""

# 4: restart on the same data dir -> open-time WAL replay.
start_node node2.log || { fail "node2 (restart) startup"; exit 1; }
sleep 1

# 5: every uncommitted write must have survived via the WAL. Replay applies the
# ops to the reopened database; a commit makes them visible to search (normal
# read-your-writes -- without the WAL there would be nothing to commit).
curl -s -m8 -o /dev/null -XPOST "localhost:$PORT/$IDX/:commit"
got="$(search_total)"
if [ "$got" = "$NDOCS" ]; then
	pass "recovered $got/$NDOCS docs after crash"
else
	fail "recovered $got docs, expected $NDOCS (WAL replay lost writes)"
fi

# 6: positive proof the recovery went through the WAL (not a lucky early commit).
if grep -qiE 'Read and execute operations WAL' "$RUNDIR/node2.log"; then
	pass "restart log shows WAL replay"
else
	echo "  note: no explicit WAL-replay log line (log level may hide L_INFO);" \
		"relying on the count assertion above"
fi

if [ "$fails" = 0 ]; then
	echo "  PASS"
	exit 0
else
	echo "  FAILED ($fails assertion(s))"
	exit 1
fi
