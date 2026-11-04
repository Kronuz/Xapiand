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
# e2e_check.sh   — the one-liner correctness regression net.
#
# Captures a newman report from OUR node (build/bin/xapiand) and diffs it against
# the saved baseline report, both for assertion outcomes (e2e_diff.py) and
# response bodies (bodydiff.py). "Green" = parity with the baseline (the doc suite
# has 73 pre-existing aspirational failures that fail on both, so this is parity,
# not 100% pass).
#
# The baseline (harness/results/e2e_base_23e1e4540.json, gitignored, ~61MB) was
# captured from THIS project's own build at 23e1e4540 -- right after the Xapian
# 2.0.0 migration + the native multi-db distributed match landed -- so the check is
# now a forward-looking regression net for the current architecture (it used to be
# captured from the pre-migration master binary, which validated the migration and
# has served its purpose). If it is missing, recapture it from the current build:
#   harness/e2e_capture.sh build/bin/xapiand \
#       harness/results/e2e_base_23e1e4540.json
#
# EXPECTED green-state bodydiff: only the genuinely run-to-run volatile fields --
# POST /twitter/user/ (a base62 auto-id), GET / (node runtime info), and
# GET /:metrics (Prometheus counters). More than those = inspect.
set -u

HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HARNESS_DIR")"
BASE="${BASE:-$HARNESS_DIR/results/e2e_base_23e1e4540.json}"
OURS="${OURS:-/tmp/ours.json}"

if [ ! -f "$BASE" ]; then
	echo "missing baseline $BASE"
	echo "recapture it: harness/e2e_capture.sh build/bin/xapiand $BASE"
	exit 1
fi

"$HARNESS_DIR/e2e_capture.sh" "$REPO/build/bin/xapiand" "$OURS" || exit 1

echo
echo "=== e2e_diff (ours vs baseline) ==="
python3 "$HARNESS_DIR/e2e_diff.py" "$OURS" "$BASE"
echo
echo "=== bodydiff (baseline vs ours) ==="
python3 "$HARNESS_DIR/bodydiff.py" "$BASE" "$OURS"
