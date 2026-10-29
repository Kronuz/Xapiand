#!/usr/bin/env bash
# engine_e2e.sh BINARY OUTDIR -- the docs E2E suite on each engine (gate 7a, T6), one fresh solo node per run.
#
# XAPIAND_ENGINE forces the engine of every index the suite creates (D8's override). Writes OUTDIR/xapian.json and
# OUTDIR/strata.json (newman reports) and prints the assertion verdict of each, then the diff of strata against xapian.
# Needs newman 6 first on PATH (newman 4.5 fails every request). Run it under a hold:
#   quiet light NAME MIN "docs E2E" -- harness/engine_e2e.sh build/bin/xapiand OUTDIR
set -u
BIN="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUT="$(mkdir -p "$2" && cd "$2" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"
export PORT="${PORT:-18951}" E2E_TMP="$OUT"
for ENG in xapian strata; do
	XAPIAND_ENGINE=$ENG "$HERE/e2e_capture.sh" "$BIN" "$OUT/$ENG.json" "$OUT/data_$ENG" > "$OUT/$ENG.capture.log" 2>&1
	echo "== $ENG =="
	python3 "$HERE/e2e_assert.py" "$OUT/$ENG.json" > "$OUT/$ENG.assert.txt"; head -1 "$OUT/$ENG.assert.txt"
	rm -rf "$OUT/data_$ENG"
done
python3 "$HERE/e2e_diff.py" "$OUT/strata.json" "$OUT/xapian.json" | tee "$OUT/diff.txt" | tail -15
