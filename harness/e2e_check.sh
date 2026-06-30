#!/usr/bin/env bash
#
# e2e_check.sh   — the one-liner correctness loop for the Leg-2 inversion work.
#
# Captures a newman report from OUR node (build/bin/xapiand) and diffs it against
# the saved 7bd295b baseline report, both for assertion outcomes (e2e_diff.py) and
# response bodies (bodydiff.py). "Green" = parity with the baseline, NOT 100% pass
# (the doc suite has 73 pre-existing aspirational failures that fail on both).
#
# The baseline report is saved at harness/results/e2e_base_7bd295b.json (gitignored,
# ~61MB). If it is missing, recapture it from the baseline binary first:
#   harness/e2e_capture.sh ../xapiand-master-bench/build/bin/xapiand \
#       harness/results/e2e_base_7bd295b.json
#
# EXPECTED green-state bodydiff: exactly 3 volatile-field divergences --
# POST /twitter/user/ (a base62 auto-id), GET / (node runtime info), and
# GET /:metrics (Prometheus counters). More than those 3 = inspect.
set -u

HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HARNESS_DIR")"
BASE="${BASE:-$HARNESS_DIR/results/e2e_base_7bd295b.json}"
OURS="${OURS:-/tmp/ours.json}"

if [ ! -f "$BASE" ]; then
	echo "missing baseline $BASE"
	echo "recapture it: harness/e2e_capture.sh ../xapiand-master-bench/build/bin/xapiand $BASE"
	exit 1
fi

"$HARNESS_DIR/e2e_capture.sh" "$REPO/build/bin/xapiand" "$OURS" || exit 1

echo
echo "=== e2e_diff (ours vs baseline) ==="
python3 "$HARNESS_DIR/e2e_diff.py" "$OURS" "$BASE"
echo
echo "=== bodydiff (baseline vs ours) ==="
python3 "$HARNESS_DIR/bodydiff.py" "$BASE" "$OURS"
