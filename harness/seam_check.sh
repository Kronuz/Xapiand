#!/usr/bin/env bash
#
# seam_check.sh [--update]  -- gate T2: Xapian stays behind the engine seam.
#
# Counts the lines naming `Xapian::` or including a Xapian header in every source outside
# src/xapian and src/engine/xapian, and compares each file with harness/seam_allowlist.txt
# ("<count> <path>" per line). A file that is not listed, or whose count grew, fails. The
# list only shrinks: --update rewrites it from the tree. The end state (X4c) is an allowlist
# of the remote protocol and replication files only.
set -u
cd "$(dirname "$0")/.."
LIST=harness/seam_allowlist.txt

counts() {
	git ls-files src | grep -E '\.(cc|h|hh|cpp|c)$' | grep -v -e '^src/xapian/' -e '^src/engine/xapian/' |
	while read -r f; do
		n=$(grep -cE 'Xapian::|#include "xapian\.h"|#include "xapian/' "$f")
		[ "$n" -gt 0 ] && echo "$n $f"
	done | sort -k2
}

actual=$(counts)
if [ "${1:-}" = "--update" ]; then
	printf '%s\n' "$actual" > "$LIST"
	echo "seam_check: wrote $LIST ($(echo "$actual" | wc -l | tr -d ' ') files)"
	exit 0
fi

fail=0
while read -r n f; do
	[ -z "$f" ] && continue
	allowed=$(awk -v f="$f" '$2 == f { print $1 }' "$LIST")
	if [ -z "$allowed" ]; then
		echo "seam_check: $f names Xapian ($n lines) and is not in the allowlist"; fail=1
	elif [ "$n" -gt "$allowed" ]; then
		echo "seam_check: $f grew from $allowed to $n lines"; fail=1
	elif [ "$n" -lt "$allowed" ]; then
		echo "seam_check: $f shrank from $allowed to $n lines: run harness/seam_check.sh --update"; fail=1
	fi
done <<< "$actual"
while read -r n f; do
	[ -z "$f" ] && continue
	echo "$actual" | awk -v f="$f" '$2 == f { found = 1 } END { exit !found }' ||
		{ echo "seam_check: $f no longer names Xapian: run harness/seam_check.sh --update"; fail=1; }
done < "$LIST"

files=$(echo "$actual" | grep -c .)
lines=$(echo "$actual" | awk '{ s += $1 } END { print s + 0 }')
echo "seam_check: files=$files lines=$lines"
exit $fail
