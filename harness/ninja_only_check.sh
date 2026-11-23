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

# A1/A3 (D168): CMake configures with Ninja only. Exits 0 only when
#  1. a fresh "Unix Makefiles" configure fails with the top-level guard's message, and
#  2. no tracked file outside the records of past runs configures CMake without -G Ninja.
set -u
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd -P)
[ -n "$ROOT" ] || exit 2
TREE=$(mktemp -d "$ROOT/.scratch/ninja-only-check.XXXXXX" 2> /dev/null || { mkdir -p "$ROOT/.scratch" && mktemp -d "$ROOT/.scratch/ninja-only-check.XXXXXX"; }) || exit 2
trap 'rm -rf "$TREE"' EXIT
status=0

# Own FETCHCONTENT_BASE_DIR: the shared .deps is never touched.
if cmake -G "Unix Makefiles" -S "$ROOT" -B "$TREE/build" -DFETCHCONTENT_BASE_DIR="$TREE/deps" > "$TREE/configure.log" 2>&1; then
	echo "FAIL: a Unix Makefiles configure succeeded"
	status=1
elif grep -q -- "-G Ninja" "$TREE/configure.log"; then
	echo "ok: a Unix Makefiles configure is refused: $(grep -m1 -- "-G Ninja" "$TREE/configure.log" | sed 's/^ *//')"
else
	echo "FAIL: a Unix Makefiles configure failed without the guard's message:"
	tail -20 "$TREE/configure.log"
	status=1
fi

# A configure is a cmake line with -S, -B or a source dir; a record of a past run keeps its command.
found=$(git -C "$ROOT" grep -nE '(^|[^[:alnum:]_./-])cmake +(-[SB]|\.\.?( |$|")|\$)' -- . \
	':!docs/gates' ':!docs/perf' ':!docs/reviews' ':!docs/status' ':!results' ':!.campaign' \
	':!docs/dev/engine-census.md' ':!harness/ninja_only_check.sh' | grep -vE -- '-G ?Ninja|-P ' || true)
if [ -n "$found" ]; then
	echo "FAIL: configures without -G Ninja:"
	echo "$found"
	status=1
else
	echo "ok: no configure without -G Ninja outside the records"
fi
exit $status
