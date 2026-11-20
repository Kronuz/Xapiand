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

# light_share_probe.sh OUTDIR [PATCH [PROBE...]]
#
# Builds Kronuz/task at Xapiand's pin plus PATCH (default harness/light_share_task.patch) with clang and TSan, runs
# task's asio, lanes and deadline_asio tests with the share off, 1 and 2, then the probes of
# harness/light_share_probe.cc (default: cap parked bypass heavyonly gate). Exits nonzero on a failed or hanging
# test, a TSan report, or a failed probe: a probe that fails is the regression the patch must not have.
set -u
HERE=$(cd "$(dirname "$0")" && pwd); SRC=$(dirname "$HERE")
OUT=$(mkdir -p "$1" && cd "$1" && pwd); PATCH=${2:-$HERE/light_share_task.patch}; shift 2 2>/dev/null || shift $#
PROBES=${*:-cap parked bypass heavyonly gate}
PIN=$(sed -n 's|.*Kronuz/task.git GIT_TAG \([0-9a-f]*\).*|\1|p' "$SRC/CMakeLists.txt")
DEPS=$SRC/.deps; [ -d "$DEPS/asio-src" ] || DEPS=$HOME/.scratch/campaign/jobs/slots/light-share/.deps
ASIO=$DEPS/asio-src/asio/include; WHEEL=$DEPS/wheel-src
T=$OUT/task; rm -rf "$T"
git clone -q "$HOME/code/Kronuz/task" "$T" 2>/dev/null || git clone -q https://github.com/Kronuz/task.git "$T"
git -C "$T" checkout -q "$PIN" && git -C "$T" apply "$PATCH" || { echo "patch failed"; exit 3; }
CXX="clang++ -std=c++20 -O1 -g -fno-omit-frame-pointer -fsanitize=thread -DASIO_STANDALONE -I$T -I$ASIO -I$WHEEL"
B=$OUT/bin; mkdir -p "$B"
for t in asio_test lanes_test deadline_asio_test; do $CXX "$T/test/$t.cc" -o "$B/$t" -lpthread || exit 3; done
$CXX "$HERE/light_share_probe.cc" -o "$B/share_probe" -lpthread || exit 3
echo "built task $PIN + $(basename "$PATCH")"
BAD=0
run() {  # run LABEL ENV... -- CMD...
	local label=$1; shift
	local envs=(); while [ "$1" != -- ]; do envs+=("$1"); shift; done; shift
	local log=$OUT/$label.log
	env -u LS_HEAVY_SHARE -u LS_LIGHT_WINDOW_MS TSAN_OPTIONS="halt_on_error=0 second_deadlock_stack=1" "${envs[@]}" timeout 300 "$@" > "$log" 2>&1
	local rc=$? races; races=$(grep -c "WARNING: ThreadSanitizer" "$log")
	echo "$label rc $rc tsan $races"; grep -h "^cap:\|^parked:\|^bypass:\|^heavyonly:\|^gate:" "$log" | sed 's/^/    /'
	[ "$races" -eq 0 ] && [ $rc -eq 0 ] || BAD=1
}
for share in off 1 2; do
	for t in asio_test lanes_test deadline_asio_test; do
		if [ $share = off ]; then run "$t-$share" -- "$B/$t"; else run "$t-$share" LS_HEAVY_SHARE=$share LS_LIGHT_WINDOW_MS=0 -- "$B/$t"; fi
	done
done
for p in $PROBES; do
	case $p in
	cap) run cap-1 LS_HEAVY_SHARE=1 LS_LIGHT_WINDOW_MS=0 -- "$B/share_probe" cap; run cap-3 LS_HEAVY_SHARE=3 LS_LIGHT_WINDOW_MS=0 -- "$B/share_probe" cap ;;
	parked) run parked-1 LS_HEAVY_SHARE=1 LS_LIGHT_WINDOW_MS=0 -- "$B/share_probe" parked ;;
	bypass) run bypass-4 LS_HEAVY_SHARE=4 LS_LIGHT_WINDOW_MS=0 -- "$B/share_probe" bypass ;;
	heavyonly) run heavyonly-2 LS_HEAVY_SHARE=2 LS_LIGHT_WINDOW_MS=2000 -- "$B/share_probe" heavyonly ;;
	gate) run gate-2 LS_HEAVY_SHARE=2 LS_LIGHT_WINDOW_MS=2000 -- "$B/share_probe" gate ;;
	esac
done
echo "exit $BAD"
exit $BAD
