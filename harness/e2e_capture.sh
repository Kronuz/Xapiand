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
#   * newman exits 1 if ANY assertion fails; in the green state every assertion passes,
#     so exit 0 is expected. The report is written either way -- judge the run with
#     e2e_assert.py (the gate), not the exit code.
#
# Binaries (already built this program):
#   ours     -> build/bin/xapiand                      (note: bin/, not build/xapiand)
#
# The captured report feeds the assertion-only gate (e2e_assert.py): the docs carry
# their own pm.test assertions and the generator synthesises a default for any request
# that lacks one, so the gate needs no response-body baseline. (e2e_check.sh can still
# diff a report against a saved reference report for a deeper body comparison.)
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

# Regenerate the Postman collection every run from the docs. The doc-driven suite's
# whole premise is that the docs are the single source of truth, so a stale cached
# collection must never mask a doc or generator change. Cheap (<1s).
python3 "$REPO/docs_to_postman.py" > "$COLL" || { echo "docs_to_postman.py failed"; exit 1; }
echo "collection: $COLL ($(wc -c < "$COLL") bytes)"

# Free :8880 if a previous node is squatting, killing only the PID that holds the port.
# Do NOT match by name (e.g. pkill -f 'bin/xapiand'): $BIN is an argument to THIS script,
# so a name match also matches e2e_capture.sh's own command line and SIGKILLs itself --
# which is exactly what killed the e2e step in CI on Linux (macOS pkill -f matched
# differently, so it slipped through locally).
squatter=""
if command -v lsof >/dev/null 2>&1; then
	squatter=$(lsof -ti tcp:8880 2>/dev/null || true)
elif command -v fuser >/dev/null 2>&1; then
	squatter=$(fuser 8880/tcp 2>/dev/null | tr -cd '0-9 ' || true)
fi
[ -n "$squatter" ] && kill -9 $squatter 2>/dev/null || true
sleep 1
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
echo "newman exit $? (0 expected when green; the verdict is e2e_assert.py) -> $(wc -c < "$OUT" 2>/dev/null) bytes"

kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
exit 0   # a captured report is success; the killed node's wait-status is not our verdict
