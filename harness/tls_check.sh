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
#   6. startup with an expired, a not-yet-valid, or a mismatched certificate or key, a key
#      at 0640 with a group other than the node's, at 0660, or at 0644, or --tls-cert alone
#      (accept: a valid pair at mode 0600, and its key at 0640 with the node's group, SYS-13)
#   7. a certificate expiring in 3 days warns at startup (accept: 30 days, no warning)
# Each refusal of 1 to 4 is counted in /:metrics by reason (C5), and the certificate's
# seconds to expiry is a metric (C6).
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
	sign soon ec:<("$OPENSSL" ecparam -name prime256v1) 20260101000000Z \
		"$(python3 -I -c 'import datetime as d; print((d.datetime.now(d.timezone.utc) + d.timedelta(days=3)).strftime("%Y%m%d%H%M%SZ"))')"
	for m in 640 660 644; do cp ec/server.key "key$m.key" && chgrp "$(id -g)" "key$m.key" && chmod "$m" "key$m.key"; done
	other=$(id -G | tr ' ' '\n' | grep -vx "$(id -g)" | head -1)  # a second group, when the user has one
	if [ -n "$other" ]; then cp ec/server.key other.key && chgrp "$other" other.key && chmod 640 other.key; fi
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
metric() {  # metric NAME [REASON]: the value /:metrics reports, or empty
	curl -s --max-time 5 --cacert "$WORK/ec/ca.pem" "https://localhost:$PORT/:metrics" |
		awk -v name="$1" -v reason="${2:-}" '$1 ~ "^" name "({|$)" && (reason == "" || $1 ~ "reason=\"" reason "\"") { print $2; exit }'
}

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

# C5 and C6: the refusals above, counted by reason; the 30-day certificate, not warned.
for reason in certificate protocol version; do
	n=$(metric xapiand_tls_refused_connections "$reason")
	[ "${n:-0}" -ge 1 ] 2>/dev/null && pass "metrics count the $reason refusal ($n)" || fail "metrics: $reason refusals '${n:-}'"
done
n=$(metric xapiand_tls_refused_connections cipher)
[ "${n:-x}" = 0 ] && pass "metrics count no cipher refusal on this node" || fail "metrics: cipher refusals '${n:-}' (want 0)"
left=$(metric xapiand_tls_certificate_expiry_seconds)
if python3 -I -c "import sys; sys.exit(not 29 * 86400 < float(sys.argv[1]) <= 30 * 86400)" "${left:-0}"; then
	pass "metrics report the certificate's seconds to expiry (${left})"
else fail "metrics: seconds to expiry '${left:-}' (want 29 to 30 days)"; fi
grep -qF "certificate expires in" "$WORK/ec.log" && fail "a 30-day certificate warned of expiry" || pass "a 30-day certificate: no expiry warning"

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
# Key modes (SYS-13): 0640 starts with the node's own group; each refusal differs from it in one variable.
if start "$WORK/key640.log" --tls-cert "$WORK/ec/server.pem" --tls-key "$WORK/key640.key"; then
	pass "startup with the key at 0640 and the node's own group"
else fail "key at 0640 with the node's own group did not start: $(tail -3 "$WORK/key640.log")"; fi
stop
if [ -f "$WORK/other.key" ]; then
	refuse "key at 0640 with another group" "other than the process's effective group" --tls-cert "$WORK/ec/server.pem" --tls-key "$WORK/other.key"
else echo "  SKIP  key at 0640 with another group: the user has no second group (the unit test covers the rule)"; fi
refuse "group-writable key (0660)" "has mode bits outside 0640" --tls-cert "$WORK/ec/server.pem" --tls-key "$WORK/key660.key"
refuse "world-readable key (0644)" "is accessible by others" --tls-cert "$WORK/ec/server.pem" --tls-key "$WORK/key644.key"
refuse "--tls-cert without --tls-key" "go together" --tls-cert "$WORK/ec/server.pem"

# 7. Expiry within 14 days: starts, warns, and the metric shows under 3 days.
if start "$WORK/soon.log" --tls-cert "$WORK/soon.pem" --tls-key "$WORK/soon.key"; then
	left=$(metric xapiand_tls_certificate_expiry_seconds)
	if ! logged "$WORK/soon.log" "The TLS certificate expires in 2 days"; then fail "a 3-day certificate: no expiry warning"
	elif python3 -I -c "import sys; sys.exit(not 2 * 86400 < float(sys.argv[1]) <= 3 * 86400)" "${left:-0}"; then
		pass "a 3-day certificate warns at startup; seconds to expiry ${left}"
	else fail "a 3-day certificate: seconds to expiry '${left:-}'"; fi
	stop
else
	fail "node with a 3-day certificate did not start: $(tail -3 "$WORK/soon.log")"
fi

[ "$failures" = 0 ] && { echo "tls_check: OK"; exit 0; }
echo "tls_check: $failures failed"; exit 1
