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

# strata_write_check.sh -- the strata engine's write slice through the server, on a single node.
#
# Runs a node with (the index is created on strata; the registry stays Xapian) and
# checks what the write slice serves without search (S2): documents by id, versions, deletes, and WAL
# replay after kill -9 (the strata variant of wal_replay_check.sh, which asserts through search).
#   write     PUT, GET, replace (version 2), DELETE; the shard is a strata directory
#   replay    PUT N docs without a commit, kill -9, restart (a write to every shard opens it writable and
#             replays its WAL): every document is served by id; a crash after the commit loses nothing;
#             a write between the crashes is replayed
#   versions  writes, replaces and deletes with known versions; kill -9; after each replay every document is at the
#             version and value the model acknowledged (harness/strata_write_model.py), through a second crash
#   commit    the node kills itself at a commit point (XAPIAND_STRATA_CRASH, a -DCRASH_POINTS=ON build): before the
#             commit, after the segment is flushed and before the manifest, after it; random writes and deletes
#             run until it dies; restart, replay, verify the model; crash again, verify again
#   replaycrash  kill -9 under load, then the node kills itself in the middle of the replay, twice at different
#             lines; the next restart converges, and so does one more crash after it
# Usage: harness/strata_write_check.sh [write|replay|versions|commit|replaycrash ...]   (default: write replay)
#        BIN=build/bin/xapiand PORT=8909 NDOCS=30
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8909}"
NDOCS="${NDOCS:-30}"
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
		XAPIAND_STRATA_CRASH="${HOOK:-}" exec "$BIN" --port "$PORT" --name strataw --verbosity 3 -D "$RUNDIR/n"
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
wal_dump() {  # the :wal endpoint of every shard answers (it decodes the strata documents in the log)
	local s ok=0
	for s in 1 2 3 4 5; do
		[ "$(http "localhost:$PORT/$IDX/.__$s/:wal")" = 200 ] && ok=$((ok + 1))
	done
	echo "$ok"
}
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
	local deleted; deleted="$(http -XDELETE "localhost:$PORT/$IDX/a")"
	[ "$deleted" = 200 ] || [ "$deleted" = 204 ] && pass "DELETE ($deleted)" || fail "DELETE: $deleted $(cat "$RUNDIR/body")"
	[ "$(http "localhost:$PORT/$IDX/a")" = 404 ] && pass "a deleted document is 404" || fail "GET after DELETE"
	[ "${ENGINE:-strata}" != strata ] || { [ "$(strata_dirs)" -ge 1 ] && [ "$(glass_files)" = 0 ]; } && pass "the index is a strata directory ($(strata_dirs) shard(s)), no glass files" || fail "engine on disk: strata=$(strata_dirs) glass=$(glass_files)"
	crash
}

put_docs() {  # put_docs FIRST LAST: one PUT each, so nothing commits before the crash
	local i
	for i in $(seq "$1" $(($2 - 1))); do
		curl -s -m15 -o /dev/null -XPUT "localhost:$PORT/$IDX/u$i" -H 'Content-Type: application/json' -d "{\"age\": $((i % 40))}"
	done
	sleep 1
}
poke() {  # a write to every shard: opens each writable, which replays its WAL (replay is lazy, as with Xapian)
	local i
	for i in $(seq 0 39); do
		curl -s -m15 -o /dev/null -XPUT "localhost:$PORT/$IDX/poke$i" -H 'Content-Type: application/json' -d '{"poke":true}'
	done
}

scenario_replay() {
	echo "== [strata-write] crash, replay, crash, replay =="
	rm -rf "$RUNDIR/n"
	start_node r1.log || { fail "startup"; return; }
	put_docs 0 "$NDOCS"   # few enough to crash before the autocommit (about 3 s)
	[ "$(wal_dump)" = 5 ] && pass "the :wal endpoint decodes the log of every shard" || fail ":wal answered for $(wal_dump) of 5 shards"
	crash

	start_node r2.log || { fail "restart 1"; return; }
	poke
	local s1; s1="$(served 0 "$NDOCS")"
	[ "$s1" = "$NDOCS" ] && pass "all $NDOCS documents are served after the first replay" || fail "after replay: $s1 of $NDOCS"
	[ "$(count_log r2.log "Read and execute operations WAL file ('$IDX/")" -ge 1 ] && pass "the first restart replayed the WAL" || fail "no replay logged on the first restart"
	sleep 5   # the autocommit commits the replay
	crash

	start_node r3.log || { fail "restart 2"; return; }
	poke
	local s2; s2="$(served 0 "$NDOCS")"
	[ "$s2" = "$NDOCS" ] && pass "idempotent: all documents after the second crash" || fail "second crash: $s2 of $NDOCS"
	[ "$(count_log r3.log 'WAL ERROR')" = 0 ] && pass "no WAL error" || fail "WAL error on the second restart"

	http -XPUT "localhost:$PORT/$IDX/extra" -H 'Content-Type: application/json' -d '{"age":7}' > /dev/null
	sleep 1.5
	crash
	start_node r4.log || { fail "restart 3"; return; }
	poke
	[ "$(http "localhost:$PORT/$IDX/extra")" = 200 ] && [ "$(field age)" = 7 ] && pass "the write between the crashes is served" || fail "extra after crash: $(cat "$RUNDIR/body")"
	[ "$(served 0 "$NDOCS")" = "$NDOCS" ] && pass "and the rest is intact" || fail "rest after the third crash"
	[ "$(count_log r4.log "Read and execute operations WAL file ('$IDX/")" -ge 1 ] && pass "that write was replayed" || fail "nothing replayed on the third restart"
	crash
}

MODEL="$RUNDIR/model.json"
model() { local c="$1"; shift; python3 "$REPO/harness/strata_write_model.py" "$c" "$PORT" "$IDX" "$MODEL" "$@"; }
ids() { local i; for i in $(seq "$1" "$2" "$3"); do printf 'u%d ' "$i"; done; }  # ids FIRST STEP LAST
check_model() {  # check_model LABEL: every id is at the version and value the model acknowledged
	local out
	if out="$(model verify 2>&1)"; then pass "$1: $out"; else fail "$1: $out"; fi
}
died() {  # died LABEL: the node killed itself (a crash point fired)
	local _
	for _ in $(seq 40); do
		kill -0 "$PID" 2>/dev/null || { wait "$PID" 2>/dev/null; PID=""; pass "$1: the node died at its crash point"; return 0; }
		sleep 0.5
	done
	fail "$1: the crash point did not fire (is the build -DCRASH_POINTS=ON?)"
	crash
	return 1
}

scenario_versions() {
	echo "== [strata-write] versions and values through crashes =="
	rm -rf "$RUNDIR/n" "$MODEL"
	local last=$((NDOCS - 1))
	start_node v1.log || { fail "startup"; return; }
	model put $(ids 0 1 "$last")      # version 1 everywhere
	model put $(ids 0 2 "$last")      # version 2 on the evens
	model put $(ids 0 3 "$last")      # version 3 on multiples of three (6 is 3)
	model del $(ids 4 5 "$last")      # deleted
	model put $(ids 4 10 "$last")     # a deleted key written again
	check_model "before the crash"
	crash
	start_node v2.log || { fail "restart 1"; return; }
	poke
	check_model "after the first replay"
	[ "$(count_log v2.log "Read and execute operations WAL file ('$IDX/")" -ge 1 ] && pass "the restart replayed the WAL" || fail "no replay logged"
	sleep 5   # the autocommit commits the replay
	crash
	start_node v3.log || { fail "restart 2"; return; }
	poke
	check_model "after the second crash"
	model put $(ids 0 4 "$last")
	model del $(ids 1 7 "$last")
	sleep 1.5
	crash
	start_node v4.log || { fail "restart 3"; return; }
	poke
	check_model "writes between the crashes"
	crash
}

scenario_commit() {
	echo "== [strata-write] the node dies at a commit point =="
	local point
	for point in commit.before commit.flushed commit.after; do
		rm -rf "$RUNDIR/n" "$MODEL"
		HOOK="$point:3" start_node c1.log || { fail "startup ($point)"; continue; }
		model load 12 40   # until the node dies; the autocommit is about 3 s in
		died "$point" || continue
		start_node c2.log || { fail "restart ($point)"; continue; }
		poke
		check_model "$point: after the restart"
		[ "$(count_log c2.log 'WAL ERROR')" = 0 ] && pass "$point: no WAL error" || fail "$point: WAL error on the restart"
		model load 1 40
		sleep 5
		crash
		start_node c3.log || { fail "restart 2 ($point)"; continue; }
		poke
		check_model "$point: after a second crash"
		crash
	done
}

scenario_replaycrash() {
	echo "== [strata-write] the node dies in the middle of the replay =="
	rm -rf "$RUNDIR/n" "$MODEL"
	start_node p1.log || { fail "startup"; return; }
	model load 1.5 40   # under the autocommit, so the log holds all of it
	crash
	HOOK="replay:5" start_node p2.log || { fail "restart 1"; return; }
	poke
	died "the first replay" || return
	HOOK="replay:2" start_node p3.log || { fail "restart 2"; return; }
	poke
	died "the second replay" || return
	start_node p4.log || { fail "restart 3"; return; }
	poke
	check_model "after two crashes in the replay"
	sleep 5
	crash
	start_node p5.log || { fail "restart 4"; return; }
	poke
	check_model "after one more crash"
	[ "$(count_log p5.log 'WAL ERROR')" = 0 ] && pass "no WAL error" || fail "WAL error on the last restart"
	crash
}

picked=("$@"); [ ${#picked[@]} -eq 0 ] && picked=(write replay)
for p in "${picked[@]}"; do
	case "$p" in
		write) scenario_write ;;
		replay) scenario_replay ;;
		versions) scenario_versions ;;
		commit) scenario_commit ;;
		replaycrash) scenario_replaycrash ;;
		*) echo "unknown scenario $p"; exit 2 ;;
	esac
done

[ "$fails" = 0 ] && { echo "  PASS"; exit 0; }
echo "  FAILED ($fails assertion(s))"; exit 1
