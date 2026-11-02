#!/usr/bin/env bash
# removed_options_check.sh BINARY
#
# Phase 7c D15: the thread-count options that went with the offload pools, the commit threads and the
# bulk threads are gone, not refused: each is an unknown argument (the command line parser rejects
# it) and none is in --help. Exit 1 when one still parses or is still listed.
set -u
BIN=${1:-build/bin/xapiand}
[ -x "$BIN" ] || { echo "removed_options_check: no binary $BIN" >&2; exit 2; }
status=0
for opt in --committers --fsynchers --http-servers --http-clients --bulk-preparers --bulk-indexers; do
	out=$("$BIN" "$opt" 2 2>&1 >/dev/null </dev/null)
	rc=$?
	if [ "$rc" -eq 0 ] || ! printf '%s' "$out" | grep -q "not expected"; then
		echo "FAIL $opt still parses (exit $rc): $(printf '%s' "$out" | head -1)"
		status=1
	else
		echo "ok   $opt is an unknown argument (exit $rc)"
	fi
	if "$BIN" --help 2>&1 | grep -q -- "$opt"; then
		echo "FAIL $opt is still in --help"
		status=1
	fi
done
exit $status
