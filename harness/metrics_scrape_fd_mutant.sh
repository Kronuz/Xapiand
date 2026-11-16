#!/bin/bash
# Mutant for test_manager_fd_count: the fetched system's listing counts its own directory descriptor (the count is
# off by one). Run after the quest's build (`campaign job submit --build ... --fails-without`); the test must fail.
set -u
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE" || exit 1
SRC=.deps/system-src/system.cc
trap 'git -C .deps/system-src checkout -q system.cc' EXIT
sed -i.bak 's/ && fd != self)/)/' "$SRC" && rm -f "$SRC.bak"
# Exits 1 only when the test ran and failed a CHECK; a mutation or build error exits 0, so --fails-without rejects it.
if grep -q "fd != self" "$SRC"; then echo "mutation did not apply" >&2; exit 0; fi
cmake --build build -j4 --target xapiand_test_manager_fd_count || exit 0
OUT=$(ctest --test-dir build --output-on-failure -R '^test_manager_fd_count$' 2>&1); RC=$?
echo "$OUT"
[ $RC -ne 0 ] && echo "$OUT" | grep -q "FAIL .*CHECK" && exit 1
exit 0
