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

# coverage_e2e_main.sh BINARY [WORKDIR] -- the process-level paths of src/main.cc and src/manager.cc, on the real binary.
#
# Runs the node (and the --dump and --restore commands) through its flags, start-up refusals, signals and shutdowns, and
# checks each result: exit statuses, the messages logged, the files written, what the node serves. Run on the
# instrumented build, the profiles it leaves merge with the other workloads' (harness/coverage.sh, workload `main`).
# Exits 1 when a check fails. Each scenario names the code it reaches.
set -u
BIN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
HERE="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$HERE/.." && pwd)"
. "$HERE/env.sh"
W="${2:-${HARNESS_TMP:-$ROOT/.scratch}/e2e_main}"
rm -rf "$W"; mkdir -p "$W"; W="$(cd "$W" && pwd)"

fails=0; checks=0
pass() { checks=$((checks + 1)); echo "  PASS  $*"; }
fail() { checks=$((checks + 1)); fails=$((fails + 1)); echo "  FAIL  $*"; }
expect_eq() {  # DESC ACTUAL EXPECTED
	if [ "$2" = "$3" ]; then pass "$1 ($2)"; else fail "$1: got '$2', expected '$3'"; fi
}
expect_log() {  # DESC FILE REGEX
	if grep -Eq -- "$3" "$2" 2>/dev/null; then pass "$1"; else fail "$1: no '$3' in $2"; fi
}
expect_no_log() {
	if grep -Eq -- "$3" "$2" 2>/dev/null; then fail "$1: found '$3' in $2"; else pass "$1"; fi
}

PIDS=""
cleanup() { for p in $PIDS; do kill -9 "$p" 2>/dev/null; done; }
trap cleanup EXIT

PORT=""
NODE_PID=""
RC=0

# start_node LOG DATADIR [flags...]: a node in the background, ready when GET / answers 200 (NODE_PID, PORT).
start_node() {
	local log=$1 data=$2; shift 2
	PORT=$(free_port)
	"$BIN" --port "$PORT" --bind-address 127.0.0.1 -D "$data" "$@" >"$log" 2>&1 &
	NODE_PID=$!; PIDS="$PIDS $NODE_PID"
	local i
	for ((i = 0; i < 400; i++)); do
		kill -0 "$NODE_PID" 2>/dev/null || { echo "  node exited during start-up (see $log)"; return 1; }
		[ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 "127.0.0.1:$PORT/" 2>/dev/null)" = 200 ] && return 0
		sleep 0.1
	done
	echo "  node not ready (see $log)"; return 1
}

# wait_exit SECONDS: waits for NODE_PID to end; RC is its status (124 when it did not).
wait_exit() {
	local i
	for ((i = 0; i < $1 * 10; i++)); do
		kill -0 "$NODE_PID" 2>/dev/null || { wait "$NODE_PID"; RC=$?; return 0; }
		sleep 0.1
	done
	kill -9 "$NODE_PID" 2>/dev/null; wait "$NODE_PID" 2>/dev/null; RC=124; return 1
}

# stop_node SIGNAL SECONDS: the signal, then wait_exit.
stop_node() { kill -"$1" "$NODE_PID" 2>/dev/null; wait_exit "$2"; }

# run_to LOG SECONDS cmd...: a command that ends by itself (RC is its status, 124 when it did not end). STDIN_FILE is its stdin.
run_to() {
	local log=$1 secs=$2; shift 2
	"$@" >"$log" 2>&1 <"${STDIN_FILE:-/dev/null}" &
	NODE_PID=$!; PIDS="$PIDS $NODE_PID"
	wait_exit "$secs"
}

put() {  # INDEX ID JSON: the status of the PUT
	curl -s -o /dev/null -w '%{http_code}' --max-time 10 -X PUT -H 'Content-Type: application/json' -d "$3" "127.0.0.1:$PORT/$1/$2"
}

ESC=$(printf '\033')
D="$W/data"

echo "== [1] a node with a pid file, a log file, verbosity and strict mode (main: writepid, setup, banner, server; manager: run)"
start_node "$W/s1.log" "$D" --name n1 -P "$W/n1.pid" -L "$W/n1.logfile" --color never -v -v --strict
pid=$NODE_PID
expect_eq "the pid file holds the node's pid" "$(tr -d '[:space:]' < "$W/n1.pid" 2>/dev/null)" "$pid"
expect_eq "the node's name is saved in its directory" "$(cat "$D/.xapiand/node" 2>/dev/null)" "n1"
expect_log "the banner names the process" "$W/n1.logfile" "\\(pid:$pid\\)"
expect_log "the same banner goes to stderr beside the log file" "$W/s1.log" "\\(pid:$pid\\)"
expect_log "strict mode is announced" "$W/s1.log" "Activated strict mode by default"
stop_node TERM 20
expect_eq "SIGTERM ends an idle node with status 0" "$RC" 0
expect_log "the signal is named" "$W/s1.log" "Received SIGTERM scheduling shutdown"
expect_log "the node ends cleanly" "$W/s1.log" "Xapiand is cleanly done with all work"
expect_log "the pid file is removed on the way out" "$W/s1.log" "Removing the pid file"
[ ! -e "$W/n1.pid" ] && pass "the pid file is gone" || fail "the pid file is still there"

echo "== [2] the saved node name (manager: init, run)"
run_to "$W/s2.log" 30 "$BIN" --port "$(free_port)" -D "$D" --name other
expect_eq "another --name on the same directory ends with EX_SOFTWARE" "$RC" 70
expect_log "it says the name does not match" "$W/s2.log" "doesn't match the one saved in the node's directory"
expect_eq "the saved name is untouched" "$(cat "$D/.xapiand/node" 2>/dev/null)" "n1"
start_node "$W/s2b.log" "$D" --name N1 -v -v
expect_eq "the same name in another case starts" "$(cat "$D/.xapiand/node" 2>/dev/null)" "n1"
stop_node INT 20
expect_eq "SIGINT ends it with status 0" "$RC" 0
expect_log "SIGINT is named" "$W/s2b.log" "Received SIGINT scheduling shutdown"

echo "== [3] documents, signals that do not end the node (main: sig_handler; manager: signal_sig_impl)"
start_node "$W/s3.log" "$D" -v
expect_eq "the saved name is used without --name" "$(cat "$D/.xapiand/node" 2>/dev/null)" "n1"
st1=$(put idx 1 '{"word": "alpha", "n": 1}'); st2=$(put idx 2 '{"word": "beta", "n": 2}')
case "$st1$st2" in 20[0-9]20[0-9]) pass "two documents are indexed ($st1 $st2)";; *) fail "the PUTs answered $st1 and $st2";; esac
expect_log "GET returns the document" <(curl -s "127.0.0.1:$PORT/idx/1") '"word": ?"alpha"'
kill -HUP "$NODE_PID"; sleep 0.3
kill -0 "$NODE_PID" 2>/dev/null && pass "SIGHUP is ignored" || fail "SIGHUP ended the node"
kill -USR1 "$NODE_PID"; sleep 2
for part in "Threads:" "Manager:" "Databases:" "Schemas:"; do
	expect_log "SIGUSR1 prints $part" "$W/s3.log" "$part"
done
expect_log "SIGUSR1 is named" "$W/s3.log" "Signal received: (user defined signal 1|usr1)"
kill -0 "$NODE_PID" 2>/dev/null && pass "the node serves on after SIGUSR1" || fail "SIGUSR1 ended the node"
expect_eq "GET / still answers" "$(curl -s -o /dev/null -w '%{http_code}' "127.0.0.1:$PORT/")" 200
stop_node TERM 20
expect_eq "it ends with status 0" "$RC" 0

echo "== [4] a node asked to end while a request is in flight (manager: shutdown_checks, try_shutdown_timer_cb)"
hold() {  # a request whose body never ends
	"$PY" -I - "$PORT" >"$W/holder.log" 2>&1 <<'PY' &
import socket, sys, time
s = socket.create_connection(("127.0.0.1", int(sys.argv[1])))
s.sendall(b"PUT /held/1 HTTP/1.1\r\nHost: x\r\nContent-Type: application/json\r\nContent-Length: 5000\r\n\r\n")
s.sendall(b'{"held":1')
time.sleep(120)
PY
	HOLDER=$!; PIDS="$PIDS $HOLDER"
}
start_node "$W/s4.log" "$D" -v
hold; sleep 1
kill -TERM "$NODE_PID"; sleep 7
kill -0 "$NODE_PID" 2>/dev/null && pass "one SIGTERM waits for the request in flight" || fail "the node ended with a request in flight"
expect_log "the wait is announced" "$W/s4.log" "HTTP is busy"
kill -9 "$HOLDER" 2>/dev/null
wait_exit 20
expect_eq "it ends with status 0 once the request is gone" "$RC" 0

echo "== [5] repeated signals force the end (manager: shutdown_sig)"
start_node "$W/s5.log" "$D" -v -v
hold; sleep 1
kill -INT "$NODE_PID"; sleep 0.4; kill -INT "$NODE_PID"; sleep 0.4; kill -INT "$NODE_PID"
wait_exit 15
kill -9 "$HOLDER" 2>/dev/null
[ "$RC" != 124 ] && pass "three SIGINTs end a node that has a request in flight (status $RC)" || fail "the node outlived three SIGINTs"
expect_log "the second signal tries an immediate shutdown" "$W/s5.log" "Trying immediate shutdown"
expect_eq "the third ends it with EX_SOFTWARE" "$RC" 70

echo "== [6] refusals at start-up (main: usedir, adjustOpenFilesLimit)"
run_to "$W/r1.log" 30 "$BIN" --port "$(free_port)" -D ""
expect_eq "an empty data directory is EX_USAGE" "$RC" 64
expect_log "and says so" "$W/r1.log" "data directory must not be empty"
mkdir -p "$W/stray/.xapiand"; echo x > "$W/stray/.xapiand/junk"
run_to "$W/r2.log" 30 "$BIN" --port "$(free_port)" -D "$W/stray"
expect_eq "a working directory with foreign files is EX_DATAERR" "$RC" 65
expect_log "and says so" "$W/r2.log" "must be empty or a valid Xapiand database"
mkdir -p "$W/xapian/.xapiand/indexes"; : > "$W/xapian/.xapiand/indexes/iamglass"
run_to "$W/r3.log" 30 "$BIN" --port "$(free_port)" -D "$W/xapian"
expect_eq "a directory of a Xapian release is EX_DATAERR" "$RC" 65
expect_log "and points to the upgrade notes" "$W/r3.log" "holds a Xapian index"
echo x > "$W/afile"
run_to "$W/r4.log" 30 "$BIN" --port "$(free_port)" -D "$W/afile/below"
expect_eq "a directory that cannot be created is EX_OSFILE" "$RC" 72
expect_log "and says so" "$W/r4.log" "Cannot create working directory"
run_to "$W/r5.log" 30 "$BIN" --port "$(free_port)" -D "$W/lowfiles" --max-files 60
expect_eq "an open-files limit below the minimum is EX_OSFILE" "$RC" 72
expect_log "and says so" "$W/r5.log" "is not enough for the server to start"

echo "== [7] open-files limits (main: adjustOpenFilesLimit)"
# 300 descriptors cannot hold the default pool and clients: the pool is cut to (300 - 57) / 7 = 34 databases,
# yet the start is refused because the computed minimum (1471) is above the limit.
run_to "$W/l1.log" 30 "$BIN" --port "$(free_port)" --bind-address 127.0.0.1 -D "$W/lim1" --max-files 300 --database-pool-size 200 --max-clients 1000 -v
expect_eq "a limit of 300 with the default pool is EX_OSFILE" "$RC" 72
expect_log "it asks for at least 1471" "$W/l1.log" "open files limit of 300 is not enough for the server to start.*at least 1471"
expect_log "the pool is cut to what 300 descriptors hold" "$W/l1.log" "database_pool_size has been reduced to 34 to compensate for low limit"
# A process limit of 128 (soft and hard) that cannot be raised.
bash -c 'ulimit -n 128; exec "$0" "$@"' "$BIN" --port "$(free_port)" --bind-address 127.0.0.1 -D "$W/lim2" -v --database-pool-size 200 --max-clients 1000 --name l2 >"$W/l2.log" 2>&1
expect_eq "a process limit that cannot be raised is EX_OSFILE" "$?" 72
expect_log "the failed raise is reported" "$W/l2.log" "can't set maximum open files to .* EPERM"
expect_log "and the limit is not enough" "$W/l2.log" "is not enough for the server to start"
# A limit above the system's.
start_node "$W/l3.log" "$W/lim3" --max-files 1000000 -v -v
expect_log "a huge request is tried" "$W/l3.log" "(Increased|Decresed) maximum number of open files to|can't set maximum open files"
stop_node TERM 20
expect_eq "the node ends with status 0" "$RC" 0

echo "== [8] detach, pid file, syslog (main: detach, writepid)"
run_to "$W/d1.out" 30 "$BIN" -d --port "$(free_port)" --bind-address 127.0.0.1 -D "$W/det" -P "$W/det.pid" -L "$W/det.log" -v
expect_eq "the parent of --detach returns 0" "$RC" 0
for _ in $(seq 100); do [ -s "$W/det.pid" ] && break; sleep 0.1; done
dpid=$(tr -d '[:space:]' < "$W/det.pid" 2>/dev/null)
[ -n "$dpid" ] && kill -0 "$dpid" 2>/dev/null && pass "the daemon runs under the pid in the pid file ($dpid)" || fail "no daemon under pid '$dpid'"
for _ in $(seq 200); do grep -q "Ready to Rock" "$W/det.log" 2>/dev/null && break; sleep 0.1; done
expect_log "the daemon names its pid" "$W/det.log" "Daemon on process ID \\[$dpid\\] taking over"
kill -TERM "$dpid" 2>/dev/null
for _ in $(seq 200); do kill -0 "$dpid" 2>/dev/null || break; sleep 0.1; done
kill -0 "$dpid" 2>/dev/null && fail "the daemon did not end" || pass "the daemon ends on SIGTERM"
[ ! -e "$W/det.pid" ] && pass "and removes its pid file" || fail "its pid file is still there"
start_node "$W/sl.log" "$W/syslogd" -L syslog
stop_node TERM 20
expect_eq "a node logging to syslog ends with status 0" "$RC" 0

echo "== [9] dump and restore (main: dump_documents, restore_documents, cli_endpoint)"
run_to "$W/dump1.log" 60 "$BIN" -D "$D" --dump idx -o "$W/idx.mp" --color never
expect_eq "--dump ends with status 0" "$RC" 0
sha1=$(sed -n 's/.*Dump sha256 = \([0-9a-f]\{64\}\).*/\1/p' "$W/dump1.log" | head -1)
[ ${#sha1} = 64 ] && pass "the dump reports its sha256" || fail "no sha256 in the dump's log"
[ -s "$W/idx.mp" ] && pass "the dump file holds data ($(wc -c < "$W/idx.mp") bytes)" || fail "the dump file is empty"
run_to "$W/dump2.log" 60 "$BIN" -D "$D" --dump idx -o "$W/idx2.mp" --color never
expect_eq "a second dump has the same sha256" "$(sed -n 's/.*Dump sha256 = \([0-9a-f]\{64\}\).*/\1/p' "$W/dump2.log" | head -1)" "$sha1"
cmp -s "$W/idx.mp" "$W/idx2.mp" && pass "and the same bytes" || fail "the two dumps differ"
run_to "$W/rest1.log" 60 "$BIN" -D "$W/restored" --restore idx -i "$W/idx.mp" --color never
expect_eq "--restore ends with status 0" "$RC" 0
expect_eq "the restore reports the dump's sha256" "$(sed -n 's/.*Restore sha256 = \([0-9a-f]\{64\}\).*/\1/p' "$W/rest1.log" | head -1)" "$sha1"
run_to "$W/dump3.log" 60 "$BIN" -D "$W/restored" --dump idx -o "$W/idx3.mp" --color never
expect_eq "the restored index dumps to the same sha256" "$(sed -n 's/.*Dump sha256 = \([0-9a-f]\{64\}\).*/\1/p' "$W/dump3.log" | head -1)" "$sha1"
STDIN_FILE="$W/idx.mp" run_to "$W/rest2.log" 60 "$BIN" -D "$W/restored2" --restore idx --color never
expect_eq "--restore reads standard input without -i" "$RC" 0
expect_eq "from standard input it reports the same sha256" "$(sed -n 's/.*Restore sha256 = \([0-9a-f]\{64\}\).*/\1/p' "$W/rest2.log" | head -1)" "$sha1"
STDIN_FILE="$W/idx.mp" run_to "$W/rest3.log" 60 "$BIN" -D "$W/restored3" --restore idx -i - --color never
expect_eq "--restore -i - reads standard input" "$RC" 0
run_to "$W/e1.log" 60 "$BIN" -D "$D" --dump idx -o "$W/nodir/x.mp" --color never
expect_eq "--dump to a file that cannot be created is EX_OSFILE" "$RC" 72
expect_log "and says so" "$W/e1.log" "Cannot open file"
run_to "$W/e2.log" 60 "$BIN" -D "$W/restored4" --restore idx -i "$W/missing.mp" --color never
expect_eq "--restore of a missing file is EX_OSFILE" "$RC" 72
run_to "$W/e3.log" 60 "$BIN" -D "$D" --dump ../outside -o "$W/e3.mp" --color never
expect_eq "--dump of an index outside the data directory is EX_USAGE" "$RC" 64
expect_log "and says so" "$W/e3.log" "outside the data directory"
run_to "$W/e4.log" 60 "$BIN" -D "$D" --dump nosuchindex -o "$W/e4.mp" --color never
echo "not a dump" > "$W/bad.mp"
run_to "$W/e5.log" 60 "$BIN" -D "$W/restored5" --restore idx -i "$W/bad.mp" --color never
expect_log "--restore of a file that is not a dump says so" "$W/e5.log" "unsupported type"
expect_eq "and ends with EX_DATAERR" "$RC" 65
run_to "$W/e6.log" 60 "$BIN" -D "$W/restored6" --restore ../outside -i "$W/idx.mp" --color never
expect_eq "--restore of an index outside the data directory is EX_USAGE" "$RC" 64
echo "  INFO  --dump of an index that does not exist ends with status $RC"

echo "== [10] colors and log formats (main: main)"
color_run() {  # MODE: a dump whose banner and messages are logged in that mode
	run_to "$W/c-$1.log" 60 "$BIN" -D "$D" --dump idx -o "$W/c.mp" --color "$1"
}
color_run never;     expect_eq "--color never writes no escape" "$(grep -c "$ESC" "$W/c-never.log")" 0
color_run auto;      expect_eq "--color auto on a pipe writes no escape" "$(grep -c "$ESC" "$W/c-auto.log")" 0
color_run always;    [ "$(grep -c "$ESC" "$W/c-always.log")" -gt 0 ] && pass "--color always writes escapes" || fail "--color always wrote none"
color_run truecolor; expect_log "--color truecolor uses 24-bit colors" "$W/c-truecolor.log" "${ESC}\\[[0-9;]*38;2;"
color_run 256;       expect_log "--color 256 uses the 256-color palette" "$W/c-256.log" "${ESC}\\[[0-9;]*38;5;"
color_run 16;        [ "$(grep -c "$ESC" "$W/c-16.log")" -gt 0 ] && pass "--color 16 writes escapes" || fail "--color 16 wrote none"
expect_no_log "--color 16 uses no 256-color or 24-bit codes" "$W/c-16.log" "${ESC}\\[[0-9;]*38;[25];"
color_run stacked;   [ "$(grep -c "$ESC" "$W/c-stacked.log")" -gt 0 ] && pass "--color stacked writes escapes" || fail "--color stacked wrote none"
run_to "$W/f-iso.log" 60 "$BIN" -D "$D" --dump idx -o "$W/c.mp" --color never --log iso8601 --log milliseconds --log thread-names --log locations
expect_log "--log iso8601 stamps lines in ISO 8601" "$W/f-iso.log" "[0-9]{4}-[0-9]{2}-[0-9]{2} [0-9]{2}:[0-9]{2}:[0-9]{2}\\.[0-9]{3} "
expect_log "--log locations names source files" "$W/f-iso.log" "\\.(cc|h):[0-9]+"
expect_log "--log thread-names names the thread" "$W/f-iso.log" "MAIN"
run_to "$W/f-epoch.log" 60 "$BIN" -D "$D" --dump idx -o "$W/c.mp" --color never --log epoch --log microseconds
expect_log "--log epoch stamps lines in epoch seconds" "$W/f-epoch.log" "^[^0-9]{1,4}[0-9]{10}\\.[0-9]{6} "
run_to "$W/f-less.log" 60 "$BIN" -D "$D" --dump idx -o "$W/c.mp" --color never --log timeless
expect_no_log "--log timeless drops the stamp" "$W/f-less.log" "^[^0-9]{1,4}[0-9]{8,} "
run_to "$W/f-def.log" 60 "$BIN" -D "$D" --dump idx -o "$W/c.mp" --color never
expect_log "the default stamp is YYYYMMDDHHMMSS" "$W/f-def.log" "^[^0-9]{1,4}[0-9]{14} "

echo "== [11] HTTPS start-up and refusals (manager: make_servers' TLS failures; main: tls_setup)"
if command -v openssl >/dev/null; then
	WORK="$W/tls" "$HERE/tls_check.sh" "$BIN" "$(free_port)" >"$W/tls.log" 2>&1
	expect_eq "tls_check.sh passes" "$?" 0
	grep -E "^  (PASS|FAIL)" "$W/tls.log" | sed 's/^/    tls_check: /' | tail -40
else
	echo "  skip: no openssl"
fi

echo "main e2e: $checks checks, $fails failed"
[ "$fails" = 0 ]
