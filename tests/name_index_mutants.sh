#!/usr/bin/env bash
# name_index_mutants.sh MUTANT TARGET TEST: applies one mutant of docs/design/name-index.md's mechanism to this
# checkout, builds the test binary TARGET (xapiand_test_index_pattern, xapiand_test_node) and runs its case TEST.
# Exits with TEST's status, so a `campaign job submit --fails-without` job passes when the mutant is killed; 126
# when the mutant does not apply or build, which never counts as killed. The mutated sources are restored on exit.
set -u
mutant=$1 target=$2 test=$3
trap 'git checkout -- src/database/name_index.cc src/database/pool.cc src/server/search_views.cc src/index_pattern.cc' EXIT

mutate() {   # mutate FILE OLD NEW: replaces the one OLD text in FILE, or fails
	python3 - "$1" "$2" "$3" <<'EOF' || { echo "MUTATION NOT APPLIED: $mutant"; exit 126; }
import sys
path, old, new = sys.argv[1:]
text = open(path).read()
if text.count(old) != 1:
    sys.exit(1)
open(path, "w").write(text.replace(old, new))
EOF
}

case $mutant in
	no-parents)    # a directory adds only its own name: a shard or a nested `strata` does not list its parent
		mutate src/database/name_index.cc \
			'if ((next == "strata" || next.starts_with(".__")) && listable(' \
			'if (n == segments.size() && listable(' ;;
	keep-slash)    # the literal prefix keeps a `/` before `**`, so `a/**` misses `a`
		mutate src/database/name_index.cc \
			'	if (prefix.ends_with('"'"'/'"'"')) {' \
			'	if (false) {' ;;
	drop-delta)    # the build forgets what resolve added while it ran
		mutate src/database/name_index.cc \
			'	for (auto it = delta_.begin(); it != delta_.end(); ) {' \
			'	delta_.clear();
	for (auto it = delta_.begin(); it != delta_.end(); ) {' ;;
	lose-merge)    # the delta is dropped instead of merged into the base
		mutate src/database/name_index.cc \
			'		merge_delta();
	}' \
			'		delta_.clear();
	}' ;;
	no-seek)       # a subtree no pattern can match below is read name by name
		mutate src/database/name_index.cc \
			'					if (!live(level)) {' \
			'					if (false) {' ;;
	no-range)      # every pattern scans all the names from the first
		mutate src/database/name_index.cc \
			'			prefixes.push_back(literal_prefix(pattern));' \
			'			prefixes.push_back(std::string_view());' ;;
	top-build)     # the build lists the top level only
		mutate src/database/name_index.cc \
			'	const std::string all = "**";' \
			'	const std::string all = "*";' ;;
	no-index)      # the search walks whatever the names hold
		mutate src/server/search_views.cc \
			'		if (indexes.complete()) {' \
			'		if (false) {' ;;
	no-hook)       # the catalog's resolve adds no names
		mutate src/database/pool.cc \
			'		names_.opened(path);' \
			'		(void)0;' ;;
	no-closed)     # a dead name's subtree is read name by name, each counted: more visits than the walk's entries
		mutate src/database/name_index.cc \
			'					closed.push_back(name);' \
			'					(void)0;' ;;
	skip-unreadable)   # the build takes a directory it cannot open for an empty one
		mutate src/index_pattern.cc \
			'	if (unreadable == Unreadable::fail && error != ENOENT && error != ENOTDIR) {' \
			'	if (false) {' ;;
	*) echo "unknown mutant $mutant"; exit 126 ;;
esac

cmake -G Ninja -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF >cmake.log 2>&1 || { tail -30 cmake.log; exit 126; }
cmake --build build -j4 --target "$target" >build.log 2>&1 || { grep -E "error:" build.log | head -20; tail -10 build.log; exit 126; }
bin=$(find build -name "$target" -type f -perm -u+x | head -1)
"$bin" "$test" >out.txt 2>&1
rc=$?
tail -40 out.txt
if [ $rc -ne 0 ]; then
	echo "KILLED: $mutant fails $test (exit $rc)"
	[ $rc -eq 124 ] || [ $rc -eq 126 ] || [ $rc -eq 127 ] && exit 1
	exit $rc
fi
echo "SURVIVED: $mutant passes $test"
exit 0
