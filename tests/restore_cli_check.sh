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

# restore_cli_check.sh BIN: `--restore` into a new, empty data directory leaves an index a node then serves in full.
# For the default shard count, --shards 1 and --shards 3: the CLI restores a CLI dump of N documents, a node serves
# the directory (same --name and --shards), and COUNT and DUMP must answer exactly the N documents, the same DUMP as
# a RESTORE over HTTP of that dump. Also: a CLI restore into an index created by PUT. Exit 0 when all hold.
# The documents are CORPUS's (an ndjson file) when set; else N generated ones whose integer fields take positive
# values first and negative ones later, one of them beyond 2^31 (where JSON's encoding is unsigned too).
. "$(dirname "$0")/../harness/env.sh"
BIN=$1; PORT=${PORT:-18866}; N=${N:-50}; BOUND=${BOUND:-90}; rc=0
W=${HARNESS_TMP:-${TMPDIR:-.}}
D=$(mktemp -d "$W/restore_cli.XXXXXX")
P=
trap '[ -n "$P" ] && kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
fail() { echo "FAIL: $*"; rc=1; }

serve() {  # serve LOG DIR [ARGS...]: a node on DIR, up when it answers
	local log=$1 dir=$2; shift 2
	"$BIN" --port $PORT --name rc -D "$dir" "$@" >"$D/$log.log" 2>&1 & P=$!
	for _ in $(seq 120); do curl -s -m2 -o /dev/null localhost:$PORT/ && return 0; sleep .5; done
	fail "$log: the node did not start"; tail -5 "$D/$log.log"; return 1
}
stop() {
	kill -TERM $P; wait $P; local s=$?; P=
	[ $s = 0 ] || fail "the node exited $s"
}
call() {  # call METHOD URL OUT [CURL ARGS...]: the status code on stdout, the body in OUT
	local m=$1 u=$2 out=$3; shift 3
	curl -sS -m 60 -X"$m" "localhost:$PORT$u" -o "$out" -w '%{http_code}' "$@"
}
indexed_all() {  # indexed_all LABEL BODY: a RESTORE's answer indexed all N documents
	local i; i=$("$PY" -I -c 'import json, sys; print(json.load(open(sys.argv[1])).get("indexed"))' "$2" 2>/dev/null)
	[ "$i" = "$N" ] || fail "$1: RESTORE indexed '$i' of $N: $(head -c 300 "$2")"
}
dump() {  # dump OUT: the served DUMP of rt, as JSON
	local s; s=$(call DUMP /rt/ "$1" -H 'Accept: application/json')
	[ "$s" = 200 ] || fail "DUMP /rt/ answered $s: $(head -c 300 "$1")"
}
count_is() {  # count_is LABEL: COUNT of rt must be N
	local s; s=$(call COUNT /rt/ "$D/count" -H 'Accept: application/json')
	local t; t=$("$PY" -I -c 'import json, sys; print(json.load(open(sys.argv[1])).get("total"))' "$D/count" 2>/dev/null)
	[ "$s" = 200 ] && [ "$t" = "$N" ] && echo "ok   $1: COUNT is $N" \
		|| fail "$1: COUNT answered $s, total '$t' (want $N): $(head -c 300 "$D/count")"
}
same_docs() {  # same_docs LABEL GOT WANT: GOT holds exactly the source documents, and equals WANT
	"$PY" -I - "$D/nd.ndjson" "$2" "$3" <<'EOF' && echo "ok   $1: DUMP holds exactly the $N documents, as the HTTP RESTORE" || fail "$1: DUMP differs"
import json, sys
src = {d["_id"]: d for d in map(json.loads, open(sys.argv[1]))}
got = json.load(open(sys.argv[2]))
want = json.load(open(sys.argv[3]))
key = lambda d: d["_id"]
got.sort(key=key); want.sort(key=key)
bare = {d["_id"]: {k: v for k, v in d.items() if k != "_version"} for d in got}
if bare != src or len(got) != len(src):
    print(f"  documents: {len(got)} served, {len(src)} restored; first difference:",
          next((i for i in sorted(set(src) | set(bare)) if src.get(i) != bare.get(i)), None))
    sys.exit(1)
if got != want:
    print("  differs from the HTTP RESTORE:", next(((a, b) for a, b in zip(got, want) if a != b), None))
    sys.exit(1)
EOF
}

if [ -n "${CORPUS:-}" ]; then
	cp "$CORPUS" "$D/nd.ndjson" && N=$(grep -c . "$D/nd.ndjson") || { echo "FAIL: cannot read CORPUS $CORPUS"; exit 1; }
else
	"$PY" -I -c "
import json
for i in range($N):
    sign = 1 if i < $N // 2 else -1
    print(json.dumps({'_id': i + 1, 'user': 'u%d' % (i % 7), 'message': 'document %d' % i, 'n': sign * (i + 1), 'w': sign * (2**31 + i)}))" > "$D/nd.ndjson"
fi

# The dump: N documents restored over HTTP, then the CLI --dump of them.
serve source "$D/src" || exit 1
s=$(call RESTORE /rt/ "$D/body" -H 'Content-Type: application/x-ndjson' --data-binary "@$D/nd.ndjson")
if [ "$s" = 200 ]; then indexed_all source "$D/body"; else fail "source RESTORE answered $s"; fi
call POST /rt/:commit "$D/body" >/dev/null
stop
timeout -s KILL $BOUND "$BIN" --port $PORT --name rc -D "$D/src" --dump rt --out "$D/rt.mp" >"$D/dump.log" 2>&1 \
	|| { fail "--dump exited $?"; exit 1; }

for S in default 1 3; do
	SH=(); [ $S = default ] || SH=(--shards "$S")

	# Reference: RESTORE over HTTP of the same dump, served after a restart.
	serve "http-$S" "$D/http-$S" "${SH[@]}" || continue
	s=$(call RESTORE /rt/ "$D/body" -H 'Content-Type: application/x-msgpack' --data-binary "@$D/rt.mp")
	if [ "$s" = 200 ]; then indexed_all "shards $S, HTTP" "$D/body"; else fail "shards $S: HTTP RESTORE answered $s"; fi
	call POST /rt/:commit "$D/body" >/dev/null
	stop
	serve "http-$S-again" "$D/http-$S" "${SH[@]}" || continue
	dump "$D/http-$S.json"
	stop

	# The CLI --restore into a new, empty directory, then served.
	timeout -s KILL $BOUND "$BIN" --port $PORT --name rc -D "$D/cli-$S" --restore rt --in "$D/rt.mp" "${SH[@]}" >"$D/cli-$S-restore.log" 2>&1 \
		|| fail "shards $S: --restore exited $?"
	serve "cli-$S" "$D/cli-$S" "${SH[@]}" || continue
	count_is "shards $S, CLI --restore into an empty directory"
	dump "$D/cli-$S.json"
	stop
	same_docs "shards $S, CLI --restore into an empty directory" "$D/cli-$S.json" "$D/http-$S.json"
done

# An index created by PUT first, then the CLI --restore into it.
serve put "$D/put" && {
	s=$(call PUT /rt/ "$D/body" -H 'Content-Type: application/json' --data-binary '{}')
	[ "$s" = 204 ] || [ "$s" = 200 ] || fail "PUT /rt/ answered $s"
	stop
}
timeout -s KILL $BOUND "$BIN" --port $PORT --name rc -D "$D/put" --restore rt --in "$D/rt.mp" >"$D/put-restore.log" 2>&1 \
	|| fail "PUT index: --restore exited $?"
serve put-again "$D/put" && {
	count_is "CLI --restore into an index created by PUT"
	dump "$D/put.json"
	stop
	same_docs "CLI --restore into an index created by PUT" "$D/put.json" "$D/http-default.json"
}

exit $rc
