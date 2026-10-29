#!/usr/bin/env bash
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
"$HERE/e2e_capture.sh" "$BIN" "$OUT/strata.json" "$OUT/data" > "$OUT/capture.log" 2>&1
python3 "$HERE/e2e_assert.py" "$OUT/strata.json" > "$OUT/assert.txt"; head -1 "$OUT/assert.txt"
rm -rf "$OUT/data"
