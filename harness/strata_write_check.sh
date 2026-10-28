#!/usr/bin/env bash
# strata_write_check.sh -- the strata engine's write slice through the server, on a single node.
#
# Runs a node with XAPIAND_ENGINE=strata (the index is created on strata; the registry stays Xapian) and
# checks what the write slice serves without search (S2): documents by id, versions, deletes, and WAL
# replay after kill -9 (the strata variant of wal_replay_check.sh, which asserts through search).
#   write     PUT, GET, replace (version 2), DELETE; the shard is a strata directory
#   replay    RESTORE N docs without a commit, kill -9, restart: every document is served by id; a second
#             crash replays nothing and loses nothing; a write between the crashes is replayed once
# Usage: harness/strata_write_check.sh [write|replay ...]   (default: both)
#        BIN=build/bin/xapiand PORT=8909 NDOCS=200
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8909}"
NDOCS="${NDOCS:-200}"
RUNDIR="${RUNDIR:-$REPO/.scratch/xstrata_$$}"
IDX="sw"
mkdir -p "$RUNDIR"
PID=""
fails=0
cleanup() { [ -n "$PID" ] && kill -9 "$PID" 2>/dev/null; [ "${KEEP_RUNDIR:-0}" = 1 ] || rm -rf "$RUNDIR"; }
trap cleanup EXIT
fail() { echo "  FAIL: $*"; fails=$((fails + 1)); }
pass() { echo "  ok: $*"; }

start_node() {  # start_node LOG
	local log="$RUNDIR/$1"
	(
		exec > >(cat >>"$log") 2>&1
		XAPIAND_ENGINE=strata exec "$BIN" --port "$PORT" --solo --name strataw --verbosity 3 -D "$RUNDIR/n"
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
http() { curl -s -m15 -o "$RUNDIR/body" -w '%{http_code}' "$@"; }
field() {  # field NAME: a field of the last body
	python3 -c "
import json, sys
try:
    d = json.load(open('$RUNDIR/body')); print(d.get('$1', '?'))
except Exception:
    print('?')"
}
restore() {  # restore FIRST LAST: loaded without a commit
	python3 -c "
import json
for i in range($1, $2):
    print(json.dumps({'_id': 'u%d' % i, 'age': i % 40}))" > "$RUNDIR/docs.ndjson"
	curl -s -m60 -o /dev/null -XRESTORE "localhost:$PORT/$IDX/" -H 'Content-Type: application/x-ndjson' --data-binary @"$RUNDIR/docs.ndjson"
	sleep 0.5
}
served() {  # served FIRST LAST: how many of the ids are served with the right age
	local ok=0 i
	for i in $(seq "$1" $(($2 - 1))); do
		[ "$(http "localhost:$PORT/$IDX/u$i")" = 200 ] && [ "$(field age)" = "$((i % 40))" ] && ok=$((ok + 1))
	done
	echo "$ok"
}
count_log() { grep -c "$2" "$RUNDIR/$1" 2>/dev/null || true; }
strata_dirs() { find "$RUNDIR/n/$IDX" -type d -name strata 2>/dev/null | wc -l | tr -d ' '; }
glass_files() { find "$RUNDIR/n/$IDX" -name iamglass 2>/dev/null | wc -l | tr -d ' '; }

scenario_write() {
	echo "== [strata-write] PUT, GET, replace, DELETE =="
	rm -rf "$RUNDIR/n"
	start_node w1.log || { fail "startup"; return; }
	http -XPUT "localhost:$PORT/$IDX/a" -H 'Content-Type: application/json' -d '{"name":"one","n":1}' > /dev/null
	[ "$(http "localhost:$PORT/$IDX/a")" = 200 ] && [ "$(field name)" = one ] && pass "GET serves the document written" || fail "GET after PUT: $(cat "$RUNDIR/body")"
	http -XPUT "localhost:$PORT/$IDX/a" -H 'Content-Type: application/json' -d '{"name":"two","n":2}' > /dev/null
	[ "$(http "localhost:$PORT/$IDX/a")" = 200 ] && [ "$(field name)" = two ] && pass "a replace is served" || fail "GET after replace: $(cat "$RUNDIR/body")"
	[ "$(field _version)" = 2 ] && pass "its version is 2" || fail "version after replace: $(field _version)"
	[ "$(http -XDELETE "localhost:$PORT/$IDX/a")" = 200 ] && pass "DELETE" || fail "DELETE: $(cat "$RUNDIR/body")"
	[ "$(http "localhost:$PORT/$IDX/a")" = 404 ] && pass "a deleted document is 404" || fail "GET after DELETE"
	[ "$(strata_dirs)" -ge 1 ] && [ "$(glass_files)" = 0 ] && pass "the index is a strata directory ($(strata_dirs) shard(s)), no glass files" || fail "engine on disk: strata=$(strata_dirs) glass=$(glass_files)"
	crash
}

scenario_replay() {
	echo "== [strata-write] crash, replay, crash, replay =="
	rm -rf "$RUNDIR/n"
	start_node r1.log || { fail "startup"; return; }
	restore 0 "$NDOCS"
	local before; before="$(served 0 "$NDOCS")"
	[ "$before" = "$NDOCS" ] && pass "all $NDOCS documents are served before the crash" || fail "before the crash: $before of $NDOCS"
	crash

	start_node r2.log || { fail "restart 1"; return; }
	local s1; s1="$(served 0 "$NDOCS")"
	[ "$s1" = "$NDOCS" ] && pass "all $NDOCS documents are served after the first replay" || fail "after replay: $s1 of $NDOCS"
	[ "$(count_log r2.log 'Read and execute operations WAL')" -ge 1 ] && pass "the first restart replayed the WAL" || pass "the first restart found everything committed (nothing to replay)"
	crash

	start_node r3.log || { fail "restart 2"; return; }
	local s2; s2="$(served 0 "$NDOCS")"
	[ "$s2" = "$NDOCS" ] && pass "idempotent: all documents after the second crash" || fail "second crash: $s2 of $NDOCS"
	[ "$(count_log r3.log 'WAL ERROR')" = 0 ] && pass "no WAL error" || fail "WAL error on the second restart"

	http -XPUT "localhost:$PORT/$IDX/extra" -H 'Content-Type: application/json' -d '{"age":7}' > /dev/null
	crash
	start_node r4.log || { fail "restart 3"; return; }
	[ "$(http "localhost:$PORT/$IDX/extra")" = 200 ] && [ "$(field age)" = 7 ] && pass "the write between the crashes is served" || fail "extra after crash: $(cat "$RUNDIR/body")"
	[ "$(served 0 "$NDOCS")" = "$NDOCS" ] && pass "and the rest is intact" || fail "rest after the third crash"
	crash
}

picked=("$@"); [ ${#picked[@]} -eq 0 ] && picked=(write replay)
for p in "${picked[@]}"; do
	case "$p" in
		write) scenario_write ;;
		replay) scenario_replay ;;
		*) echo "unknown scenario $p"; exit 2 ;;
	esac
done

[ "$fails" = 0 ] && { echo "  PASS"; exit 0; }
echo "  FAILED ($fails assertion(s))"; exit 1
