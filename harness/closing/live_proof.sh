#!/usr/bin/env bash
# The closing harness's live proof on a lease (`lease run ... --setup harness/closing/setup.sh -- harness/closing/live_proof.sh`):
# the offline tests, the Live tests, and a short run of loadtest, stall_load, closing_tenants and closing_mixed against
# Xapiand, Elasticsearch and Quickwit, each step's log and exit code in $OUT. Sizes are those of a smoke run.
set -uo pipefail

D=$HOME/data
. "$HOME/work/harness/closing/pins.env"
H=$HOME/work/harness
OUT=${OUT:?}
W=$D/proof
export CLOSING_DATA=$D
export CLOSING_XAPIAND=$D/xapiand/xapiand CLOSING_ES=$D/elasticsearch-$ES_VERSION/bin/elasticsearch CLOSING_QUICKWIT=$D/quickwit-v$QW_VERSION/quickwit
ulimit -n 524288
rm -rf "$W"
mkdir -p "$W" "$OUT"
steps=$OUT/steps.tsv
: >"$steps"

# step NAME SECONDS CMD...: run with a timeout, log to $OUT/NAME.log, record the exit code
step() {
	local name=$1 secs=$2 rc t0
	shift 2
	t0=$(date +%s)
	timeout -k 10 "$secs" "$@" >"$OUT/$name.log" 2>&1
	rc=$?
	printf '%s\t%s\t%s\n' "$name" "$rc" "$(($(date +%s) - t0))" >>"$steps"
	echo "step $name rc=$rc" >&2
	return 0
}

{
	uname -a
	nproc
	free -m
	lscpu | head -20
	stat -fc %T /sys/fs/cgroup
	cat /sys/class/dmi/id/product_uuid 2>/dev/null
	df -h "$D" | tail -1
} >"$OUT/host.txt" 2>&1
git -C "$HOME/work" rev-parse HEAD >"$OUT/rev.txt" 2>&1 || true
sha256sum "$CLOSING_XAPIAND" >"$OUT/binaries.txt"

step fetch-logs 600 "$H/closing/fetch_corpus.sh" logs --limit 200000
step fetch-writes 600 "$H/closing/fetch_corpus.sh" logs-writes --limit 100000
step fetch-wiki 600 "$H/closing/fetch_corpus.sh" wiki --limit 40000
LOGS=$D/corpora/sample-200000/logs.ndjson
WRITES=$D/corpora/sample-100000/logs-writes.ndjson
WIKI=$D/corpora/sample-40000/wiki.ndjson
Q=$H/closing/queries/logs-smoke.jsonl

step offline 300 python3 -I "$H/test_closing_harness.py"
CLOSING_LIVE=1 CLOSING_LOGS=$LOGS step live 1200 python3 -I "$H/test_closing_harness.py" Live

port() { case $1 in xapiand) echo 18970 ;; es) echo 19270 ;; quickwit) echo 17270 ;; esac; }
bin() { case $1 in xapiand) echo "$CLOSING_XAPIAND" ;; es) echo "$CLOSING_ES" ;; quickwit) echo "$CLOSING_QUICKWIT" ;; esac; }

for sys in xapiand es quickwit; do
	P=$(port "$sys")
	B=$(bin "$sys")
	T=127.0.0.1:$P
	step "loadtest-$sys" 900 python3 -I "$H/loadtest.py" --system "$sys" --bin "$B" --workdir "$W/lt-$sys" --target "$T" \
		--corpus "$LOGS" --schema logs --limit 100000 --clients 4 --batch-docs 1000 --queries "$Q" --latency --runs 3 \
		--warm-secs 3 --duration 0 --restart --drop-caches --assert-settings --cpus 4 --out "$OUT/loadtest-$sys.json"
	step "stall-saturate-$sys" 900 python3 -I "$H/stall_load.py" --system "$sys" --bin "$B" --workdir "$W/sat-$sys" \
		--target "$T" --schema logs --queries "$Q" --corpus "$LOGS" --limit 50000 --class term --saturate \
		--saturate-start 100 --saturate-max 1600 --bisect-steps 1 --step-secs 4 --procs 2 --cpus 4 --out "$OUT/stall-saturate-$sys.json"
	step "stall-mixed-$sys" 900 python3 -I "$H/stall_load.py" --system "$sys" --bin "$B" --workdir "$W/mix-$sys" \
		--target "$T" --schema logs --queries "$Q" --corpus "$LOGS" --limit 50000 --mixed --write-corpus "$WRITES" \
		--write-rate 500 --warmup 5 --duration 20 --procs 2 --cpus 4 --out "$OUT/stall-mixed-$sys.json"
	step "tenants-$sys" 900 python3 -I "$H/wide_search_check.py" tenants "$B" "$W/ten-$sys" "$P" --system "$sys" \
		--corpus "$WIKI" --tenants 200 --per-tenant 20 --shape per-index --shape shared --runs 5 --cold 50 --idle-s 5 \
		--budget-s 120 --cpus 4 --result "$OUT/tenants-$sys.json"
	step "mixed-kill-$sys" 600 python3 -I "$H/engine_mixed_load.py" "$B" "$W/crash-$sys" --system "$sys" --kill-at 20 \
		--secs 40 --target "$T" --cpus 4 --result "$OUT/mixed-kill-$sys.json"
	pkill -f "$D/(xapiand|elasticsearch|quickwit)" 2>/dev/null || true
done

cat "$steps" >&2
