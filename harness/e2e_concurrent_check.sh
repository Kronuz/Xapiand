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


#
# e2e_concurrent_check.sh BINARY
#
# Two docs E2E captures started at the same moment must both be GREEN: each run takes its own port
# and work directory, so neither kills, rebinds or overwrites the other. CAPTURE names the capture
# script under test (default: e2e_capture.sh beside this file). Exits 0 when both runs are GREEN.
set -u

BIN="${1:?usage: e2e_concurrent_check.sh BINARY}"
HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$HARNESS_DIR/env.sh"
CAPTURE="${CAPTURE:-$HARNESS_DIR/e2e_capture.sh}"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/xa_e2e2.XXXXXX")"
trap 'rm -rf "$WORK"' EXIT

pids=()
for n in 1 2; do
	"$CAPTURE" "$BIN" "$WORK/run$n.json" > "$WORK/run$n.log" 2>&1 &
	pids+=($!)
done
wait "${pids[@]}"

failures=0
for n in 1 2; do
	if [ -s "$WORK/run$n.json" ] && "$PY" "$HARNESS_DIR/e2e_assert.py" "$WORK/run$n.json" 2>&1 | grep -q '^GREEN'; then
		echo "  PASS  concurrent run $n GREEN"
	else
		echo "  FAIL  concurrent run $n not GREEN"; tail -12 "$WORK/run$n.log" | sed 's/^/    /'
		failures=$((failures + 1))
	fi
done
exit $((failures > 0))
