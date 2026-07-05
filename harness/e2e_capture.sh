#!/usr/bin/env bash
#
# e2e_capture.sh BINARY OUT.json [DATADIR]
#
# Boot a fresh Xapiand node and capture a newman JSON report of the doc collection.
# This encodes the recipe (and the gotchas) the Leg-2 inversion work relies on:
#
#   * Kill any node squatting on :8880 FIRST. A leftover node makes our fresh node
#     fail to bind (EADDRINUSE) and exit, and newman then silently hits the STALE
#     node -- which looks like a bogus regression. (This bit us once; see the
#     `kill -0 $pid` check below: we confirm OUR node is the one serving.)
#   * --solo (no discovery/replication ports) + a FRESH data dir + a FIXED --name
#     (so volatile node-identity does not diverge the bodydiff).
#   * newman exits 1 because of the 73 pre-existing doc failures -- that is NORMAL;
#     the report is still written. Compare with e2e_diff.py / bodydiff.py, not the
#     exit code.
#
# Binaries (already built this program):
#   ours     -> build/bin/xapiand                      (note: bin/, not build/xapiand)
#
# The current e2e baseline is captured from THIS project's own build at 23e1e4540
# (right after the Xapian 2.0.0 migration + native multi-db distributed match), so
# the regression net now tracks the current architecture against itself. Earlier the
# baseline was the pre-migration master oracle (7bd295b, in ../xapiand-master-bench),
# which validated the migration and has served its purpose.
#
# Example:
#   harness/e2e_capture.sh build/bin/xapiand harness/results/e2e_base_23e1e4540.json
#   harness/e2e_capture.sh build/bin/xapiand /tmp/ours.json
set -u

BIN="${1:?usage: e2e_capture.sh BINARY OUT.json [DATADIR]}"
OUT="${2:?usage: e2e_capture.sh BINARY OUT.json [DATADIR]}"
DATADIR="${3:-/tmp/xe2e_$$}"

HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
REPO="$(dirname "$HARNESS_DIR")"
COLL="${COLL:-/tmp/xe2e_collection.json}"

# Generate the Postman collection once (regenerate if missing); reuse it for every
# node so the inputs are identical across baseline and ours.
if [ ! -f "$COLL" ]; then
	python3 "$REPO/docs_to_postman.py" > "$COLL" || { echo "docs_to_postman.py failed"; exit 1; }
fi
echo "collection: $COLL ($(wc -c < "$COLL") bytes)"

pkill -9 -f 'bin/xapiand' 2>/dev/null; sleep 1   # no squatter on :8880
rm -rf "$DATADIR"; mkdir -p "$DATADIR"
"$BIN" --port 8880 --name benchnode --solo -D "$DATADIR" >/tmp/xe2e_node.log 2>&1 &
pid=$!

for _ in $(seq 1 120); do
	if ! kill -0 "$pid" 2>/dev/null; then
		echo "node exited before ready (EADDRINUSE / leftover node on :8880?):"
		grep -iE 'error|exception|bind' /tmp/xe2e_node.log | head -3
		exit 1
	fi
	[ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 localhost:8880/ 2>/dev/null)" = "200" ] && break
	sleep 0.5
done

echo "node up (pid $pid); newman -> $OUT"
newman run "$COLL" --reporters json --reporter-json-export "$OUT" --timeout-request 30000 >/tmp/xe2e_newman.log 2>&1
echo "newman exit $? (1 is normal: 73 pre-existing failures) -> $(wc -c < "$OUT" 2>/dev/null) bytes"

kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
exit 0   # a captured report is success; the killed node's wait-status is not our verdict
