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

# signal_check.sh -- capture the SIGINT/SIGTERM shutdown behavior of a single
# node, so the libev->reactor manager migration can be proven to preserve
# it. Scenarios: (1) one SIGINT = graceful shutdown; (2) two rapid SIGINT =
# escalate to "immediate"; (3) three = force. For each: measure wall time from
# first signal to process exit, and dump the shutdown-relevant log tail.
set -u

REPO="$(cd "$(dirname "$0")/.." && pwd)"
BIN="${BIN:-$REPO/build/bin/xapiand}"
PORT=8899
RUNDIR="/tmp/xsig_$$"
mkdir -p "$RUNDIR"

cleanup() { [ -n "${PID:-}" ] && kill -9 "$PID" 2>/dev/null; rm -rf "$RUNDIR"; }
trap cleanup EXIT

start_node() {
	"$BIN" --port "$PORT" --name signode -D "$RUNDIR/n" \
		>"$RUNDIR/node.log" 2>&1 &
	PID=$!
}

wait_ready() {
	local deadline=$(( $(date +%s) + 40 ))
	while [ "$(date +%s)" -lt "$deadline" ]; do
		kill -0 "$PID" 2>/dev/null || { echo "  node died during startup"; return 1; }
		if curl -s --max-time 3 "localhost:$PORT/" >/dev/null 2>&1; then return 0; fi
		sleep 0.5
	done
	echo "  node never became ready"; return 1
}

# $1 = number of SIGINTs, $2 = gap seconds between them
scenario() {
	local n="$1" gap="$2" label="$3"
	echo "=== scenario: $label ($n x SIGINT, gap ${gap}s) ==="
	start_node
	if ! wait_ready; then echo "  SETUP FAIL"; return 1; fi
	echo "  ready (pid $PID); sending signals"
	local t0 t1
	t0=$(python3 -c 'import time;print(time.time())')
	for ((k=1;k<=n;k++)); do
		kill -INT "$PID" 2>/dev/null
		[ "$k" -lt "$n" ] && sleep "$gap"
	done
	# wait for exit (max 30s)
	local deadline=$(( $(date +%s) + 30 ))
	while kill -0 "$PID" 2>/dev/null && [ "$(date +%s)" -lt "$deadline" ]; do sleep 0.1; done
	if kill -0 "$PID" 2>/dev/null; then
		echo "  DID NOT EXIT within 30s -- killing"; kill -9 "$PID" 2>/dev/null; PID=""
		return 1
	fi
	t1=$(python3 -c 'import time;print(time.time())')
	printf "  EXITED after %.2fs\n" "$(python3 -c "print($t1-$t0)")"
	echo "  --- shutdown log tail ---"
	grep -iE "shutdown|signal|Ready to|cleanly|uncleanly|immediate|breaking|exiting" "$RUNDIR/node.log" | tail -15 | sed 's/^/    /'
	PID=""
	echo
}

scenario 1 0   "single graceful"
scenario 2 0.3 "double rapid"
scenario 3 0.3 "triple force"
echo "=== baseline capture complete ==="
