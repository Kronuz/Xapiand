#!/usr/bin/env bash
# stored_stream.sh BIN: T2's upload rows (docs/design/phase7c.md D9, D12). On a fresh node, PUT `stored` blobs of
# 2x the body cap, one byte over it, and one just under it (buffered), and GET each back byte-equal; then four
# uploads at once; then the refusals (a size past the record limit is a 413, a POST with no id is a 400, a client that
# leaves mid-upload leaves no volume behind). Exit 0 when every row passes.
BIN=$1; PORT=${PORT:-18830}; CAP=$((16 * 1024 * 1024)); rc=0
W=${HARNESS_TMP:-${TMPDIR:-.}}
D=$(mktemp -d "$W/stored_stream.XXXXXX")
trap 'kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
"$BIN" --port $PORT --name ss -D "$D/n" >"$D/log" 2>&1 & P=$!
for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
fail() { echo "FAIL: $*"; rc=1; }
make() { head -c "$2" /dev/urandom > "$D/$1.bin"; }
curl -s -XPUT localhost:$PORT/up -o /dev/null

row() {  # row NAME BYTES: upload and read back
  make "$1" "$2"
  code=$(curl -s -o "$D/$1.put" -w '%{http_code}' -m 120 -XPUT "localhost:$PORT/up/$1?commit=true" -H 'Content-Type: application/octet-stream' --data-binary "@$D/$1.bin")
  case $code in 2??) ;; *) fail "$1 ($2 bytes): PUT answered $code"; return;; esac
  code=$(curl -s -o "$D/$1.got" -w '%{http_code}' -m 120 "localhost:$PORT/up/$1" -H 'Accept: application/octet-stream')
  if [ "$code" = 200 ] && cmp -s "$D/$1.bin" "$D/$1.got"; then echo "ok   $1: $2 bytes read back byte-equal"; else fail "$1 ($2 bytes): GET $code, or the bytes differ"; fi
}
row under $((CAP - 4096))
row over $((CAP + 1))
row double $((2 * CAP))

# Four uploads at once, to different ids (so, different shards).
for i in 1 2 3 4; do make par$i $((20 * 1024 * 1024 + i)); done
for i in 1 2 3 4; do
  curl -s -o /dev/null -w '%{http_code}\n' -m 120 -XPUT "localhost:$PORT/up/par$i?commit=true" -H 'Content-Type: application/octet-stream' --data-binary "@$D/par$i.bin" > "$D/par$i.code" &
done
wait %2 %3 %4 %5 2>/dev/null
for i in 1 2 3 4; do
  c=$(cat "$D/par$i.code" 2>/dev/null)
  curl -s -o "$D/par$i.got" -m 120 "localhost:$PORT/up/par$i" -H 'Accept: application/octet-stream'
  case $c in 2??) cmp -s "$D/par$i.bin" "$D/par$i.got" && echo "ok   par$i: concurrent upload read back byte-equal" || fail "par$i: the bytes differ";; *) fail "par$i: PUT answered $c";; esac
done

# The refusals, over a raw socket (curl would send a body).
/usr/local/bin/python3 -I - "$PORT" <<'PY' || rc=1
import socket, sys
port = int(sys.argv[1])
def talk(request, body=b'', leave_after=None, read=True):
    s = socket.create_connection(('127.0.0.1', port), timeout=30)
    s.sendall(request + b'\r\n\r\n' + body)
    if leave_after is not None:
        s.close(); return None
    data = s.recv(65536); s.close()
    return int(data.split(b' ', 2)[1]) if data else None
bad = 0
def check(name, got, want):
    global bad
    ok = got == want
    print(('ok   ' if ok else 'FAIL ') + f'{name}: {got}' + ('' if ok else f' (want {want})')); bad += 0 if ok else 1
check('a blob past the record limit is a 413', talk(b'PUT /up/huge HTTP/1.1\r\nHost: x\r\nContent-Type: application/octet-stream\r\nContent-Length: 5368709120'), 413)
check('a POST of a big blob with no id is a 400', talk(b'POST /up/ HTTP/1.1\r\nHost: x\r\nContent-Type: application/octet-stream\r\nContent-Length: 20971520'), 400)
check('a big stored body for a view that cannot stream is a 413', talk(b'POST /up/:search HTTP/1.1\r\nHost: x\r\nContent-Type: application/octet-stream\r\nContent-Length: 20971520'), 413)
check('a big JSON body is a 413', talk(b'PUT /up/j HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 20971520'), 413)
sys.exit(1 if bad else 0)
PY
# A client that leaves mid-upload: the node goes on, and its volume is removed.
before=$(find "$D/n" -name 'docdata.*' | wc -l)
/usr/local/bin/python3 -I - "$PORT" <<'PY'
import socket, sys
s = socket.create_connection(('127.0.0.1', int(sys.argv[1])), timeout=30)
s.sendall(b'PUT /up/leaver HTTP/1.1\r\nHost: x\r\nContent-Type: application/octet-stream\r\nContent-Length: 41943040\r\n\r\n' + b'x' * (20 * 1024 * 1024))
s.close()
PY
sleep 3
after=$(find "$D/n" -name 'docdata.*' | wc -l)
code=$(curl -s -o /dev/null -w '%{http_code}' -m 20 "localhost:$PORT/up/under" -H 'Accept: application/octet-stream')
if [ "$after" = "$before" ] && [ "$code" = 200 ]; then echo "ok   a client that left mid-upload: $after volumes, the node still serves"; else fail "a client that left mid-upload: volumes $before -> $after, GET $code"; fi
if grep -q "blocks on database" "$D/log"; then fail "the node logged a reactor block"; fi
exit $rc
