#!/usr/bin/env bash
# one_id_mutants.sh MUTANT TARGET TEST: applies one mutant of docs/design/one-id.md's mechanism to this checkout,
# builds the test binary TARGET (xapiand_test_node, xapiand_test_pool) and runs its case TEST. Exits 0 only when
# TEST fails, the mutant killed; 1 when it passes or the mutant does not apply or build.
set -u
mutant=$1 target=$2 test=$3

mutate() {   # mutate FILE OLD NEW: replaces the one OLD line in FILE, or fails
	python3 - "$1" "$2" "$3" <<'EOF' || { echo "MUTATION NOT APPLIED: $mutant"; exit 1; }
import sys
path, old, new = sys.argv[1:]
text = open(path).read()
if text.count(old) != 1:
    sys.exit(1)
open(path, "w").write(text.replace(old, new))
EOF
}

case $mutant in
	named-docid)   # an explicit numeric id takes the docid it names, as new_did did
		mutate src/database/shard.cc \
			'		info = co_await eng.replace_by_key(key, doc, args);' \
			'		info = co_await (issued == 0 && numeric_id(key) != 0 ? eng.replace(static_cast<engine::docid>(numeric_id(key)), doc, args) : eng.replace_by_key(key, doc, args));' ;;
	no-persist)    # the commit does not persist the counter
		mutate src/database/shard.cc \
			'		co_await eng.set_metadata(std::string(ID_HIGH_METADATA), MsgPack(id_high_).serialise());  // durable with its documents' \
			'		(void)0;' ;;
	no-advance)    # an explicit or restored numeric id does not advance the counter
		mutate src/database/shard.cc \
			'		raise_id_high(numeric_id(key));  // an explicit numeric id advances the counter past it' \
			'		(void)0;' ;;
	no-replay)     # the log's replay does not advance the counter
		mutate src/database/shard.cc \
			'		entry_.raise_id_high(id);  // a write the last commit missed advances the counter again' \
			'		(void)id;' ;;
	no-stride)     # a shard issues the next id whatever bucket it is in
		mutate src/database/handler.cc \
			'		if (bucket_shard(static_cast<std::size_t>((id - 1) % ID_BUCKETS), n_shards) == shard_num) {' \
			'		if (n_shards != 0 || shard_num != 0) {' ;;
	hash-route)    # a numeric id routes by its key's hash, like any other id
		mutate src/database/handler.cc \
			'	if (const auto id = numeric_id(key); id != 0) {' \
			'	if (const auto id = numeric_id(key); id != 0 && key.empty()) {' ;;
	lookup-fan-out)   # a lookup by id reads every shard
		mutate src/database/handler.cc \
			'	const size_t first = id_shard(term, n_shards), last = first + 1;' \
			'	const size_t first = 0, last = n_shards;' ;;
	bulk-fan-out)   # a RESTORE's placement asks every shard for every id it has not found
		mutate src/database/handler.cc \
			'					if (found[k].first == 0 && !failed[k] && !serial[k] && id_shard(terms[k], n_shards) == shard) {' \
			'					if (found[k].first == 0 && !failed[k] && !serial[k]) {' ;;
	*) echo "unknown mutant $mutant"; exit 1 ;;
esac

cmake -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF >cmake.log 2>&1 || { tail -30 cmake.log; exit 1; }
cmake --build build -j4 --target "$target" >build.log 2>&1 || { grep -E "error:" build.log | head -20; tail -10 build.log; exit 1; }
bin=$(find build -name "$target" -type f -perm -u+x | head -1)
"$bin" "$test" >out.txt 2>&1
rc=$?
tail -40 out.txt
if [ $rc -ne 0 ]; then
	echo "KILLED: $mutant fails $test"
	exit 0
fi
echo "SURVIVED: $mutant passes $test (exit $rc)"
exit 1
