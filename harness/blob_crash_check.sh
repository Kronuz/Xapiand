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

# blob_crash_check.sh -- kill -9 at each crash point of stored-blob writes and chunked uploads around a checkpoint,
# on a lone-voter shard group (cluster-release D12, D16; journal-replaces-wal A2).
#
# Runs tests/test_group_chunk_group.cc at every crash point: a child runs the script and SIGKILLs itself at the
# point, the parent reopens the group; every acknowledged blob must read back byte-equal and recovery must leave
# each file its bundle names at its length.
#
#   --mutant truncation   build with strata's file_set recovery skipping D16's truncation first; the check must fail
#                         (exit 0 then means the mutant survived: exit 1).
#
# Usage: harness/blob_crash_check.sh [--build DIR] [--mutant truncation]
# BUILD (default build): a configured build of this checkout (-DBUILD_TESTS=ON), with its fetched sources under
# FETCHCONTENT_BASE_DIR. GROUP_CHUNK_STRIDE=N checks every Nth crash point only.
set -u
REPO="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="${BUILD:-$REPO/build}"
MUTANT=""
while [ $# -gt 0 ]; do
	case "$1" in
		--build) BUILD="$2"; shift 2 ;;
		--mutant) MUTANT="$2"; shift 2 ;;
		*) echo "usage: $0 [--build DIR] [--mutant truncation]"; exit 2 ;;
	esac
done
TARGET=xapiand_test_group_chunk_group

restore=""
cleanup() { [ -n "$restore" ] && mv -f "$restore.orig" "$restore"; }
trap cleanup EXIT

if [ -n "$MUTANT" ]; then
	[ "$MUTANT" = truncation ] || { echo "unknown mutant: $MUTANT"; exit 2; }
	deps="$(sed -n 's/^FETCHCONTENT_BASE_DIR:PATH=//p' "$BUILD/CMakeCache.txt")"
	src="$deps/strata-src/include/strata/group/file_set.hh"
	grep -q 'file->truncate(f.length);' "$src" || { echo "the mutant does not apply to $src"; exit 127; }
	cp "$src" "$src.orig"
	restore="$src"
	# Recovery leaves a named file longer than the bundle names (D16 recovery step 2 skipped).
	sed -i.bak 's/file->truncate(f.length);/(void)f;/' "$src" && rm -f "$src.bak"
fi

cmake --build "$BUILD" -j4 --target "$TARGET" || exit 126
# ctest's test, so it runs where ctest runs it, with the same binary.
ctest --test-dir "$BUILD" -R "^test_group_chunk_group\$" --output-on-failure --timeout 3600
status=$?
if [ -n "$MUTANT" ]; then
	[ $status -ne 0 ] && echo "mutant $MUTANT: killed" || echo "mutant $MUTANT: SURVIVED"
fi
exit $status
