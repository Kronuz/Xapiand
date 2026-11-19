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

# light_share_t10.sh BIN OUTDIR ARM...
#
# T10's flood (harness/lua_bounds.py at 4 loops) once per ARM, in the order given. ARM is "off" or a heavy share
# (LS_HEAVY_SHARE); "SHARE@MS" also sets LS_LIGHT_WINDOW_MS (0: the share always binds). Exits nonzero when any arm fails.
set -u
BIN=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); OUT=$(mkdir -p "$2" && cd "$2" && pwd); shift 2
HERE=$(cd "$(dirname "$0")" && pwd)
[ -x "$BIN" ] || { echo "no binary $BIN" >&2; exit 1; }
i=0; FAILED=0
for arm in "$@"; do
	i=$((i + 1))
	unset LS_HEAVY_SHARE LS_LIGHT_WINDOW_MS
	if [ "$arm" != off ]; then export LS_HEAVY_SHARE=${arm%@*}; [ "$arm" = "${arm%@*}" ] || export LS_LIGHT_WINDOW_MS=${arm#*@}; fi
	python3 "$HERE/lua_bounds.py" "$BIN" --port 18995 --loops 4 --out "$OUT/t10-$i-$arm.json" > "$OUT/t10-$i-$arm.log" 2>&1
	rc=$?; echo "arm $i $arm rc $rc"; [ $rc -eq 0 ] || FAILED=1
	grep -h "light GET" "$OUT/t10-$i-$arm.log"
done
unset LS_HEAVY_SHARE LS_LIGHT_WINDOW_MS
exit $FAILED
