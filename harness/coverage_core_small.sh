#!/usr/bin/env bash
# coverage_core_small.sh: runs the four core-small coverage tests (cast, datetime_msgpack, endpoint, query_dsl) with
# coverage_one.sh over one build directory, so the instrumented objects compile once. Env: REPEAT (runs per test).
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
export BUILD=${BUILD:-$ROOT/build-cov-core}
rc=0
for row in cast:src/cast.cc datetime:src/datetime_msgpack.cc endpoint:src/endpoint.cc query_dsl:src/query_dsl.cc; do
	echo "=== core_${row%%:*}"
	"$ROOT/harness/coverage_one.sh" "core_${row%%:*}" "${row#*:}" || rc=$?
done
exit $rc
