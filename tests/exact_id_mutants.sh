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
	old-codec)   # numeric id keys go back to the value codec, exact only below 2^32
		mutate src/serialise.cc '					return sortable::encode(value.i64());' '					return integer(value.i64());'
		mutate src/serialise.cc '					return sortable::encode(value.u64());' '					return positive(value.u64());'
		mutate src/serialise.cc '					return sortable::encode(value.f64());' '					return floating(value.f64());'
		mutate src/serialise.cc '	return sortable::encode(document_id);' '	return positive(document_id);'
		mutate src/serialise.cc '	const auto number = sortable::decode(serialised_id);' '	const auto number = sortable_unserialise(serialised_id);'
		mutate src/serialise.cc '	return number ? number->to_uint64() : std::nullopt;' \
			'	return number >= 0 && number < 0x1p64 && number == static_cast<long double>(static_cast<std::uint64_t>(number)) ? std::optional<std::uint64_t>(static_cast<std::uint64_t>(number)) : std::nullopt;' ;;
	query-old-codec)   # a term search on a numeric _id builds its term with the value codec
		mutate src/query_dsl.cc \
			"	const bool numeric_id = field_spc.prefix() == DOCUMENT_ID_TERM_PREFIX && field_spc.get_ctype() == 'N';" \
			'	const bool numeric_id = false;' ;;
	cap-2-32)   # the counter's bound goes back to 2^32 - 1
		mutate src/database/handler.h \
			'inline constexpr std::uint64_t MAX_NUMERIC_ID = (std::uint64_t{1} << 63) - 1;' \
			'inline constexpr std::uint64_t MAX_NUMERIC_ID = (std::uint64_t{1} << 32) - 1;' ;;
	*) echo "unknown mutant $mutant"; exit 1 ;;
esac

cmake -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF >cmake.log 2>&1 || { tail -30 cmake.log; exit 1; }
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
