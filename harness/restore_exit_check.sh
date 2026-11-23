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

# restore_exit_check.sh [BIN]: the exit status of the CLI --restore. A file that is not a dump (its objects are not
# maps) ends with EX_DATAERR (65) and says so; a valid dump restores the same documents and ends with status 0.
# Without BIN it builds build-rx/bin/xapiand from this checkout. Exits 1 when a check fails.
. "$(dirname "$0")/env.sh"
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BIN=${1:-}
if [ -z "$BIN" ]; then
	B=${BUILD:-$ROOT/build-rx}
	cmake -G Ninja -S "$ROOT" -B "$B" -DFETCHCONTENT_BASE_DIR="$ROOT/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF >"$B.cmake.log" 2>&1 \
		|| { tail -30 "$B.cmake.log"; exit 3; }
	cmake --build "$B" -j"${JOBS:-4}" --target xapiand >"$B.build.log" 2>&1 || { grep -m20 -B2 -A8 "error" "$B.build.log"; exit 3; }
	BIN=$B/bin/xapiand
fi
PORT=${PORT:-18851}; N=${N:-200}; BOUND=${BOUND:-90}; fails=0
W=${HARNESS_TMP:-${TMPDIR:-.}}
D=$(mktemp -d "$W/restore_exit.XXXXXX")
trap 'kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
check() {  # DESC ACTUAL EXPECTED
	if [ "$2" = "$3" ]; then echo "ok   $1 ($2)"; else echo "FAIL $1: got '$2', expected '$3'"; fails=$((fails + 1)); fi
}
strip() { sed 's/\x1b\[[0-9;]*m//g' "$1"; }
run() {  # LOG ARGS...: status in RC
	local log=$1; shift
	timeout -s KILL $BOUND "$BIN" --port $PORT --name rx "$@" --color never >"$log" 2>&1; RC=$?
}

# A valid dump: N documents indexed by a node, dumped by the CLI.
"$BIN" --port $PORT --name rx -D "$D/n" >"$D/node.log" 2>&1 & P=$!
for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
"$PY" -I -c "
import json
for i in range($N): print(json.dumps({'_id': i + 1, 'user': 'u%d' % (i % 7), 'message': 'document %d' % i}))" > "$D/nd.ndjson"
curl -s -m 60 -XRESTORE "localhost:$PORT/rt/" -H 'Content-Type: application/x-ndjson' --data-binary "@$D/nd.ndjson" -o /dev/null
curl -s -m 30 -XPOST "localhost:$PORT/rt/:commit" -o /dev/null
kill -TERM $P; wait $P 2>/dev/null
run "$D/dump.log" -D "$D/n" --dump rt --out "$D/rt.mp"
check "--dump ends with status 0" $RC 0
dsha=$(strip "$D/dump.log" | grep -o 'Dump sha256 = [0-9a-f]*' | awk '{print $4}')

run "$D/ok.log" -D "$D/n2" --restore rt2 --in "$D/rt.mp"
check "--restore of a dump ends with status 0" $RC 0
check "and reports the dump's sha256" "$(strip "$D/ok.log" | grep -o 'Restore sha256 = [0-9a-f]*' | awk '{print $4}')" "$dsha"
run "$D/dump2.log" -D "$D/n2" --dump rt2 --out "$D/rt2.mp"
check "the restored index dumps to the same sha256" "$(strip "$D/dump2.log" | grep -o 'Dump sha256 = [0-9a-f]*' | awk '{print $4}')" "$dsha"

# Files that are not dumps: text (every byte a small integer in MessagePack) and a dump with a stray integer after its objects.
echo "not a dump" >"$D/bad.mp"
run "$D/bad.log" -D "$D/n3" --restore bad --in "$D/bad.mp"
check "--restore of a file that is not a dump is EX_DATAERR" $RC 65
strip "$D/bad.log" | grep -q 'unsupported type\|not a map' && echo "ok   and says so" || { echo "FAIL the log does not say so"; fails=$((fails + 1)); }
{ cat "$D/rt.mp"; printf '\x01'; } >"$D/stray.mp"
run "$D/stray.log" -D "$D/n4" --restore stray --in "$D/stray.mp"
check "--restore of a dump with a stray integer after its objects is EX_DATAERR" $RC 65
exit $((fails > 0))
