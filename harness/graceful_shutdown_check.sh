#!/usr/bin/env bash
# graceful_shutdown_check.sh -- verify graceful shutdown across all three network
# layers on a single node: HTTP, the remote (Xapian binary) protocol, and the
# replication protocol.
#
# The contract: while a client is still being served, a single SIGINT/SIGTERM does
# NOT exit -- it starts a *graceful* shutdown that stops taking new work and waits
# for the in-flight work to drain. A second (or, failing that, the ~30s
# try-shutdown backstop) forces the exit.
#
# The counters behind it: http_clients (in-flight HTTP requests, bumped over each
# Request's lifetime in search_views.cc), remote_clients and replication_clients
# (per accepted connection, in the *ProtocolViews ctors/dtors). ready_to_end_http/
# remote/replication gate the wait; all three are exported as
# xapiand_{http,remote,replication}_current_connections. The reactor migration once
# left the HTTP counter unwired (every SIGINT looked idle and dropped active
# requests); remote/replication kept theirs. This test guards all three.
#
# Scenarios (each on a fresh node), exit non-zero if any assertion fails:
#   1. HTTP  in-flight request + 1x SIGINT -> WAITS; drains -> clean exit.
#   2. HTTP  in-flight request + 2x SIGINT -> FORCED exit.
#   3. HTTP  no clients        + 1x SIGINT -> prompt exit.
#   4. remote      connection  + 1x SIGINT -> stays alive past the ~5s idle exit; rapid 2nd forces.
#   5. replication connection  + 1x SIGINT -> stays alive past the ~5s idle exit; rapid 2nd forces.
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT="${PORT:-8905}"
RUNDIR="/tmp/xgsd_$$"
mkdir -p "$RUNDIR"

PID=""
HOLDER=""
fails=0
cleanup() {
	[ -n "$HOLDER" ] && kill -9 "$HOLDER" 2>/dev/null
	[ -n "$PID" ] && kill -9 "$PID" 2>/dev/null
	rm -rf "$RUNDIR"
}
trap cleanup EXIT

now() { python3 -c 'import time;print(time.time())'; }
elapsed() { python3 -c "print(round($(now)-$1,2))"; }

start_node() {
	rm -rf "$RUNDIR/n"
	"$BIN" --port "$PORT" --solo --name gsdnode -D "$RUNDIR/n" >"$RUNDIR/node.log" 2>&1 &
	PID=$!
	local deadline=$(( $(date +%s) + 40 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		kill -0 "$PID" 2>/dev/null || { echo "  node died during startup"; return 1; }
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	echo "  node never became ready"; return 1
}

# Non-solo node: also starts the remote (Xapian binary) and replication servers,
# so their per-connection counters (remote_clients / replication_clients) and the
# same graceful-shutdown wait can be exercised. Derived ports avoid the --solo run.
XPORT=$((PORT + 1000))   # --xapian-port  (remote protocol)
LPORT=$((PORT - 1000))   # --replica-port (replication protocol)
DPORT=$((PORT + 50000))  # --discovery-port
start_cluster_node() {
	rm -rf "$RUNDIR/c"
	"$BIN" --port "$PORT" --xapian-port "$XPORT" --replica-port "$LPORT" \
		--discovery-port "$DPORT" --name gsdnode -D "$RUNDIR/c" >"$RUNDIR/node.log" 2>&1 &
	PID=$!
	local deadline=$(( $(date +%s) + 40 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		kill -0 "$PID" 2>/dev/null || { echo "  node died during startup"; return 1; }
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	echo "  node never became ready"; return 1
}

# Hold a raw TCP connection to a protocol port ($1). The server builds its
# per-connection Views object on accept (++remote_clients / ++replication_clients)
# and then blocks reading the next framed message, so an idle socket keeps the
# count > 0. Sets HOLDER.
hold_raw_connection() {
	python3 - "$1" >"$RUNDIR/raw.log" 2>&1 <<'PY' &
import socket, sys, time
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
try:
    s.recv(1024)   # drain any greeting the server sends first
except Exception:
    pass
print("HELD", flush=True)
time.sleep(120)
PY
	HOLDER=$!
	sleep 2
}

# $1 = label, $2 = protocol port.
# An idle non-solo node drains and exits by ~5s (its try-shutdown timer's first
# tick). A held protocol connection keeps that layer's client counter > 0, so
# ready_to_end_<layer>() stays false and the node must still be alive well past
# that -- until released or forced. (We assert on liveness rather than the
# "... is busy" log line, because a fresh node's brief database_pool pending state
# logs "Waiting for replicators" first and shadows it.)
raw_protocol_scenario() {
	local label="$1" rport="$2"
	echo "=== scenario: $label connection + SIGINT -> wait, then force ==="
	if ! start_cluster_node; then echo "  SETUP FAIL"; fails=$((fails+1)); return; fi
	hold_raw_connection "$rport"
	kill -INT "$PID"
	sleep 8
	if kill -0 "$PID" 2>/dev/null; then
		echo "  PASS: node still alive 8s after SIGINT (idle exits by ~5s; the $label connection held it open)"
	else
		echo "  FAIL: node exited within 8s despite a held $label connection"
		fails=$((fails+1))
	fi
	# Force with a rapid signal pair (the 2nd must land < 1s after the 1st of the
	# pair to escalate to _shutdown_now); the connection is still held.
	kill -INT "$PID"; sleep 0.4; kill -INT "$PID"
	if exited_within 8; then
		echo "  PASS: rapid 2nd signal forced the exit with the connection still held"
	else
		echo "  FAIL: rapid signals did not force exit within 8s"
		fails=$((fails+1)); kill -9 "$PID" 2>/dev/null
	fi
	kill -9 "$HOLDER" 2>/dev/null; HOLDER=""; PID=""
	echo
}

exited_within() {  # $1 seconds -- returns 0 if PID exits within the window
	local deadline=$(( $(date +%s) + $1 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		kill -0 "$PID" 2>/dev/null || return 0
		sleep 0.1
	done
	return 1
}

http_conns() {
	curl -s "localhost:$PORT/:metrics" 2>/dev/null \
		| sed -n 's/^xapiand_http_current_connections[^ ]* //p'
}

# ---- Scenario 1: graceful wait then clean drain -------------------------------
echo "=== scenario 1: in-flight request + 1x SIGINT -> wait, then drain ==="
if ! start_node; then echo "  SETUP FAIL"; exit 1; fi
python3 - "$PORT" >"$RUNDIR/holder.log" 2>&1 <<'PY' &
import socket, sys, time
port = int(sys.argv[1])
s = socket.create_connection(("127.0.0.1", port))
# Declare a body, then send only part of it: the request stays in-flight.
s.sendall(b"PUT /gsd/1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 5000\r\n\r\n")
s.sendall(b'{"held":1')
print("INFLIGHT", flush=True)
time.sleep(120)
PY
HOLDER=$!
sleep 2
conns="$(http_conns)"
echo "  http_current_connections while in-flight: ${conns:-?} (>=2 expected: held request + this probe)"
t0="$(now)"
kill -INT "$PID"
if exited_within 3; then
	echo "  FAIL: node exited within 3s despite an in-flight request"
	fails=$((fails+1))
else
	echo "  PASS: node still alive 3s after SIGINT (graceful wait)"
fi
kill -9 "$HOLDER" 2>/dev/null; HOLDER=""
if exited_within 12; then
	echo "  PASS: node drained + exited ($(elapsed "$t0")s total after release)"
else
	echo "  FAIL: node did not exit within 12s after the request was released"
	fails=$((fails+1)); kill -9 "$PID" 2>/dev/null
fi
grep -qiE "HTTP is busy" "$RUNDIR/node.log" && echo "  (log confirms: \"HTTP is busy...\")"
PID=""
echo

# ---- Scenario 2: second signal forces exit ------------------------------------
echo "=== scenario 2: in-flight request + 2x SIGINT -> force exit ==="
if ! start_node; then echo "  SETUP FAIL"; exit 1; fi
python3 - "$PORT" >"$RUNDIR/holder.log" 2>&1 <<'PY' &
import socket, sys, time
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
s.sendall(b"PUT /gsd/1 HTTP/1.1\r\nHost: x\r\nContent-Length: 5000\r\n\r\n")
s.sendall(b'{"held":1')
print("INFLIGHT", flush=True)
time.sleep(120)
PY
HOLDER=$!
sleep 2
t0="$(now)"
kill -INT "$PID"; sleep 0.4; kill -INT "$PID"
if exited_within 6; then
	echo "  PASS: node forced to exit ($(elapsed "$t0")s) by the 2nd signal, in-flight request overridden"
else
	echo "  FAIL: node did not exit within 6s after 2 signals"
	fails=$((fails+1)); kill -9 "$PID" 2>/dev/null
fi
kill -9 "$HOLDER" 2>/dev/null; HOLDER=""; PID=""
echo

# ---- Scenario 3: idle node exits promptly -------------------------------------
echo "=== scenario 3: no clients + 1x SIGINT -> prompt exit ==="
if ! start_node; then echo "  SETUP FAIL"; exit 1; fi
t0="$(now)"
kill -INT "$PID"
if exited_within 10; then
	echo "  PASS: idle node exited promptly ($(elapsed "$t0")s)"
else
	echo "  FAIL: idle node did not exit within 10s"
	fails=$((fails+1)); kill -9 "$PID" 2>/dev/null
fi
PID=""
echo

# ---- Scenarios 4 & 5: remote + replication use the SAME graceful wait ----------
# These layers were never broken by the reactor migration (their per-connection
# Views objects keep incrementing remote_clients / replication_clients); this is
# the regression guard proving the contract holds for them too. They need a
# non-solo node, so the protocol servers actually listen.
raw_protocol_scenario "remote protocol"      "$XPORT"
raw_protocol_scenario "replication protocol" "$LPORT"

if [ "$fails" -eq 0 ]; then
	echo "=== ALL GRACEFUL-SHUTDOWN SCENARIOS PASSED ==="
	exit 0
fi
echo "=== $fails scenario(s) FAILED ==="
exit 1
