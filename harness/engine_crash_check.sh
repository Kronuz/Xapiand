#!/usr/bin/env bash
# engine_crash_check.sh -- kill -9 during indexing, over the same input (gate 7a, T8).
#
# On the strata engine:
#   A. PUT N documents without a commit, kill -9 before the autocommit, restart, "poke" every shard (a write opens a
#      shard writable and replays its WAL; replay is lazy), then list the document set.
#   B. A writer PUTs documents one after another (acknowledgement times logged); kill -9 CUT seconds in; restart, poke, list the set.
#   C. The engine's own check (index::check: the digest of every file, plus the field table) runs on every shard
#      after each kill -9 and after the restart, through tests/strata_check_tool.cc (CHECK_TOOL).
# Compared: A's set equals the input (N ids, every `n` right). B's set holds every write
# acknowledged more than GRACE seconds before the kill (the async WAL may lose the last moments), nothing that was not
# written, every document intact (the cut point differs per run). Exit 0 = all checks passed.
#   D. (--reactors N, default 2) N writers on N HTTP reactors put sequences into ONE one-shard index; kill -9; restart; poke; list. Every write
#      acknowledged more than GRACE seconds before the kill is present, and what each writer recovered is a prefix of what it sent:
#      the WAL's order is the apply order across reactors (phase 7c D5, C6).
# Usage: BIN=build/bin/xapiand harness/engine_crash_check.sh [--reactors N]
set -u
REACTORS=2
while [ $# -gt 0 ]; do
	case "$1" in
		--reactors) REACTORS="$2"; shift 2 ;;
		*) echo "usage: $0 [--reactors N]"; exit 2 ;;
	esac
done
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8931}"
CHECK_TOOL="${CHECK_TOOL:-$(dirname "$BIN")/strata_check}"   # cmake --build build --target strata_check
NDOCS="${NDOCS:-50}"
IDX="walrec"
RUNDIR="${RUNDIR:-$REPO/.scratch/xcrash_$$}"
mkdir -p "$RUNDIR"
PID=""; BG=""
fails=0
cleanup() { [ -n "$BG" ] && kill -9 "$BG" 2>/dev/null; [ -n "$PID" ] && kill -9 "$PID" 2>/dev/null; [ "${KEEP_RUNDIR:-0}" = 1 ] || rm -rf "$RUNDIR"; }
trap cleanup EXIT
fail() { echo "  FAIL: $*"; fails=$((fails + 1)); }
pass() { echo "  ok: $*"; }

start_node() {  # start_node ENGINE LOG
	"$BIN" --port "$PORT" --name crashnode --verbosity 3 --processors "$REACTORS" -D "$RUNDIR/n_$1" >>"$RUNDIR/$2" 2>&1 &
	PID=$!
	for _ in $(seq 80); do
		kill -0 "$PID" 2>/dev/null || { echo "  node died during startup"; return 1; }
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	return 1
}
crash() { kill -9 "$PID" 2>/dev/null; wait "$PID" 2>/dev/null; PID=""; }
poke() {  # a write to every shard, then delete the pokes
	local i
	for i in $(seq 0 39); do curl -s -m15 -o /dev/null -XPUT "localhost:$PORT/$1/poke$i" -H 'Content-Type: application/json' -d '{"poke":true}'; done
	for i in $(seq 0 39); do curl -s -m15 -o /dev/null -XDELETE "localhost:$PORT/$1/poke$i"; done
	curl -s -m60 -o /dev/null -XPOST "localhost:$PORT/$1/:commit"   # replayed writes show in a search after a commit
}
list_set() {  # list_set INDEX OUT: "_id n" lines, sorted, of every document (pages of 1000)
	python3 -I - "$PORT" "$1" "$2" <<'PY'
import sys, json, http.client
port, idx, out = sys.argv[1], sys.argv[2], sys.argv[3]
rows, off = [], 0
while True:
    c = http.client.HTTPConnection('localhost', int(port), timeout=120)
    c.request('POST', f'/{idx}/:search', body=json.dumps({'_query': '*', '_limit': 1000, '_offset': off, '_check_at_least': 100000, '_sort': '_id'}), headers={'Content-Type': 'application/json'})
    r = c.getresponse(); d = json.loads(r.read() or b'{}')
    hits = d.get('hits', [])
    if not hits: break
    rows += [(h['_id'], h.get('n')) for h in hits]
    off += len(hits)
open(out, 'w').write(''.join(f'{i} {n}\n' for i, n in sorted(rows, key=lambda x: str(x[0]))))
PY
}
check_digest() {  # check_digest INDEX LABEL: the strata engine's check (every file's digest) passes on each shard; node stopped
	local out
	if [ -x "$CHECK_TOOL" ]; then
		local shards=("$RUNDIR"/n_strata/"$1"/.__*)
		[ -e "${shards[0]}" ] || shards=("$RUNDIR"/n_strata/"$1")   # a one-shard index has no shard subdirectories
		out=$("$CHECK_TOOL" "${shards[@]}" 2>&1) && pass "$2: strata check passes on $(echo "$out" | grep -c ': ok') shards" || fail "$2: strata check: $(echo "$out" | grep -v ': ok' | head -3 | tr '\n' ';')"
	else
		fail "$2: no CHECK_TOOL (the strata_check target, tests/strata_check_tool.cc) to run the strata check"
	fi
}

for ENG in strata; do
	echo "== [crash $ENG] A: $NDOCS PUTs without commit, kill -9, restart, replay =="
	start_node "$ENG" "$ENG.A1.log" || { fail "$ENG startup"; continue; }
	for i in $(seq 1 "$NDOCS"); do
		curl -s -m5 -o /dev/null -XPUT "localhost:$PORT/$IDX/$i" -H 'Content-Type: application/json' -d "{\"n\":$i,\"txt\":\"doc number $i\"}"
	done
	sleep 1
	crash
	[ "$ENG" != strata ] || check_digest "$IDX" "$ENG right after the kill -9 (A)"
	start_node "$ENG" "$ENG.A2.log" || { fail "$ENG restart"; continue; }
	poke "$IDX"
	list_set "$IDX" "$RUNDIR/A_$ENG"
	seq 1 "$NDOCS" | awk '{print $1" "$1}' | sort > "$RUNDIR/A_input"
	sort "$RUNDIR/A_$ENG" > "$RUNDIR/A_$ENG.s"
	cmp -s "$RUNDIR/A_input" "$RUNDIR/A_$ENG.s" && pass "$ENG: recovered set = input ($(wc -l < "$RUNDIR/A_$ENG.s" | tr -d ' ') documents, every n right)" || fail "$ENG: recovered $(wc -l < "$RUNDIR/A_$ENG.s" | tr -d ' ') of $NDOCS, diff: $(diff "$RUNDIR/A_input" "$RUNDIR/A_$ENG.s" | head -4 | tr '\n' ';')"
	grep -q "Read and execute operations WAL" "$RUNDIR/$ENG.A2.log" && pass "$ENG: the restart logged a WAL replay" || fail "$ENG: no WAL replay logged"
	crash
	[ "$ENG" != strata ] || check_digest "$IDX" "$ENG after the restart and a second kill (A)"
	echo "== [crash $ENG] B: a writer PUTs documents one after another, kill -9 ${CUT:-4}s in =="
	start_node "$ENG" "$ENG.B1.log" || { fail "$ENG startup B"; continue; }
	python3 -I - "$PORT" "$RUNDIR/acked_$ENG" <<'PY' &
import sys, json, time, http.client
c = http.client.HTTPConnection('localhost', int(sys.argv[1]), timeout=30)
with open(sys.argv[2], 'w', buffering=1) as log:
    i = 0
    while True:
        i += 1
        try:
            c.request('PUT', f'/bulk/{i}', body=json.dumps({'n': i, 'txt': f'doc number {i}'}), headers={'Content-Type': 'application/json'})
            r = c.getresponse(); r.read()
        except Exception:
            break
        if r.status in (200, 201, 204):
            log.write(f'{i} {time.time():.3f}\n')
PY
	BG=$!
	sleep "${CUT:-4}"
	killed=$(python3 -c 'import time; print(f"{time.time():.3f}")')
	crash
	kill -9 "$BG" 2>/dev/null; wait "$BG" 2>/dev/null; BG=""
	[ "$ENG" != strata ] || check_digest bulk "$ENG right after the kill -9 (B)"
	start_node "$ENG" "$ENG.B2.log" || { fail "$ENG restart B"; continue; }
	poke bulk
	list_set bulk "$RUNDIR/B_$ENG"
	nb=$(wc -l < "$RUNDIR/B_$ENG" | tr -d ' ')
	na=$(wc -l < "$RUNDIR/acked_$ENG" | tr -d ' ')
	read -r bad dup lost late <<<"$(python3 -I - "$RUNDIR/B_$ENG" "$RUNDIR/acked_$ENG" "$killed" "${GRACE:-2}" <<'PY'
import sys
rec = [l.split() for l in open(sys.argv[1])]
ids = [int(r[0]) for r in rec]; bad = sum(1 for r in rec if r[0] != r[1])
acked = [(int(a), float(t)) for a, t in (l.split() for l in open(sys.argv[2]))]
killed, grace = float(sys.argv[3]), float(sys.argv[4])
have = set(ids)
lost = sum(1 for i, t in acked if t <= killed - grace and i not in have)       # acknowledged long before the kill, missing
late = sum(1 for i, t in acked if t > killed - grace and i not in have)        # inside the async-WAL window, missing
print(bad, len(ids) - len(have), lost, late)
PY
)"
	[ "$bad" = 0 ] && [ "$dup" = 0 ] && [ "$lost" = 0 ] && [ "$nb" -le $((na + 1)) ] && pass "$ENG: $na writes acknowledged, $nb recovered; none acknowledged more than ${GRACE:-2}s before the kill is missing ($late newer ones missing), all intact, none duplicated" || fail "$ENG: B acknowledged $na, recovered $nb, $bad wrong, $dup duplicated, $lost old acknowledged writes missing"
	crash
	[ "$ENG" != strata ] || check_digest bulk "$ENG after the restart and a second kill (B)"

	echo "== [crash $ENG] D: $REACTORS writers on $REACTORS reactors into one index, kill -9 ${CUT:-4}s in =="
	start_node "$ENG" "$ENG.D1.log" || { fail "$ENG startup D"; continue; }
	# One shard, so one log: what a crash loses is a suffix of the log's order, which is the order the writes were applied in.
	curl -s -m15 -o /dev/null -XPUT "localhost:$PORT/multi/" -H 'Content-Type: application/json' -d '{"_settings":{"number_of_shards":1}}'
	WBG=""
	for w in $(seq 1 "$REACTORS"); do
		python3 -I - "$PORT" "$RUNDIR/acked_D$w" "$w" <<'PY' &
import sys, json, time, http.client
c = http.client.HTTPConnection('localhost', int(sys.argv[1]), timeout=30)
w = sys.argv[3]
with open(sys.argv[2], 'w', buffering=1) as log:
    i = 0
    while True:
        i += 1
        try:
            c.request('PUT', f'/multi/w{w}-{i}', body=json.dumps({'w': int(w), 'n': i, 'txt': f'writer {w} doc {i}'}), headers={'Content-Type': 'application/json'})
            r = c.getresponse(); r.read()
        except Exception:
            break
        if r.status in (200, 201, 204):
            log.write(f'{i} {time.time():.3f}\n')
PY
		WBG="$WBG $!"
	done
	sleep "${CUT:-4}"
	killed=$(python3 -c 'import time; print(f"{time.time():.3f}")')
	crash
	for b in $WBG; do kill -9 "$b" 2>/dev/null; wait "$b" 2>/dev/null; done
	[ "$ENG" != strata ] || check_digest multi "$ENG right after the kill -9 (D)"
	start_node "$ENG" "$ENG.D2.log" || { fail "$ENG restart D"; continue; }
	poke multi
	list_set multi "$RUNDIR/D_$ENG"
	read -r bad dup lost gaps total <<<"$(python3 -I - "$RUNDIR/D_$ENG" "$RUNDIR" "$killed" "${GRACE:-2}" "$REACTORS" <<'PY'
import sys, re
rows = [l.split() for l in open(sys.argv[1])]
rundir, killed, grace, n = sys.argv[2], float(sys.argv[3]), float(sys.argv[4]), int(sys.argv[5])
got = {}
dup = bad = 0
for _id, num in rows:
    m = re.fullmatch(r'w(\d+)-(\d+)', _id)
    if not m or str(num) != m.group(2):
        bad += 1; continue
    s = got.setdefault(int(m.group(1)), set())
    if int(m.group(2)) in s: dup += 1
    s.add(int(m.group(2)))
lost = gaps = 0
for w in range(1, n + 1):
    acked = [(int(a), float(t)) for a, t in (l.split() for l in open(f'{rundir}/acked_D{w}'))]
    have = got.get(w, set())
    lost += sum(1 for i, t in acked if t <= killed - grace and i not in have)
    # what a writer recovered is a prefix of what it sent: no id is missing below its highest recovered one
    gaps += sum(1 for i in range(1, max(have, default=0) + 1) if i not in have)
print(bad, dup, lost, gaps, len(rows))
PY
)"
	[ "$bad" = 0 ] && [ "$dup" = 0 ] && [ "$lost" = 0 ] && [ "$gaps" = 0 ] && [ "$total" -gt 0 ] && pass "$ENG: $REACTORS writers, $total documents recovered; none acknowledged more than ${GRACE:-2}s before the kill is missing, each writer's set is a prefix of its sequence (WAL order = apply order)" || fail "$ENG: D recovered $total, $bad wrong, $dup duplicated, $lost old acknowledged writes missing, $gaps gaps below a writer's highest id"
	crash
	[ "$ENG" != strata ] || check_digest multi "$ENG after the restart and a second kill (D)"
done
if [ "$fails" = 0 ]; then echo "PASS"; else echo "FAIL ($fails)"; fi
[ "$fails" = 0 ]
