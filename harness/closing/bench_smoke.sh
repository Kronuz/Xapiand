#!/usr/bin/env bash
# closing_bench.sh on a small machine (`lease run ... -- harness/closing/bench_smoke.sh`): 20,000-document samples, the
# builds of C-wiki and C-logs, then rounds of every row, then the report. Exercises the units, the arms' rotation, the
# anchors, the deadline and the verdicts; the numbers are smoke values.
set -uo pipefail

D=$HOME/data
H=$HOME/work/harness
OUT=${OUT:?}
export CLOSING_DATA=$D OUT
. "$H/closing/pins.env"
export CLOSING_XAPIAND=$D/xapiand/xapiand CLOSING_ES=$D/elasticsearch-$ES_VERSION/bin/elasticsearch CLOSING_QUICKWIT=$D/quickwit-v$QW_VERSION/quickwit
export AB_COMPARE=$D/tools/ab_compare.py
ulimit -n 524288
mkdir -p "$OUT"
steps=$OUT/steps.tsv
: >"$steps"

step() {
	local name=$1 secs=$2 rc t0
	shift 2
	t0=$(date +%s)
	timeout -k 10 "$secs" "$@" >"$OUT/$name.log" 2>&1
	rc=$?
	printf '%s\t%s\t%s\n' "$name" "$rc" "$(($(date +%s) - t0))" >>"$steps"
	echo "step $name rc=$rc" >&2
}

step fetch-logs 300 "$H/closing/fetch_corpus.sh" logs --limit 20000
step fetch-writes 300 "$H/closing/fetch_corpus.sh" logs-writes --limit 20000
step fetch-wiki 300 "$H/closing/fetch_corpus.sh" wiki --limit 20000
step fetch-wiki-queries 60 "$H/closing/fetch_corpus.sh" wiki-queries
step plan 60 bash "$H/closing_bench.sh" --plan --approved 250
step smoke 2000 bash "$H/closing_bench.sh" --smoke --deadline-min 30 \
	--units B1,B2,t1-r1,t1-r2,t1-r3,t4a-r1,t4b-r1,t5b-r1,t6-r1,t7-r1
step anchors 60 python3 "$H/closing_report.py" anchors "$OUT/results"
step report 120 python3 "$H/closing_report.py" report "$OUT/results" --out "$OUT/report.csv"
cat "$steps" >&2
