#!/usr/bin/env bash
# restore_stream_check.sh BIN: RESTORE as D12 runs it (a streamed body parsed in chores, indexed as tasks). On a fresh
# node: an NDJSON stream of N documents (default 20000) and a MsgPack stream, a body that arrives chunked (buffered),
# a stream with a bad line, and a client that leaves mid-stream (what was indexed stays). Every count is read back.
BIN=$1; PORT=${PORT:-18832}; N=${N:-20000}; rc=0
W=${HARNESS_TMP:-${TMPDIR:-.}}
D=$(mktemp -d "$W/restore_stream.XXXXXX")
trap 'kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
start() {
  "$BIN" --port $PORT --name rs -D "$D/n" >>"$D/log" 2>&1 & P=$!
  for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
}
start
fail() { echo "FAIL: $*"; rc=1; }

/usr/local/bin/python3 -I - "$D" "$N" <<'PY'
import json, struct, sys
d, n = sys.argv[1], int(sys.argv[2])
with open(f'{d}/nd.ndjson', 'w') as f:
    for i in range(n):
        f.write(json.dumps({'_id': i + 1, 'user': f'u{i % 97}', 'message': f'message number {i} of the restore stream'}) + '\n')
with open(f'{d}/bad.ndjson', 'w') as f:
    for i in range(1000):
        f.write(json.dumps({'_id': i + 1, 'user': 'u', 'message': 'm'}) + '\n')
        if i == 500:
            f.write('{"_id": 9999, "broken\n')
def pack(o):
    if isinstance(o, dict):
        b = b'\xde' + struct.pack('>H', len(o))
        for k, v in o.items():
            b += pack(k) + pack(v)
        return b
    if isinstance(o, str):
        e = o.encode()
        return b'\xdb' + struct.pack('>I', len(e)) + e
    return b'\xd3' + struct.pack('>q', o)
with open(f'{d}/mp.msgpack', 'wb') as f:
    for i in range(n // 4):
        f.write(pack({'_id': i + 1, 'user': f'u{i % 97}', 'message': f'message number {i}'}))
PY

count() { curl -s -m 30 "localhost:$PORT/$1/:count" | /usr/local/bin/python3 -I -c 'import json,sys; print(json.load(sys.stdin).get("total", -1))'; }
indexed() { /usr/local/bin/python3 -I -c 'import json,sys; print(json.load(sys.stdin).get("indexed", -1))' < "$1"; }
row() {  # row NAME INDEX FILE CT WANT [extra curl args]
  name=$1; idx=$2; file=$3; ct=$4; want=$5; shift 5
  code=$(curl -s -o "$D/$name.out" -w '%{http_code}' -m 300 -XRESTORE "localhost:$PORT/$idx/" -H "Content-Type: $ct" --data-binary "@$file" "$@")
  got=$(indexed "$D/$name.out"); have=$(count "$idx")
  if [ "$code" = 200 ] && [ "$got" = "$want" ] && [ "$have" = "$want" ]; then echo "ok   $name: $want documents indexed and counted"; else fail "$name: HTTP $code, indexed $got, counted $have, wanted $want"; fi
}
row ndjson nd "$D/nd.ndjson" application/x-ndjson "$N"
row msgpack mp "$D/mp.msgpack" application/x-msgpack $((N / 4))
row chunked ch "$D/bad.ndjson" application/x-ndjson 1000 -H 'Transfer-Encoding: chunked'
row badline bl "$D/bad.ndjson" application/x-ndjson 1000

# A client that leaves mid-stream: the node goes on and what it had indexed stays.
/usr/local/bin/python3 -I - "$PORT" "$D/nd.ndjson" <<'PY'
import socket, sys
data = open(sys.argv[2], 'rb').read()
s = socket.create_connection(('127.0.0.1', int(sys.argv[1])), timeout=30)
s.sendall(b'RESTORE /left/ HTTP/1.1\r\nHost: x\r\nContent-Type: application/x-ndjson\r\nContent-Length: %d\r\n\r\n' % (len(data) * 4) + data)
s.close()
PY
# The node finishes the documents it had received, then the stream fails and what was indexed is committed.
have=-1
for _ in $(seq 60); do sleep 1; now=$(count left); [ "$now" = "$have" ] && break; have=$now; done
sleep 5   # the closing commit of the documents indexed so far
have=$(count left)
code=$(curl -s -o /dev/null -w '%{http_code}' -m 20 "localhost:$PORT/nd/1")
# The restore stops soon after the client leaves (it does not index the rest of what it had read): fewer than N stay.
if [ "$code" = 200 ] && [ "${have:--1}" -gt 0 ] && [ "$have" -lt "$N" ] 2>/dev/null; then echo "ok   a client that left mid-stream: $have of $N documents stay, the node serves"; else fail "a client that left mid-stream: counted $have of $N, GET $code"; fi
# What the cut-off stream had indexed was committed: it survives a kill -9.
kill -9 $P 2>/dev/null; wait $P 2>/dev/null
start
again=$(count left)
if [ "${again:--1}" = "$have" ]; then echo "ok   after kill -9 and a restart: $again documents"; else fail "after kill -9 and a restart: $again documents, had $have"; fi
if grep -q "blocks on database" "$D/log"; then fail "the node logged a reactor block"; fi
exit $rc
