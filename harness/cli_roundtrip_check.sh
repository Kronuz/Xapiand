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

# cli_roundtrip_check.sh BIN: the CLI --dump then --restore on fresh data directories (docs/design/phase7c.md D12, D13).
# Both must exit 0 within a bound (the control loop's detached tasks are drained at exit) and print the same sha256.
# A --restore of a body nested 200000 deep must fail with the depth limit, not crash. Exit 0 when all hold.
. "$(dirname "$0")/env.sh"
BIN=$1; PORT=${PORT:-18836}; N=${N:-3000}; BOUND=${BOUND:-90}; rc=0
W=${HARNESS_TMP:-${TMPDIR:-.}}
D=$(mktemp -d "$W/cli_roundtrip.XXXXXX")
trap 'kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
fail() { echo "FAIL: $*"; rc=1; }
"$BIN" --port $PORT --name cl -D "$D/n" >"$D/log" 2>&1 & P=$!
for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
"$PY" -I -c "
import json
for i in range($N): print(json.dumps({'_id': i + 1, 'user': 'u%d' % (i % 7), 'message': 'document %d' % i}))" > "$D/nd.ndjson"
curl -s -m 60 -XRESTORE "localhost:$PORT/rt/" -H 'Content-Type: application/x-ndjson' --data-binary "@$D/nd.ndjson" -o /dev/null
curl -s -m 30 -XPOST "localhost:$PORT/rt/:commit" -o /dev/null
kill -TERM $P; wait $P 2>/dev/null
timeout -s KILL $BOUND "$BIN" --port $PORT --name cl -D "$D/n" --dump rt --out "$D/rt.mp" >"$D/dump.log" 2>&1; drc=$?
timeout -s KILL $BOUND "$BIN" --port $PORT --name cl -D "$D/n2" --restore rt2 --in "$D/rt.mp" --shards 2 >"$D/restore.log" 2>&1; rrc=$?
dsha=$(sed 's/\x1b\[[0-9;]*m//g' "$D/dump.log" | grep -o 'Dump sha256 = [0-9a-f]*' | awk '{print $4}')
rsha=$(sed 's/\x1b\[[0-9;]*m//g' "$D/restore.log" | grep -o 'Restore sha256 = [0-9a-f]*' | awk '{print $4}')
[ $drc = 0 ] && echo "ok   --dump exited 0" || fail "--dump exited $drc (124: still running after ${BOUND}s)"
[ $rrc = 0 ] && echo "ok   --restore exited 0" || fail "--restore exited $rrc (124: still running after ${BOUND}s)"
[ -n "$dsha" ] && [ "$dsha" = "$rsha" ] && echo "ok   the sha256 lines match" || fail "sha256: dump '$dsha', restore '$rsha'"

# A body nested far past MAX_JSON_DEPTH is refused by the unpacker, not copied (a recursion that ends the stack).
"$PY" -I -c "import sys; sys.stdout.buffer.write(b'\x91' * 200000 + b'\x01')" > "$D/deep.mp"
timeout -s KILL $BOUND "$BIN" --port $PORT --name cl -D "$D/n3" --restore deep --in "$D/deep.mp" --shards 2 >"$D/deep.log" 2>&1; krc=$?
[ $krc -ne 0 ] && [ $krc -lt 128 ] && echo "ok   --restore of a body nested 200000 deep failed cleanly (exit $krc)" || fail "--restore of a body nested 200000 deep exited $krc (0: accepted; 128 and over: killed by a signal)"
grep -q 'nested too deeply\|depth size overflow' "$D/deep.log" && echo "ok   the log names the depth limit" || fail "the log does not name the depth limit"
exit $rc
