#!/usr/bin/env bash
# sortable_xlang_mutants.sh MUTANT SORTABLE_SOURCE_DIR: applies one mutant to the client's codec (xapiand/sortable.py) in this checkout,
# builds sortable_cli and runs tests/sortable_xlang.py. Exits 0 only when the check fails (the mutant killed); 1 when
# it passes or the mutant does not apply or build.
set -u
mutant=$1 sortable=$2
codec=contrib/python/xapiand-py/xapiand/sortable.py

mutate() {   # mutate OLD NEW: replaces the one OLD text in the codec, or fails
	python3 - "$codec" "$1" "$2" <<'PY' || { echo "MUTATION NOT APPLIED: $mutant"; exit 1; }
import sys
path, old, new = sys.argv[1:]
text = open(path).read()
if text.count(old) != 1:
    sys.exit(1)
open(path, "w").write(text.replace(old, new))
PY
}

case $mutant in
	rule-7)   # decode accepts a tail ending in 00 (rule 8 is implied by rule 9, so its mutant is equivalent)
		mutate ' or (tail and tail[-1] == 0)' '' ;;
	window-62)   # encode's direct tags end at 2^62, so 2^63..2^64 take the escape
		mutate 'if -60 <= exponent <= 63:' 'if -60 <= exponent <= 62:' ;;
	*) echo "unknown mutant $mutant"; exit 1 ;;
esac

mkdir -p .scratch/xlang
c++ -std=c++20 -O2 -I"$sortable" -o .scratch/xlang/sortable_cli "$sortable/tools/cli.cc" || exit 1
python3 -I tests/sortable_xlang.py "$sortable" .scratch/xlang/sortable_cli
rc=$?
if [ $rc -ne 0 ]; then
	echo "KILLED: $mutant fails sortable_xlang"
	exit 0
fi
echo "SURVIVED: $mutant passes sortable_xlang"
exit 1
