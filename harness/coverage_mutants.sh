#!/usr/bin/env bash
# coverage_mutants.sh MUTANT: applies one mutant to the code a coverage test (tests/test_coverage_*.cc) covers, builds that
# test and runs it. Exits with the test's status, so a `campaign job submit --fails-without` job passes when the mutant is
# killed; 126 when the mutant does not apply or build, which never counts as killed. The mutated source is restored on exit.
# Env: COVERAGE_DEPS (the FetchContent directory; default .deps), JOBS (default 4).
set -u
mutant=$1
ROOT=$(cd "$(dirname "$0")/.." && pwd); cd "$ROOT" || exit 126
trap 'git checkout -- src' EXIT

mutate() {   # mutate FILE OLD NEW: replaces the one OLD text in FILE, or fails
	python3 - "$1" "$2" "$3" <<'PY' || { echo "MUTATION NOT APPLIED: $mutant"; exit 126; }
import sys
path, old, new = sys.argv[1:]
text = open(path).read()
if text.count(old) != 1:
    sys.exit(1)
open(path, "w").write(text.replace(old, new))
PY
}

case $mutant in
	geo-km)        # the "km" suffix of a distance is a tenth of a kilometer
		target=geospatial
		mutate src/geospatial/geospatial.cc '		mul = 1000.0;' '		mul = 100.0;' ;;
	geo-mile)      # a mile is not 1609.344 m
		target=geospatial
		mutate src/geospatial/geospatial.cc '		mul = 1609.344;' '		mul = 1600.0;' ;;
	y2j-hex)       # 0x integers are read in base 8
		target=y2j
		mutate src/y2j/y2j.cc '                base = 16;' '                base = 8;' ;;
	y2j-depth)     # the nesting limit is one level too generous
		target=y2j
		mutate src/y2j/y2j.cc 'if (++depth > maxDepth && maxDepth) {' 'if (++depth > maxDepth + 1 && maxDepth) {' ;;
	agg-depth)     # the depth limit refuses the last allowed level
		target=aggregations
		mutate src/aggregations/aggregations.cc '	if (aggs_depth(aggs) > AGGS_MAX_DEPTH) {' '	if (aggs_depth(aggs) >= AGGS_MAX_DEPTH) {' ;;
	agg-reserved)  # a reserved but unimplemented kind is no longer refused
		target=aggregations
		mutate src/aggregations/aggregations.cc '							THROW(AggregationError, "Aggregation {} is reserved but not implemented yet", repr(sub_agg_type));' '							break;' ;;
	*) echo "unknown mutant $mutant" >&2; exit 126 ;;
esac

DEPS=${COVERAGE_DEPS:-$ROOT/.deps}
cmake -G Ninja -S . -B build-mutant -DFETCHCONTENT_BASE_DIR="$DEPS" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DLTO=OFF \
	-DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache >build-mutant.log 2>&1 &&
	cmake --build build-mutant -j"${JOBS:-4}" --target "xapiand_test_coverage_$target" >>build-mutant.log 2>&1 ||
	{ tail -20 build-mutant.log; echo "MUTANT DID NOT BUILD: $mutant"; exit 126; }
"build-mutant/bin/xapiand_test_coverage_$target"
