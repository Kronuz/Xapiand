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

# light_share_ab.sh BIN OUTDIR ARM... [-- STALL_LOAD_ARGS]
#
# T4/T5's mix (x7-gate's command: 20,000 accounts, light 200/s, 20 s phases, 4 heavy clients per reactor, a
# prebuilt RESTORE stream) once per ARM, in the order given. ARM is "off" or a heavy share (LS_HEAVY_SHARE, the
# loops that may run a heavy turn at once). Prints each arm's light p99s, worst loop lag and heavy throughput.
set -u
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); OUT=$(mkdir -p "$2" && cd "$2" && pwd); shift 2
ARMS=(); while [ $# -gt 0 ] && [ "$1" != -- ]; do ARMS+=("$1"); shift; done; [ $# -gt 0 ] && shift
HERE=$(cd "$(dirname "$0")" && pwd)
echo "commit $(git -C "$HERE" rev-parse --short HEAD), $(sysctl -n hw.ncpu 2>/dev/null || nproc) CPUs"
[ -x "$BIN" ] || { echo "no binary $BIN" >&2; exit 1; }
i=0; FAILED=0
for arm in "${ARMS[@]}"; do
	i=$((i + 1))
	if [ "$arm" = off ]; then unset LS_HEAVY_SHARE; else export LS_HEAVY_SHARE=$arm; fi
	python3 "$HERE/stall_load.py" --bin "$BIN" --target 127.0.0.1:18980 --workdir "$OUT/w" --accounts-docs 20000 \
		--phase both --rate 200 --duration 20 --no-assert --restore-prebuilt 8 "$@" \
		--out "$OUT/ls-$i-$arm.json" 2> "$OUT/ls-$i-$arm.log"
	rc=$?; echo "arm $i $arm rc $rc"; [ $rc -eq 0 ] || FAILED=1
done
unset LS_HEAVY_SHARE
rm -rf "$OUT/w"
python3 "$HERE/light_share_report.py" "$OUT"/ls-*.json || FAILED=1
exit $FAILED
