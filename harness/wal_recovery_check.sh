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

# wal_recovery_check.sh -- every acknowledged write survives kill -9 on a single node.
#
# A shard is a lone-voter group (docs/mechanisms/journal-durability.md): a PUT answers once its
# command is in the journal at the request's `ack` level, so a node killed right after the last
# answer recovers every acknowledged write on restart, from the checkpoint and the journal, with
# no commit in between.
#
# Steps (exit non-zero on any failed assertion):
#   1. Start a node on a fresh data dir.
#   2. PUT N docs, no ?commit.
#   3. kill -9 the node SETTLE seconds after the last answer (default 0: at once).
#   4. Restart on the SAME data dir.
#   5. SEARCH -> assert the recovered count == N.
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
. "$REPO/harness/env.sh"
PORT="${PORT:-$(free_port)}"
NDOCS="${NDOCS:-50}"
SETTLE="${SETTLE:-0}"
IDX="walrec"
RUNDIR="${RUNDIR:-$REPO/.scratch/xwalrec_$$}"
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
	"$BIN" --port "$PORT" --name walrecnode --verbosity 3 -D "$RUNDIR/n" >>"$RUNDIR/$1" 2>&1 &
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

echo "== [wal-recovery] kill -9 after acknowledged writes -> restart recovers them =="

# 1 + 2: start, then index N docs without a commit; each answer is an acknowledgement.
start_node node1.log || { fail "node1 startup"; exit 1; }
acked=0
for i in $(seq 1 "$NDOCS"); do
	code="$(curl -s -m5 -o /dev/null -w '%{http_code}' -XPUT "localhost:$PORT/$IDX/$i" \
		-H 'Content-Type: application/json' -d "{\"n\":$i,\"txt\":\"doc number $i\"}")"
	case "$code" in 2*) acked=$((acked + 1)) ;; esac
done
[ "$acked" = "$NDOCS" ] && pass "$acked/$NDOCS writes acknowledged" || fail "$acked/$NDOCS writes acknowledged"
sleep "$SETTLE"

# 3: crash the node.
kill -9 "$PID" 2>/dev/null
wait "$PID" 2>/dev/null
PID=""

# 4: restart on the same data dir: recovery from the checkpoint and the journal.
start_node node2.log || { fail "node2 (restart) startup"; exit 1; }

# 5: every acknowledged write survived; the commit only refreshes what search sees.
curl -s -m8 -o /dev/null -XPOST "localhost:$PORT/$IDX/:commit"
got="$(search_total)"
if [ "$got" = "$acked" ]; then
	pass "recovered $got/$acked acknowledged docs after kill -9"
else
	fail "recovered $got docs, expected $acked (acknowledged writes lost)"
fi

if [ "$fails" = 0 ]; then
	echo "  PASS"
	exit 0
else
	echo "  FAILED ($fails assertion(s))"
	exit 1
fi
