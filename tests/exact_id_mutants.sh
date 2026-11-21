#!/usr/bin/env bash
# exact_id_mutants.sh MUTANT TEST: applies one mutant of docs/design/exact-id-keys.md's mechanism to this checkout,
# builds xapiand_test_node and runs its case TEST. Exits 0 only when TEST fails, the mutant killed; 1 when it passes
# or the mutant does not apply or build.
set -u
mutant=$1 test=$2

mutate() {   # mutate FILE OLD NEW: replaces the one OLD line in FILE, or fails
	python3 - "$1" "$2" "$3" <<'PY' || { echo "MUTATION NOT APPLIED: $mutant"; exit 1; }
import sys
path, old, new = sys.argv[1:]
text = open(path).read()
if text.count(old) != 1:
    sys.exit(1)
open(path, "w").write(text.replace(old, new))
PY
}

case $mutant in
	query-double)   # a term search on a numeric _id encodes the key as a double: inexact above 2^53
		mutate src/query_dsl.cc \
			"	auto serialized_term = numeric_id ? Serialize::id(field_spc, obj) : Serialize::MsgPack(field_spc, obj);" \
			'	auto serialized_term = numeric_id ? Serialize::id(field_spc, MsgPack(obj.as_f64())) : Serialize::MsgPack(field_spc, obj);' ;;
	cap-2-32)   # the counter's bound goes back to 2^32 - 1
		mutate src/database/handler.h \
			'inline constexpr std::uint64_t MAX_NUMERIC_ID = (std::uint64_t{1} << 63) - 1;' \
			'inline constexpr std::uint64_t MAX_NUMERIC_ID = (std::uint64_t{1} << 32) - 1;' ;;
	index-commit)   # the test's merge_all commits the index itself, behind the engine's revision
		mutate tests/test_node.cc $'\tengine::sync_wait(use_index(pin, WRITE_QUIET).commit());\n' ''
		mutate tests/test_node.cc $'\tauto& index = A::index(pin.entry());\n\tstd::vector<std::uint64_t> ids;' \
			$'\tauto& index = A::index(pin.entry());\n\tindex.commit();\n\tstd::vector<std::uint64_t> ids;' ;;
	*) echo "unknown mutant $mutant"; exit 1 ;;
esac

cmake -G Ninja -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF >cmake.log 2>&1 || { tail -30 cmake.log; exit 1; }
cmake --build build -j4 --target xapiand_test_node >build.log 2>&1 || { grep -E "error:" build.log | head -20; tail -10 build.log; exit 1; }
bin=$(find build -name xapiand_test_node -type f -perm -u+x | head -1)
"$bin" "$test" >out.txt 2>&1
rc=$?
tail -40 out.txt
if [ $rc -ne 0 ]; then
	echo "KILLED: $mutant fails $test"
	exit 0
fi
echo "SURVIVED: $mutant passes $test (exit $rc)"
exit 1
