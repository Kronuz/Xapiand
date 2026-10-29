#!/usr/bin/env bash
# thread_census.sh PID [--allow PREFIX,PREFIX,...] [--json]
#
# Phase 7c T1: the threads of a running process, counted by name prefix (trailing digits and
# separators stripped). Linux reads /proc/PID/task/*/comm; macOS counts the threads with
# `ps -M` and takes their names from a one second `sample` (ps cannot show names there).
# With --allow, a prefix outside the list is an offender and the exit status is 1: the check
# T1 runs against section 2's thread list. Prints "<count> <prefix>" lines and a total.
set -eu
[ $# -ge 1 ] || { sed -n 2,9p "$0"; exit 2; }
PID=$1; shift
ALLOW=""; JSON=0
while [ $# -gt 0 ]; do
	case $1 in
		--allow) ALLOW=$2; shift 2 ;;
		--json) JSON=1; shift ;;
		*) echo "thread_census: unknown argument $1" >&2; exit 2 ;;
	esac
done
kill -0 "$PID" 2>/dev/null || { echo "thread_census: no process $PID" >&2; exit 2; }

SCRATCH=$(cd "$(dirname "$0")/.." && pwd)/.scratch
mkdir -p "$SCRATCH"
WORK=$(mktemp -d "$SCRATCH/thread_census.XXXXXX")
trap 'rm -rf "$WORK"' EXIT

names() {
	if [ -d "/proc/$PID/task" ]; then
		for f in /proc/"$PID"/task/*/comm; do cat "$f" 2>/dev/null || true; done
	else
		sample "$PID" 1 -mayDie -file "$WORK/sample.txt" >/dev/null 2>&1 || { echo "thread_census: sample failed" >&2; exit 2; }
		# "    1000 Thread_12345: name" or "Thread_12345   DispatchQueue_1: com.apple.main-thread (serial)"
		awk '/^[[:space:]]+[0-9]+ Thread_[0-9]+/ {
			line = $0; sub(/^[[:space:]]+[0-9]+ Thread_[0-9]+[[:space:]]*:?[[:space:]]*/, "", line)
			if (line ~ /com.apple.main-thread/) line = "main"
			sub(/^DispatchQueue_[0-9]+:[[:space:]]*/, "", line); sub(/ \(serial\).*$/, "", line)
			if (line == "") line = "unnamed"
			print line }' "$WORK/sample.txt"
	fi
}

names | sed -E 's/[-_.]*[0-9]*[-_.]*$//' | sort | uniq -c | sort -k1,1nr -k2 | awk '{ c = $1; $1 = ""; sub(/^ /, ""); print c "\t" $0 }' > "$WORK/census.tsv"
TOTAL=$(awk -F'\t' '{ s += $1 } END { print s + 0 }' "$WORK/census.tsv")
PS_TOTAL=""
if [ ! -d "/proc/$PID/task" ]; then PS_TOTAL=$(ps -M -p "$PID" | awk 'NR > 1 { n++ } END { print n + 0 }'); fi

OFFENDERS=""
if [ -n "$ALLOW" ]; then
	OFFENDERS=$(awk -F'\t' -v allow="$ALLOW" 'BEGIN { n = split(allow, a, ","); for (i = 1; i <= n; i++) ok[a[i]] = 1 } !($2 in ok) { print $2 }' "$WORK/census.tsv" | paste -sd, -)
fi

if [ "$JSON" = 1 ]; then
	awk -F'\t' -v total="$TOTAL" -v pst="${PS_TOTAL:-null}" -v off="$OFFENDERS" 'BEGIN { printf "{\"threads\":{" }
		{ gsub(/"/, "\\\"", $2); printf "%s\"%s\":%d", (NR > 1 ? "," : ""), $2, $1 }
		END { printf "},\"total\":%d,\"ps_total\":%s,\"offenders\":\"%s\"}\n", total, pst, off }' "$WORK/census.tsv"
else
	awk -F'\t' '{ printf "%6d  %s\n", $1, $2 }' "$WORK/census.tsv"
	echo "total $TOTAL${PS_TOTAL:+ (ps -M counts $PS_TOTAL)}"
	[ -z "$OFFENDERS" ] || echo "thread_census: not in --allow: $OFFENDERS"
fi
[ -z "$OFFENDERS" ]
