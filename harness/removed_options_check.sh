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
