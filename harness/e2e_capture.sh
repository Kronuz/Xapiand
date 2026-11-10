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
# e2e_capture.sh BINARY OUT.json [DATADIR]
#
# Boot a fresh Xapiand node and capture a newman JSON report of the doc collection.
# This encodes the recipe (and the gotchas) the Leg-2 inversion work relies on:
#
#   * Every run takes its own port and its own work directory, so concurrent runs (CI jobs
#     on one host) never meet. An explicit PORT is freed first: only a LISTENING process on
#     it is killed (never a client of it). A leftover node makes our fresh node fail to
#     bind (EADDRINUSE) and exit, and newman then silently hits the STALE node -- which
#     looks like a bogus regression; see the `kill -0 $pid` check below.
#   * (no discovery/replication ports) + a FRESH data dir + a FIXED --name
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
# XAPIAND_FLAGS adds node flags (--tls-cert/--tls-key); SCHEME=https probes and runs the
# collection over TLS, trusting CA (newman 6 or later, for --ssl-extra-ca-certs). PORT
# pins the node port (default: a free one); E2E_TMP pins the work directory and keeps it.
#
# Example:
#   harness/e2e_capture.sh build/bin/xapiand harness/results/e2e_base_23e1e4540.json
#   harness/e2e_capture.sh build/bin/xapiand /tmp/ours.json
set -u

BIN="${1:?usage: e2e_capture.sh BINARY OUT.json [DATADIR]}"
OUT="${2:?usage: e2e_capture.sh BINARY OUT.json [DATADIR]}"
HARNESS_DIR="$(cd "$(dirname "$0")" && pwd)"
. "$HARNESS_DIR/env.sh"
if [ -n "${E2E_TMP:-}" ]; then
	TMPD="$E2E_TMP"; mkdir -p "$TMPD"
else
	TMPD="$(mktemp -d "${TMPDIR:-/tmp}/xe2e.XXXXXX")"
	trap 'rm -rf "$TMPD"' EXIT
fi
DATADIR="${3:-$TMPD/xe2e_$$}"

REPO="$(dirname "$HARNESS_DIR")"
COLL="${COLL:-$TMPD/xe2e_collection.json}"
SCHEME="${SCHEME:-http}"
PICKED_PORT=""
[ -n "${PORT:-}" ] || { PORT="$(free_port)" && PICKED_PORT=1; }
CA="${CA:-}"
read -r -a node_flags <<< "${XAPIAND_FLAGS:-}"
probe_ca=()
newman_ca=()
# newman 4 fails every request (it mangles the collection's templating), so the major version is checked on every run.
major=$(newman --version 2>/dev/null | cut -d. -f1)
[ "${major:-0}" -ge 6 ] || {
	echo "the docs E2E needs newman 6 or later first on PATH (found '$(newman --version 2>/dev/null)')."
	echo "Install one beside the work, not globally: npm i --prefix DIR newman@6.2.3; PATH=DIR/node_modules/.bin:\$PATH"
	exit 1
}
if [ "$SCHEME" = https ]; then
	[ -n "$CA" ] || { echo "SCHEME=https needs CA"; exit 1; }
	probe_ca=(--cacert "$CA")
	newman_ca=(--ssl-extra-ca-certs "$CA")
fi

# Regenerate the Postman collection every run from the docs. The doc-driven suite's
# whole premise is that the docs are the single source of truth, so a stale cached
# collection must never mask a doc or generator change. Cheap (<1s).
XAPIAND_URL="$SCHEME://localhost:$PORT" python3 "$REPO/docs_to_postman.py" > "$COLL" || { echo "docs_to_postman.py failed"; exit 1; }
echo "collection: $COLL ($(wc -c < "$COLL") bytes)"

# Free an explicit PORT if a previous node is squatting, killing only the PID that listens on it.
# Do NOT match by name (e.g. pkill -f 'bin/xapiand'): $BIN is an argument to THIS script,
# so a name match also matches e2e_capture.sh's own command line and SIGKILLs itself --
# which is exactly what killed the e2e step in CI on Linux (macOS pkill -f matched
# differently, so it slipped through locally).
squatter=""
if [ -n "$PICKED_PORT" ]; then
	:
elif command -v lsof >/dev/null 2>&1; then
	squatter=$(lsof -ti "tcp:$PORT" -sTCP:LISTEN 2>/dev/null || true)
elif command -v ss >/dev/null 2>&1; then
	squatter=$(ss -Htlnp "sport = :$PORT" 2>/dev/null | grep -o 'pid=[0-9]*' | cut -d= -f2 | sort -u || true)
fi
if [ -n "$squatter" ]; then kill -9 $squatter 2>/dev/null || true; sleep 1; fi
rm -rf "$DATADIR"; mkdir -p "$DATADIR"
"$BIN" --port "$PORT" --name benchnode -D "$DATADIR" ${node_flags[@]+"${node_flags[@]}"} >"$TMPD/xe2e_node.log" 2>&1 &
pid=$!

for _ in $(seq 1 120); do
	if ! kill -0 "$pid" 2>/dev/null; then
		echo "node exited before ready (EADDRINUSE / leftover node on :$PORT?):"
		grep -iE 'error|exception|bind' "$TMPD/xe2e_node.log" | head -3
		exit 1
	fi
	[ "$(curl -s -o /dev/null -w '%{http_code}' --max-time 2 ${probe_ca[@]+"${probe_ca[@]}"} "$SCHEME://localhost:$PORT/" 2>/dev/null)" = "200" ] && break
	sleep 0.5
done

echo "node up (pid $pid); newman -> $OUT"
newman run "$COLL" ${newman_ca[@]+"${newman_ca[@]}"} --reporters json --reporter-json-export "$OUT" --timeout-request 30000 >"$TMPD/xe2e_newman.log" 2>&1
rc=$?
echo "newman exit $rc (0 expected when green; the verdict is e2e_assert.py) -> $(wc -c < "$OUT" 2>/dev/null) bytes"
[ "$rc" -eq 0 ] || { echo "node log tail:"; tail -20 "$TMPD/xe2e_node.log"; }

kill "$pid" 2>/dev/null; wait "$pid" 2>/dev/null
exit 0   # a captured report is success; the killed node's wait-status is not our verdict
