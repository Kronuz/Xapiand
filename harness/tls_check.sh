#!/usr/bin/env bash
#
# tls_check.sh BINARY [PORT]
#
# HTTPS refusals (docs/design/transport-security.md, T2), each beside an accept case that
# differs in one variable, each refusal asserting the reason the server logs:
#   1. a client not trusting the CA (accept: one that does)
#   2. plain HTTP to the TLS port (accept: HTTPS)
#   3. a raw TLS 1.1 ClientHello (accept: the same hello at TLS 1.2)
#   4. a static-RSA TLS 1.2 suite on an RSA certificate (accept: ECDHE-RSA-AES128-GCM)
#   5. a forced TLS 1.2 renegotiation (accept: the same connection without it)
#   6. startup with an expired, a not-yet-valid, a mismatched, or a group-readable
#      certificate or key, or --tls-cert alone (accept: a valid pair at mode 0600)
# Exits 0 when every case holds.
set -uo pipefail

BIN="${1:?usage: tls_check.sh BINARY [PORT]}"
PORT="${2:-18890}"
HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
WORK="${WORK:-$(dirname "$HARNESS_DIR")/.scratch/tls_check}"
OPENSSL="${OPENSSL:-openssl}"
rm -rf "$WORK"; mkdir -p "$WORK"
failures=0
pass() { echo "  PASS  $*"; }
fail() { echo "  FAIL  $*"; failures=$((failures + 1)); }

# Certificates: an EC pair and an RSA pair from one CA, a second CA nobody trusts, and the
# bad ones for startup, dated with `openssl ca` (which every OpenSSL 3 and 4 has).
"$HARNESS_DIR/tls_certs.sh" "$WORK/ec" > /dev/null
"$HARNESS_DIR/tls_certs.sh" "$WORK/other" > /dev/null
(
	cd "$WORK" && umask 077
	mkdir -p ca/new && : > ca/index.txt && echo 01 > ca/serial
	cat > ca/ca.cnf <<-EOF
	[ca]
	default_ca = test
	[test]
	dir = $WORK/ca
	database = \$dir/index.txt
	serial = \$dir/serial
	new_certs_dir = \$dir/new
	certificate = $WORK/ec/ca.pem
	private_key = $WORK/ec/ca.key
	default_md = sha256
	policy = any
	unique_subject = no
	copy_extensions = copy
	[any]
	commonName = supplied
	EOF
	sign() {  # sign NAME KEYALG START END
		"$OPENSSL" req -newkey "$2" -nodes -subj /CN=localhost -keyout "$1.key" -out "$1.csr" \
			-addext "subjectAltName=DNS:localhost,IP:127.0.0.1" 2>/dev/null
		"$OPENSSL" ca -batch -config ca/ca.cnf -startdate "$3" -enddate "$4" -in "$1.csr" -out "$1.pem" 2>/dev/null
		chmod 644 "$1.pem"
	}
	sign rsa rsa:2048 20260101000000Z 20991231000000Z
	sign expired ec:<("$OPENSSL" ecparam -name prime256v1) 20200101000000Z 20200201000000Z
	sign future ec:<("$OPENSSL" ecparam -name prime256v1) 20980101000000Z 20990101000000Z
	cp ec/server.key readable.key && chmod 640 readable.key
)

node_pid=""
ok() { [ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 5 "$@")" = 200 ]; }
start() {  # start LOG FLAGS...: a solo node, ready when it answers HTTPS with a 200
	local log="$1"; shift
	rm -rf "$WORK/data"; mkdir -p "$WORK/data"
	"$BIN" --port "$PORT" --name tlscheck --solo -D "$WORK/data" "$@" > "$log" 2>&1 &
	node_pid=$!
	for _ in $(seq 1 120); do
		kill -0 "$node_pid" 2>/dev/null || return 1
		ok --cacert "$WORK/ec/ca.pem" "https://localhost:$PORT/" && return 0
		sleep 0.5
	done
	return 1
}
stop() { [ -n "$node_pid" ] && { kill "$node_pid" 2>/dev/null; wait "$node_pid" 2>/dev/null; node_pid=""; }; }
trap stop EXIT
logged() { sleep 1; grep -qF "$2" "$1"; }  # logged LOG TEXT

# hello VERSION: sends a raw ClientHello (record 1.0, the given client_version, ECDHE-RSA
# and ECDHE-ECDSA AES-GCM suites, no supported_versions) and prints the first reply record:
# "alert <description>" or "handshake".
hello() {
	python3 -I - "$PORT" "$1" <<-'EOF'
	import os, socket, struct, sys
	port, version = int(sys.argv[1]), {"1.1": 0x0302, "1.2": 0x0303}[sys.argv[2]]
	def ext(kind, body): return struct.pack("!HH", kind, len(body)) + body
	exts = ext(10, struct.pack("!HH", 2, 23)) + ext(11, b"\x01\x00") \
		+ ext(13, struct.pack("!H", 6) + struct.pack("!HHH", 0x0403, 0x0804, 0x0401))
	suites = struct.pack("!HHHH", 0xc02b, 0xc02f, 0xc013, 0x002f)
	body = struct.pack("!H", version) + os.urandom(32) + b"\x00" + struct.pack("!H", len(suites)) + suites \
		+ b"\x01\x00" + struct.pack("!H", len(exts)) + exts
	hs = b"\x01" + struct.pack("!I", len(body))[1:] + body
	s = socket.create_connection(("127.0.0.1", port), timeout=5)
	s.sendall(b"\x16\x03\x01" + struct.pack("!H", len(hs)) + hs)
	head = s.recv(7)
	if len(head) >= 7 and head[0] == 0x15: print("alert", head[6])
	elif head[:1] == b"\x16": print("handshake")
	else: print("none", head.hex())
	EOF
}

echo "== [T2] TLS refusals on :$PORT"
ec=(--tls-cert "$WORK/ec/server.pem" --tls-key "$WORK/ec/server.key")
if ! start "$WORK/ec.log" "${ec[@]}"; then
	fail "node with a valid EC pair at 0600 did not start: $(tail -3 "$WORK/ec.log")"
	exit 1
fi
pass "startup with a valid pair at mode 0600"

# 1. Trust.
ok --cacert "$WORK/ec/ca.pem" "https://localhost:$PORT/" && pass "client trusting the CA accepted" || fail "client trusting the CA refused"
if ok --cacert "$WORK/other/ca.pem" "https://localhost:$PORT/"; then fail "client not trusting the CA got through"
elif logged "$WORK/ec.log" "the client refused the certificate"; then pass "client not trusting the CA refused, logged as certificate"
else fail "client not trusting the CA: no certificate reason in the log"; fi

# 2. Protocol.
if ok "http://localhost:$PORT/"; then fail "plain HTTP answered on the TLS port"
elif logged "$WORK/ec.log" "something other than TLS"; then pass "plain HTTP refused, logged as protocol"
else fail "plain HTTP: no protocol reason in the log"; fi

# 3. Version.
r=$(hello 1.2); [ "$r" = handshake ] && pass "raw TLS 1.2 hello answered with a handshake" || fail "raw TLS 1.2 hello: $r"
r=$(hello 1.1)
if [ "$r" != "alert 70" ]; then fail "raw TLS 1.1 hello: $r (want alert 70, protocol_version)"
elif logged "$WORK/ec.log" "refused: protocol version"; then pass "raw TLS 1.1 hello refused (alert 70), logged as version"
else fail "raw TLS 1.1 hello: no version reason in the log"; fi

# 5. Renegotiation: s_client renegotiates on a line holding just R.
reneg() {  # reneg R|'': one TLS 1.2 connection, optionally renegotiating, then a request
	{ sleep 1; [ -n "$1" ] && { echo R; sleep 1; }; printf 'GET / HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n'; sleep 2; } |
		"$OPENSSL" s_client -connect "127.0.0.1:$PORT" -tls1_2 -CAfile "$WORK/ec/ca.pem" -servername localhost 2>&1
}
out=$(reneg ""); grep -q "HTTP/1.1 200" <<< "$out" && pass "TLS 1.2 connection without renegotiation served" || fail "TLS 1.2 connection without renegotiation not served"
out=$(reneg R)
if grep -q "HTTP/1.1 200" <<< "$out" && ! grep -qi "no renegotiation\|alert" <<< "$out"; then fail "renegotiation went through"
else pass "renegotiation refused ($(grep -oi 'no renegotiation\|alert [a-z ]*' <<< "$out" | head -1))"; fi
stop

# 4. Ciphers, on an RSA certificate.
if start "$WORK/rsa.log" --tls-cert "$WORK/rsa.pem" --tls-key "$WORK/rsa.key"; then
	suite() {  # suite NAME: offers only that TLS 1.2 suite; prints the one negotiated
		"$OPENSSL" s_client -connect "127.0.0.1:$PORT" -tls1_2 -cipher "$1" -CAfile "$WORK/ec/ca.pem" \
			-servername localhost < /dev/null 2>&1 | sed -n 's/.*Cipher is \([^ ]*\).*/\1/p' | head -1
	}
	r=$(suite ECDHE-RSA-AES128-GCM-SHA256)
	[ "$r" = ECDHE-RSA-AES128-GCM-SHA256 ] && pass "ECDHE-RSA-AES128-GCM accepted" || fail "ECDHE-RSA-AES128-GCM: negotiated '$r'"
	r=$(suite AES128-GCM-SHA256)
	if [ -n "$r" ] && [ "$r" != "(NONE)" ]; then fail "static RSA accepted ($r)"
	elif logged "$WORK/rsa.log" "no shared cipher"; then pass "static-RSA AES128-GCM refused, logged as cipher"
	else fail "static RSA: no cipher reason in the log"; fi
	stop
else
	fail "node with the RSA pair did not start: $(tail -3 "$WORK/rsa.log")"
fi

# 6. Startup refusals: the node exits, naming the reason.
refuse() {  # refuse WHAT TEXT FLAGS...
	local what="$1" text="$2"; shift 2
	if start "$WORK/refuse.log" "$@"; then fail "$what: started"; stop
	elif grep -qF "$text" "$WORK/refuse.log"; then pass "$what: refused, naming it"
	else fail "$what: refused without '$text': $(grep -v '^$' "$WORK/refuse.log" | tail -2)"; fi
	stop
}
refuse "expired certificate" "has expired" --tls-cert "$WORK/expired.pem" --tls-key "$WORK/expired.key"
refuse "not-yet-valid certificate" "is not valid yet" --tls-cert "$WORK/future.pem" --tls-key "$WORK/future.key"
refuse "mismatched key" "TLS certificate or key" --tls-cert "$WORK/ec/server.pem" --tls-key "$WORK/other/server.key"
refuse "group-readable key" "make it mode 0600" --tls-cert "$WORK/ec/server.pem" --tls-key "$WORK/readable.key"
refuse "--tls-cert without --tls-key" "go together" --tls-cert "$WORK/ec/server.pem"

[ "$failures" = 0 ] && { echo "tls_check: OK"; exit 0; }
echo "tls_check: $failures failed"; exit 1
