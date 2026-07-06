#!/usr/bin/env bash
#
# e2e_baseline.sh [BINARY]
#
# Refresh the committed e2e baseline (harness/e2e_baseline.json) from a binary
# (default: the current build). Run this ONLY when you have intentionally changed
# documented behaviour and verified the new responses are correct -- the resulting
# git diff of e2e_baseline.json shows exactly which assertions changed, so it is
# reviewable.
#
# The baseline is DISTILLED: per request it keeps just the failed-assertion set and
# the status code (keyed by the doc-derived request name), no response bodies. That
# is a few KB -- small enough to commit and share -- unlike the 60 MB raw newman
# report, which is gitignored under results/ and drifts per machine. Matching by
# name (see e2e_diff.py) means adding or removing doc examples does not require a
# refresh; only a genuine behaviour change does.
set -u

HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HARNESS_DIR")"
BIN="${1:-$REPO/build/bin/xapiand}"
BASELINE="$HARNESS_DIR/e2e_baseline.json"
RAW="/tmp/xe2e_baseline_raw.json"

[ -x "$BIN" ] || { echo "binary not found/executable: $BIN"; exit 1; }

echo "capturing a fresh report from $BIN ..."
"$HARNESS_DIR/e2e_capture.sh" "$BIN" "$RAW" || { echo "capture failed"; exit 1; }

python3 "$HARNESS_DIR/e2e_diff.py" --distill "$RAW" > "$BASELINE" || { echo "distill failed"; exit 1; }

sha="$(cd "$REPO" && git rev-parse --short HEAD 2>/dev/null || echo unknown)"
n="$(python3 -c 'import json,sys; print(len(json.load(open(sys.argv[1]))["e2e_baseline"]))' "$BASELINE")"
echo "baseline refreshed: $BASELINE ($n requests, $(wc -c < "$BASELINE") bytes, at commit $sha)"
echo "review the git diff, then commit it."
