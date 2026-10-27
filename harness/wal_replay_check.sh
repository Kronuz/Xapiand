#!/usr/bin/env bash
# wal_replay_check.sh -- WAL replay after kill -9 commits what it replays (CR1), on a single node.
#
# Scenarios (docs/mechanisms/wal-replay.md); exit non-zero on a failed assertion:
#   visible     RESTORE N docs with numeric fields without a commit, kill -9, restart: the documents,
#               the schema and the numeric aggregations are visible without a further write.
#   walcommit   the replay's commit is in the WAL (a COMMIT line for the replayed revision), as for
#               any commit, so replicas can pull it.
#   idempotent  crash, replay, crash, replay: the second restart replays nothing and the state is
#               identical; a write between the crashes is replayed exactly once, on top.
#   failure     a commit that fails during the replay's reopen surfaces as an error (no half-open
#               shard answering from stale data), and the next healthy start recovers everything.
# Usage: harness/wal_replay_check.sh [visible|walcommit|idempotent|failure ...]   (default: all)
#        BIN=build/bin/xapiand PORT=8908 NDOCS=200
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8908}"
NDOCS="${NDOCS:-200}"
RUNDIR="${RUNDIR:-$REPO/.scratch/xwalreplay_$$}"
IDX="replay"
mkdir -p "$RUNDIR"
PID=""
fails=0
cleanup() { [ -n "$PID" ] && kill -9 "$PID" 2>/dev/null; [ "${KEEP_RUNDIR:-0}" = 1 ] || rm -rf "$RUNDIR"; }
trap cleanup EXIT
fail() { echo "  FAIL: $*"; fails=$((fails + 1)); }
pass() { echo "  ok: $*"; }

# start_node LOG [FSIZE_BLOCKS]: with a file-size limit, writes past it fail (EFBIG) instead of killing the node.
start_node() {
	local log="$RUNDIR/$1" limit="${2:-}"
	(
		exec > >(cat >>"$log") 2>&1
		[ -n "$limit" ] && { ulimit -f "$limit"; trap '' XFSZ; }
		exec "$BIN" --port "$PORT" --solo --name walreplaynode --verbosity 3 -D "$RUNDIR/n"
	) &
	PID=$!
	for _ in $(seq 80); do
		kill -0 "$PID" 2>/dev/null || return 1
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	return 1
}
crash() { kill -9 "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; PID=""; }

docs() {  # docs FIRST LAST
	python3 -c "
import json
for i in range($1, $2):
    print(json.dumps({'_id': 'u%d' % i, 'age': i % 40, 'balance': i * 1.5}))"
}
restore() {  # restore FIRST LAST: loaded without a commit
	docs "$1" "$2" > "$RUNDIR/docs.ndjson"
	curl -s -m30 -o /dev/null -XRESTORE "localhost:$PORT/$IDX/" -H 'Content-Type: application/x-ndjson' --data-binary @"$RUNDIR/docs.ndjson"
	sleep 0.5
}
state() {  # "total avg max", or "? ? ?"
	curl -s -m10 -XPOST "localhost:$PORT/$IDX/:search?volatile" \
		-d '{"_query":"*","_limit":0,"_aggs":{"a":{"_avg":{"_field":"age"}},"m":{"_max":{"_field":"age"}}}}' \
		| python3 -c 'import sys,json
try:
    d = json.load(sys.stdin); a = d["aggregations"]; print(d["total"], a["a"]["_avg"], a["m"]["_max"])
except Exception:
    print("? ? ?")'
}
expect() {  # expect LAST -> "total avg max" for docs 0..LAST-1
	python3 -c "print($1, sum(i % 40 for i in range($1)) / $1, max(i % 40 for i in range($1)))"
}
same_state() { python3 -c "
a = '$1'.split(); b = '$2'.split()
import sys; sys.exit(0 if len(a) == 3 and a[0] == b[0] and abs(float(a[1]) - float(b[1])) < 1e-6 and float(a[2]) == float(b[2]) else 1)" 2>/dev/null; }
wal_open_ops() {  # registry shards (they hold the schema) whose WAL does not end in a COMMIT line
	local s ops bad=0
	for s in 1 2 3 4 5; do
		ops="$(curl -s -m10 "localhost:$PORT/.xapiand/indexes/.__$s/:wal" | python3 -c 'import sys,json
try:
    print(" ".join(l["op"] for l in json.load(sys.stdin)))
except Exception:
    print("?")')"
		case "$ops" in "") ;; *COMMIT) ;; *) bad=$((bad + 1)) ;; esac
	done
	echo "$bad"
}
count_log() { grep -c "$2" "$RUNDIR/$1" 2>/dev/null || true; }

scenario_main() {  # visible, walcommit and idempotent share one run
	echo "== [wal-replay] crash, replay, crash, replay =="
	start_node n1.log || { fail "startup"; return; }
	restore 0 "$NDOCS"
	crash
	local want; want="$(expect "$NDOCS")"

	start_node n2.log || { fail "restart 1"; return; }
	local s1; s1="$(state)"
	same_state "$s1" "$want" && pass "visible: $s1 after the first replay" || fail "visible: got '$s1', expected '$want'"
	[ "$(count_log n2.log 'Read and execute operations WAL')" -ge 1 ] && pass "the first restart replayed the WAL" || fail "no replay logged on the first restart"

	local open=""
	for _ in $(seq 20); do open="$(wal_open_ops)"; [ "$open" = 0 ] && break; sleep 0.5; done
	[ "$open" = 0 ] && pass "walcommit: every registry WAL ends in the replay's COMMIT line" || fail "walcommit: $open registry WAL(s) end without a COMMIT line"
	crash

	start_node n3.log || { fail "restart 2"; return; }
	local s2; s2="$(state)"
	same_state "$s2" "$s1" && pass "idempotent: identical state after the second crash ($s2)" || fail "idempotent: '$s2' differs from '$s1'"
	[ "$(count_log n3.log 'Read and execute operations WAL')" = 0 ] && pass "idempotent: nothing replayed again" || fail "idempotent: the second restart replayed the WAL again"
	[ "$(count_log n3.log 'WAL ERROR')" = 0 ] && pass "no WAL error on the second restart" || fail "WAL error on the second restart"

	# A document with a new numeric field changes the schema: one more uncommitted registry write.
	echo '{"_id":"extra","age":7,"fresh":3}' | curl -s -m30 -o /dev/null -XRESTORE "localhost:$PORT/$IDX/" -H 'Content-Type: application/x-ndjson' --data-binary @-
	sleep 0.5
	crash
	start_node n4.log || { fail "restart 3"; return; }
	local s3 want; s3="$(state)"; set -- $s3
	[ "$1" = $((NDOCS + 1)) ] && pass "idempotent: the write between the crashes is there ($s3)" || fail "idempotent: got '$s3', expected $((NDOCS + 1)) documents"
	local fresh; fresh="$(curl -s -m10 -XPOST "localhost:$PORT/$IDX/:search?volatile" -d '{"_query":"*","_limit":0,"_aggs":{"f":{"_max":{"_field":"fresh"}}}}' | python3 -c 'import sys,json
try:
    print(json.load(sys.stdin)["aggregations"]["f"]["_max"])
except Exception:
    print("?")')"
	[ "$fresh" = 3 ] || [ "$fresh" = 3.0 ] && pass "idempotent: its new numeric field is visible (max=$fresh)" || fail "idempotent: new numeric field max=$fresh, expected 3"
	[ "$(count_log n4.log 'Read and execute operations WAL')" -ge 1 ] && pass "idempotent: that write was replayed" || fail "idempotent: nothing replayed on the third restart"
	crash
	start_node n5.log || { fail "restart 4"; return; }
	local s4; s4="$(state)"
	same_state "$s4" "$s3" && pass "idempotent: identical state after the third crash ($s4)" || fail "idempotent: '$s4' differs from '$s3'"
	[ "$(count_log n5.log 'Read and execute operations WAL')" = 0 ] && pass "idempotent: nothing replayed again" || fail "idempotent: the fourth start replayed the WAL again"
	crash
}

scenario_failure() {
	echo "== [wal-replay] a commit that fails during the replay surfaces =="
	rm -rf "$RUNDIR/n"
	start_node f1.log || { fail "startup"; return; }
	restore 0 "$NDOCS"
	crash
	# Limit (512-byte blocks) below the 8 KiB tables: the replay's commit writes past it (EFBIG).
	local blocks="${FAIL_BLOCKS:-6}"
	if start_node f2.log "$blocks"; then
		fail "failure: the node served requests although the replay could not commit"
		crash
	else
		pass "failure: the node did not come up on a shard whose replay could not commit"
		wait "$PID" 2>/dev/null; PID=""
	fi
	grep -q "DatabaseError: Error writing block" "$RUNDIR/f2.log" && pass "failure: the commit error is reported" || fail "failure: no commit error in the log"
	start_node f3.log || { fail "healthy restart"; return; }
	local s want; s="$(state)"; want="$(expect "$NDOCS")"
	same_state "$s" "$want" && pass "failure: the next healthy start recovers everything ($s)" || fail "failure: got '$s', expected '$want'"
	crash
}

picked=("$@"); [ ${#picked[@]} -eq 0 ] && picked=(main failure)
for p in "${picked[@]}"; do
	case "$p" in
		visible|walcommit|idempotent|main) rm -rf "$RUNDIR/n"; scenario_main ;;
		failure) scenario_failure ;;
		*) echo "unknown scenario $p"; exit 2 ;;
	esac
done

[ "$fails" = 0 ] && { echo "  PASS"; exit 0; }
echo "  FAILED ($fails assertion(s))"; exit 1
