#!/usr/bin/env bash
# engine_migration_check.sh -- _dump on one engine, _restore on the other, both ways, over the bank dataset.
#
# Two solo nodes run at once, one per engine (XAPIAND_ENGINE picks the engine of an index at creation):
#   1. Xapian node: RESTORE accounts.ndjson into /bank/, DUMP it                 -> dump_x1
#   2. strata node: RESTORE dump_x1 into /bank/, DUMP it                         -> dump_s1   (xapian -> strata)
#   3. Xapian node: RESTORE dump_s1 into /bank2/, DUMP it                        -> dump_x2   (strata -> xapian)
# Each dump is compared record by record (sorted by _id, canonical JSON): dump_x1 = dump_s1 = dump_x2.
# The engine of each index is checked on disk (a `strata` directory, or Xapian's iamglass).
# Usage: BIN=build/bin/xapiand harness/engine_migration_check.sh     exit 0 = all checks passed
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
DATA="${DATA:-$REPO/docs/public/assets/accounts.ndjson}"
PX="${PX:-8921}"; PS="${PS:-8922}"
RUNDIR="${RUNDIR:-$REPO/.scratch/xmig_$$}"
mkdir -p "$RUNDIR"
PIDS=()
fails=0
cleanup() { for p in "${PIDS[@]:-}"; do [ -n "$p" ] && kill -9 "$p" 2>/dev/null; done; [ "${KEEP_RUNDIR:-0}" = 1 ] || rm -rf "$RUNDIR"; }
trap cleanup EXIT
fail() { echo "  FAIL: $*"; fails=$((fails + 1)); }
pass() { echo "  ok: $*"; }

start_node() {  # start_node ENGINE PORT
	XAPIAND_ENGINE="$1" "$BIN" --port "$2" --solo --name "mig$1" --verbosity 2 -D "$RUNDIR/$1" >"$RUNDIR/$1.log" 2>&1 &
	PIDS+=($!)
	for _ in $(seq 80); do
		kill -0 "$!" 2>/dev/null || { echo "  node $1 died"; return 1; }
		curl -s --max-time 3 -o /dev/null "localhost:$2/" 2>/dev/null && return 0
		sleep 0.5
	done
	return 1
}
restore() {  # restore PORT INDEX FILE [CONTENT-TYPE]
	curl -s -m300 -o "$RUNDIR/restore.out" -w '%{http_code}' -XRESTORE "localhost:$1/$2/?commit" -H "Content-Type: ${4:-application/json}" --data-binary @"$3"
}
dump() {  # dump PORT INDEX OUT   (no Accept header: the server answers a JSON array; any explicit Accept is a 406)
	curl -s -m300 -o "$3" -w '%{http_code}' -XDUMP "localhost:$1/$2/"
}
canon() {  # canon FILE [ignore-version]: records sorted by _id, canonical JSON, one per line
	python3 -I - "$1" "${2:-}" <<'PY'
import sys, json
txt = open(sys.argv[1]).read()
recs = json.loads(txt) if txt.lstrip().startswith('[') else [json.loads(l) for l in txt.splitlines() if l.strip()]
recs.sort(key=lambda r: str(r.get('_id')))
if sys.argv[2]:
    for r in recs: r.pop('_version', None)
for r in recs: print(json.dumps(r, sort_keys=True, separators=(',', ':')))
PY
}
same() {  # same LABEL A B [ignore-version]
	canon "$2" "${4:-}" > "$2.c${4:-}"; canon "$3" "${4:-}" > "$3.c${4:-}"
	local na nb; na=$(wc -l < "$2.c${4:-}" | tr -d ' '); nb=$(wc -l < "$3.c${4:-}" | tr -d ' ')
	if cmp -s "$2.c${4:-}" "$3.c${4:-}" && [ "$na" -gt 0 ]; then pass "$1: $na records, identical${4:+ (ignoring _version)}"; else fail "$1: $na vs $nb records, $(diff "$2.c${4:-}" "$3.c${4:-}" | grep -c '^<') differ: $(diff "$2.c${4:-}" "$3.c${4:-}" | head -2 | cut -c1-120 | tr '\n' ' ')"; fi
}

echo "== [engine-migration] bank: xapian -> strata -> xapian =="
rm -rf "$RUNDIR/xapian" "$RUNDIR/strata"
start_node xapian "$PX" || { fail "xapian node startup"; exit 1; }
start_node strata "$PS" || { fail "strata node startup"; exit 1; }
[ "$(restore "$PX" bank "$DATA" application/x-ndjson)" = 200 ] && pass "RESTORE accounts.ndjson on xapian" || fail "restore on xapian: $(head -c 300 "$RUNDIR/restore.out")"
[ "$(dump "$PX" bank "$RUNDIR/dump_x1")" = 200 ] && pass "DUMP on xapian" || fail "dump on xapian"
[ "$(restore "$PS" bank "$RUNDIR/dump_x1")" = 200 ] && pass "RESTORE the xapian dump on strata" || fail "restore on strata: $(head -c 300 "$RUNDIR/restore.out")"
[ "$(dump "$PS" bank "$RUNDIR/dump_s1")" = 200 ] && pass "DUMP on strata" || fail "dump on strata"
[ "$(restore "$PX" bank2 "$RUNDIR/dump_s1")" = 200 ] && pass "RESTORE the strata dump on xapian" || fail "restore on xapian 2: $(head -c 300 "$RUNDIR/restore.out")"
[ "$(dump "$PX" bank2 "$RUNDIR/dump_x2")" = 200 ] && pass "DUMP on xapian again" || fail "dump on xapian 2"
# control: the xapian dump restored on xapian too (the xapian restore of a dump is the same whatever engine made the dump)
[ "$(restore "$PX" bank3 "$RUNDIR/dump_x1")" = 200 ] && [ "$(dump "$PX" bank3 "$RUNDIR/dump_x3")" = 200 ] && pass "control: RESTORE and DUMP of the xapian dump on xapian" || fail "control restore"
sx=$(find "$RUNDIR/xapian/bank" -type d -name strata 2>/dev/null | wc -l | tr -d ' '); gx=$(find "$RUNDIR/xapian/bank" -name iamglass 2>/dev/null | wc -l | tr -d ' ')
ss=$(find "$RUNDIR/strata/bank" -type d -name strata 2>/dev/null | wc -l | tr -d ' '); gs=$(find "$RUNDIR/strata/bank" -name iamglass 2>/dev/null | wc -l | tr -d ' ')
[ "$sx" = 0 ] && [ "$gx" -ge 1 ] && [ "$ss" -ge 1 ] && [ "$gs" = 0 ] && pass "engines on disk: xapian node glass=$gx strata=$sx; strata node strata=$ss glass=$gs" || fail "engines on disk: xapian node glass=$gx strata=$sx; strata node strata=$ss glass=$gs"
same "xapian dump = strata dump of the restored copy" "$RUNDIR/dump_x1" "$RUNDIR/dump_s1"
same "strata dump = xapian dump of the round trip, ignoring _version" "$RUNDIR/dump_s1" "$RUNDIR/dump_x2" iv
same "xapian restore of the strata dump = xapian restore of the xapian dump (control)" "$RUNDIR/dump_x2" "$RUNDIR/dump_x3"
# strict, for the record: the xapian restore of a dump bumps the version of a few documents (also in the control), so this
# line is informational
canon "$RUNDIR/dump_s1" > "$RUNDIR/s1.c"; canon "$RUNDIR/dump_x2" > "$RUNDIR/x2.c"
echo "  note: strict strata dump vs xapian round trip: $(diff "$RUNDIR/s1.c" "$RUNDIR/x2.c" | grep -c '^<') records differ (all in _version); xapian dump restored on xapian vs original xapian dump: $(canon "$RUNDIR/dump_x1" > "$RUNDIR/x1.c"; canon "$RUNDIR/dump_x3" > "$RUNDIR/x3.c"; diff "$RUNDIR/x1.c" "$RUNDIR/x3.c" | grep -c '^<') differ"
if [ "$fails" = 0 ]; then echo "PASS"; else echo "FAIL ($fails)"; fi
[ "$fails" = 0 ]
