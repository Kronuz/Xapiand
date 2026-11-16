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

# restore_corpora.sh write BIN OUTDIR | check BIN [CORPORA]: the restore corpora of T13.
#
# A corpus is a directory with rt.mp (a --dump of the index `rt`, written by the build named in its BUILD file) and
# is checked against tests/corpora/source.ndjson, the documents that build indexed (harness/restore_corpora_gen.py).
#
# write: on a fresh node of BIN, RESTORE the source documents, then `--dump rt --out OUTDIR/rt.mp`; BUILD names BIN's
#   version and $BUILD_ID (the commit or tag it was built from). Run it with the build that must write the corpus.
# check: for each corpus under CORPORA (default tests/corpora), RESTORE rt.mp into a fresh node over HTTP and, separately,
#   with `--restore rt --in` into an empty data directory; each is then served again (from disk) and its DUMP must hold
#   exactly the source documents, each equal field by field once `_version` is dropped, and COUNT must be their number.
#   Exit 0 when every corpus passes both ways.
. "$(dirname "$0")/env.sh"
MODE=${1:-}; BIN=${2:-}; HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/.." && pwd)
PORT=${PORT:-$(free_port)}; BOUND=${BOUND:-180}; rc=0
W=${HARNESS_TMP:-${TMPDIR:-.}}
D=$(mktemp -d "$W/restore_corpora.XXXXXX")
P=
trap '[ -n "$P" ] && kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
fail() { echo "FAIL: $*"; rc=1; }
SOURCE=$ROOT/tests/corpora/source.ndjson

serve() {  # serve LABEL DIR [ARGS...]: a node on DIR, up when it answers
	local log=$1 dir=$2; shift 2
	"$BIN" --port "$PORT" --name rc -D "$dir" "$@" >"$D/$log.log" 2>&1 & P=$!
	for _ in $(seq 120); do curl -s -m2 -o /dev/null "localhost:$PORT/" && return 0; sleep .5; done
	fail "$log: the node did not start"; tail -5 "$D/$log.log"; return 1
}
stop() { kill -TERM "$P"; wait "$P"; local s=$?; P=; [ $s = 0 ] || fail "the node exited $s"; }
call() {  # call METHOD URL OUT [CURL ARGS...]: the status on stdout, the body in OUT
	local m=$1 u=$2 out=$3; shift 3
	curl -sS -m 120 -X"$m" "localhost:$PORT$u" -o "$out" -w '%{http_code}' "$@"
}

write() {
	local out=$2 s
	mkdir -p "$out"
	serve write "$D/data" || return 1
	s=$(call RESTORE /rt/ "$D/body" -H 'Content-Type: application/x-ndjson' --data-binary "@$SOURCE")
	[ "$s" = 200 ] || { fail "RESTORE of the source answered $s: $(head -c 300 "$D/body")"; return 1; }
	call POST /rt/:commit "$D/body" >/dev/null
	stop
	timeout -s KILL "$BOUND" "$BIN" --port "$PORT" --name rc -D "$D/data" --dump rt --out "$out/rt.mp" >"$D/dump.log" 2>&1 \
		|| { fail "--dump exited $?"; tail -5 "$D/dump.log"; return 1; }
	echo "$("$BIN" --version 2>&1 | head -1), built from ${BUILD_ID:-unknown}" >"$out/BUILD"
	echo "wrote $out/rt.mp ($(wc -c <"$out/rt.mp") bytes): $(cat "$out/BUILD")"
}

same_docs() {  # same_docs LABEL DUMPJSON: the dump holds exactly the source documents
	"$PY" -I - "$SOURCE" "$2" <<'PYEOF' && echo "ok   $1: DUMP holds exactly the source documents" || fail "$1: DUMP differs"
import json, sys
src = {d["_id"]: d for d in map(json.loads, open(sys.argv[1], encoding="utf-8"))}
got = {}
for d in json.load(open(sys.argv[2], encoding="utf-8")):
    d.pop("_version", None)
    if d["_id"] in got:
        print(f"  document {d['_id']} appears twice"); sys.exit(1)
    got[d["_id"]] = d
bad = [i for i in sorted(set(src) | set(got)) if src.get(i) != got.get(i)]
if bad:
    i = bad[0]
    print(f"  {len(got)} documents served, {len(src)} in the source, {len(bad)} differ; first: _id {i}")
    s, g = src.get(i, {}), got.get(i, {})
    for k in sorted(set(s) | set(g)):
        if s.get(k) != g.get(k):
            print(f"    {k}: source {s.get(k)!r:.120} served {g.get(k)!r:.120}")
    sys.exit(1)
PYEOF
}

check_served() {  # check_served LABEL DIR [ARGS...]: serve DIR, COUNT and DUMP it
	local label=$1 dir=$2 want s t; shift 2
	want=$(wc -l <"$SOURCE" | tr -d ' ')
	serve "$label" "$dir" "$@" || return 1
	s=$(call COUNT /rt/ "$D/count" -H 'Accept: application/json')
	t=$("$PY" -I -c 'import json, sys; print(json.load(open(sys.argv[1])).get("total"))' "$D/count" 2>/dev/null)
	[ "$s" = 200 ] && [ "$t" = "$want" ] && echo "ok   $label: COUNT is $want" \
		|| fail "$label: COUNT answered $s, total '$t' (want $want): $(head -c 300 "$D/count")"
	s=$(call DUMP /rt/ "$D/$label.json" -H 'Accept: application/json')
	[ "$s" = 200 ] || fail "$label: DUMP answered $s: $(head -c 300 "$D/$label.json")"
	stop
	[ "$s" = 200 ] && same_docs "$label" "$D/$label.json"
}

check() {
	local corpus name n=0 s
	for corpus in "${1:-$ROOT/tests/corpora}"/*/; do
		[ -f "$corpus/rt.mp" ] || continue
		name=$(basename "$corpus"); n=$((n + 1))
		echo "corpus $name: $(cat "$corpus/BUILD" 2>/dev/null)"
		serve "http-$name" "$D/http-$name" || continue
		s=$(call RESTORE /rt/ "$D/body" -H 'Content-Type: application/x-msgpack' --data-binary "@$corpus/rt.mp")
		[ "$s" = 200 ] || fail "$name: HTTP RESTORE answered $s: $(head -c 300 "$D/body")"
		call POST /rt/:commit "$D/body" >/dev/null
		stop
		check_served "http-$name-again" "$D/http-$name"
		timeout -s KILL "$BOUND" "$BIN" --port "$PORT" --name rc -D "$D/cli-$name" --restore rt --in "$corpus/rt.mp" \
			>"$D/cli-$name.log" 2>&1 || { fail "$name: --restore exited $?"; tail -5 "$D/cli-$name.log"; continue; }
		check_served "cli-$name" "$D/cli-$name"
	done
	[ $n -gt 0 ] || fail "no corpus under ${1:-$ROOT/tests/corpora}"
}

case $MODE in
	write) [ -n "${3:-}" ] && [ -x "$BIN" ] || { echo "usage: $0 write BIN OUTDIR" >&2; exit 2; }; write "$BIN" "$3" ;;
	check) [ -x "$BIN" ] || { echo "usage: $0 check BIN [CORPORA]" >&2; exit 2; }; check "${3:-}" ;;
	*) echo "usage: $0 write BIN OUTDIR | check BIN [CORPORA]" >&2; exit 2 ;;
esac
exit $rc
