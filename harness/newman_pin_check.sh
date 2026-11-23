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

# newman_pin_check.sh BINARY OUTDIR -- the docs E2E passes with a wrong newman first on PATH.
# A stub reporting 4.5.0 is put first on PATH; engine_e2e.sh must still run the pinned newman and print its version.
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
OUT="$(mkdir -p "$2" && cd "$2" && pwd)"
want="$(sed -n 's/.*"newman": *"\([^"]*\)".*/\1/p' "$HERE/package.json")"
mkdir -p "$OUT/stub"
printf '#!/bin/sh\n[ "$1" = --version ] && echo 4.5.0 && exit 0\nexit 1\n' > "$OUT/stub/newman"
chmod +x "$OUT/stub/newman"
PATH="$OUT/stub:$PATH"
[ "$(newman --version)" = 4.5.0 ] || { echo "stub is not first on PATH"; exit 1; }
log="$("$HERE/engine_e2e.sh" "$1" "$OUT/e2e" 2>&1)"; rc=$?
printf '%s\n' "$log"
printf '%s\n' "$log" | grep -q "^newman $want " || { echo "FAIL: pinned newman $want was not used"; exit 1; }
[ "$rc" -eq 0 ] || { echo "FAIL: engine_e2e.sh exit $rc"; exit "$rc"; }
echo "PASS: ran pinned newman $want with a 4.5.0 first on PATH"
