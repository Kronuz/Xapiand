#!/usr/bin/env bash
# cli_roundtrip_check.sh BIN: the CLI --dump then --restore on fresh data directories (docs/design/phase7c.md D12, D13).
# Both must exit 0 within a bound (the control loop's detached tasks are drained at exit) and print the same sha256.
# Exit 0 when they do.
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
exit $rc
