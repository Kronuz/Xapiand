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

# engine_e2e.sh BINARY OUTDIR -- the docs E2E suite on a fresh node.
#
# Writes OUTDIR/strata.json (the newman report) and prints the assertion verdict.
# Needs newman 6 first on PATH (newman 4.5 fails every request). Run it under a hold:
#   quiet light NAME MIN "docs E2E" -- harness/engine_e2e.sh build/bin/xapiand OUTDIR
set -u
BIN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUT="$(mkdir -p "$2" && cd "$2" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
export PORT="${PORT:-18951}" E2E_TMP="$OUT"
rm -f "$OUT/strata.json"
cap=0; "$HERE/e2e_capture.sh" "$BIN" "$OUT/strata.json" "$OUT/data" > "$OUT/capture.log" 2>&1 || cap=$?
rm -rf "$OUT/data"
[ "$cap" -eq 0 ] && [ -s "$OUT/strata.json" ] || { echo "capture failed (exit $cap): $OUT/capture.log"; tail -5 "$OUT/capture.log"; exit 1; }
python3 "$HERE/e2e_assert.py" "$OUT/strata.json" > "$OUT/assert.txt"; rc=$?; head -1 "$OUT/assert.txt"
grep -q '^GREEN' "$OUT/assert.txt" || exit "$(( rc ? rc : 1 ))"
