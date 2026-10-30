#!/usr/bin/env bash
# graceful_shutdown_check.sh -- verify graceful shutdown of the HTTP layer.
#
# The contract: while a client is still being served, a single SIGINT/SIGTERM does
# NOT exit -- it starts a *graceful* shutdown that stops taking new work and waits
# for the in-flight work to drain. A second (or, failing that, the ~30s
# try-shutdown backstop) forces the exit.
#
# The counters behind it: http_clients (in-flight HTTP requests, bumped over each
# Request's lifetime in search_views.cc); ready_to_end_http gates the wait and the count is
# exported as xapiand_http_current_connections. The reactor migration once left the HTTP
# counter unwired (every SIGINT looked idle and dropped active requests); this test guards it.
#
# Scenarios (each on a fresh node), exit non-zero if any assertion fails:
#   1. HTTP  in-flight request + 1x SIGINT -> WAITS; drains -> clean exit.
#   2. HTTP  in-flight request + 2x SIGINT -> FORCED exit.
#   3. HTTP  no clients        + 1x SIGINT -> prompt exit.
#   4. A write the debounced autocommit has not committed + SIGTERM at 0.5, 1.0 and 1.5 s after it (the commit is a task
#      the registry started on the control loop, which stops at the shutdown) -> clean exit, and the write is there after a restart.
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
	"$BIN" --port "$PORT" --name gsdnode -D "$RUNDIR/n" >"$RUNDIR/node.log" 2>&1 &
	PID=$!
	local deadline=$(( $(date +%s) + 40 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		kill -0 "$PID" 2>/dev/null || { echo "  node died during startup"; return 1; }
		curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && return 0
		sleep 0.5
	done
	echo "  node never became ready"; return 1
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

# ---- Scenario 4: SIGTERM while the debounced commit fires --------------------
echo "=== scenario 4: an uncommitted write + SIGTERM around its autocommit -> clean exit, write kept ==="
for delay in 0.5 1.0 1.5; do
	if ! start_node; then echo "  SETUP FAIL"; exit 1; fi
	curl -s -m10 -o /dev/null -XPUT "localhost:$PORT/gsd4/1" -H 'Content-Type: application/json' -d '{"n":1}'
	sleep "$delay"
	t0="$(now)"
	kill -TERM "$PID"
	if exited_within 20; then
		echo "  PASS: SIGTERM ${delay}s after the write: exited ($(elapsed "$t0")s)"
	else
		echo "  FAIL: SIGTERM ${delay}s after the write: no exit within 20s"
		fails=$((fails+1)); kill -9 "$PID" 2>/dev/null
	fi
	PID=""
	# Restart on the same data: the write was committed by the shutdown or replayed from the WAL.
	"$BIN" --port "$PORT" --name gsdnode -D "$RUNDIR/n" >"$RUNDIR/node2.log" 2>&1 &
	PID=$!
	for _ in $(seq 60); do curl -s --max-time 3 -o /dev/null "localhost:$PORT/" 2>/dev/null && break; sleep 0.5; done
	got="$(curl -s -m10 "localhost:$PORT/gsd4/1")"
	case "$got" in
		*'"n":1'*) echo "  PASS: the write is there after the restart" ;;
		*) echo "  FAIL: the write is missing after the restart: $got"; fails=$((fails+1)) ;;
	esac
	kill -TERM "$PID" 2>/dev/null; exited_within 20 || kill -9 "$PID" 2>/dev/null
	PID=""
done
echo

if [ "$fails" -eq 0 ]; then
	echo "=== ALL GRACEFUL-SHUTDOWN SCENARIOS PASSED ==="
	exit 0
fi
echo "=== $fails scenario(s) FAILED ==="
exit 1
