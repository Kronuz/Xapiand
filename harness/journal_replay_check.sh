#!/usr/bin/env bash
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

# journal_replay_check.sh -- recovery after kill -9 rebuilds what was acknowledged, on a single node.
#
# A shard is a lone-voter group (docs/mechanisms/journal-durability.md): a RESTORE answers once its
# batches are in the journal, and a restart recovers from the checkpoint and the journal.
# Scenarios; exit non-zero on a failed assertion:
#   main     RESTORE N docs with numeric fields without a commit, kill -9, restart: the documents,
#            the schema and the numeric aggregations (avg, max) are visible without a further write;
#            crash and restart again: the state is identical; a write between the crashes (a new
#            numeric field, a schema change) is there once, on top, and survives one more crash.
#   failure  a recovery that cannot write (a file-size limit) does not serve: the node stays down
#            and says why, and the next healthy start recovers everything.
# Usage: harness/journal_replay_check.sh [main|failure ...]   (default: all)
#        BIN=build/bin/xapiand PORT=8908 NDOCS=200 SETTLE=0 (seconds between the last answer and kill -9)
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8908}"
NDOCS="${NDOCS:-200}"
SETTLE="${SETTLE:-0}"
RUNDIR="${RUNDIR:-$REPO/.scratch/xjournalreplay_$$}"
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
		exec "$BIN" --port "$PORT" --name journalreplaynode --verbosity 3 -D "$RUNDIR/n"
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
	sleep "$SETTLE"
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
scenario_main() {
	echo "== [journal-replay] crash, recover, crash, recover =="
	start_node n1.log || { fail "startup"; return; }
	restore 0 "$NDOCS"
	crash
	local want; want="$(expect "$NDOCS")"

	start_node n2.log || { fail "restart 1"; return; }
	local s1; s1="$(state)"
	same_state "$s1" "$want" && pass "visible: $s1 after the first recovery" || fail "visible: got '$s1', expected '$want'"
	crash

	start_node n3.log || { fail "restart 2"; return; }
	local s2; s2="$(state)"
	same_state "$s2" "$s1" && pass "idempotent: identical state after the second crash ($s2)" || fail "idempotent: '$s2' differs from '$s1'"

	# A document with a new numeric field changes the schema: one more write, no commit.
	echo '{"_id":"extra","age":7,"fresh":3}' | curl -s -m30 -o /dev/null -XRESTORE "localhost:$PORT/$IDX/" -H 'Content-Type: application/x-ndjson' --data-binary @-
	sleep "$SETTLE"
	crash
	start_node n4.log || { fail "restart 3"; return; }
	local s3; s3="$(state)"; set -- $s3
	[ "$1" = $((NDOCS + 1)) ] && pass "idempotent: the write between the crashes is there once ($s3)" || fail "idempotent: got '$s3', expected $((NDOCS + 1)) documents"
	local fresh; fresh="$(curl -s -m10 -XPOST "localhost:$PORT/$IDX/:search?volatile" -d '{"_query":"*","_limit":0,"_aggs":{"f":{"_max":{"_field":"fresh"}}}}' | python3 -c 'import sys,json
try:
    print(json.load(sys.stdin)["aggregations"]["f"]["_max"])
except Exception:
    print("?")')"
	[ "$fresh" = 3 ] || [ "$fresh" = 3.0 ] && pass "idempotent: its new numeric field is visible (max=$fresh)" || fail "idempotent: new numeric field max=$fresh, expected 3"
	crash
	start_node n5.log || { fail "restart 4"; return; }
	local s4; s4="$(state)"
	same_state "$s4" "$s3" && pass "idempotent: identical state after the third crash ($s4)" || fail "idempotent: '$s4' differs from '$s3'"
	crash
}

scenario_failure() {
	echo "== [journal-replay] a recovery that cannot write does not serve =="
	rm -rf "$RUNDIR/n"
	start_node f1.log || { fail "startup"; return; }
	restore 0 "$NDOCS"
	crash
	# A file-size limit (512-byte blocks) small enough that recovery's writes go past it (EFBIG).
	local blocks="${FAIL_BLOCKS:-6}"
	if start_node f2.log "$blocks"; then
		fail "failure: the node served requests although its recovery could not write"
		crash
	else
		pass "failure: the node did not come up on a shard whose recovery could not write"
		wait "$PID" 2>/dev/null; PID=""
	fi
	grep -qiE "file too large|EFBIG" "$RUNDIR/f2.log" && pass "failure: the write error is reported" || fail "failure: no file-size error in the log"
	start_node f3.log || { fail "healthy restart"; return; }
	local s want; s="$(state)"; want="$(expect "$NDOCS")"
	same_state "$s" "$want" && pass "failure: the next healthy start recovers everything ($s)" || fail "failure: got '$s', expected '$want'"
	crash
}

picked=("$@"); [ ${#picked[@]} -eq 0 ] && picked=(main failure)
for p in "${picked[@]}"; do
	case "$p" in
		main) rm -rf "$RUNDIR/n"; scenario_main ;;
		failure) scenario_failure ;;
		*) echo "unknown scenario $p"; exit 2 ;;
	esac
done

[ "$fails" = 0 ] && { echo "  PASS"; exit 0; }
echo "  FAILED ($fails assertion(s))"; exit 1
