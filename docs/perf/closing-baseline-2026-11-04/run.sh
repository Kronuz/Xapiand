#!/bin/bash
# Xapiand-baseline.sh: the closing benchmark's local baseline, Xapiand on strata against Elasticsearch and Quickwit, on
# the local Linux VM (8 CPUs, 16 GiB). It uses the harness's smoke layout (2 system cores, 4 GiB per arm) at 200,000
# documents, builds C-wiki and C-logs, runs five rounds of every row the harness has (t1, t2, t4a, t4b, t5a, t5b, t6,
# t7), then the verdicts. Results come home to ~/.scratch/xapiand-baseline/ID/; SUMMARY.txt lists the steps and verdicts.
# The harness is branch fix/closing-bench-harness; everything on the VM lives under ~/xbase (its own work and data).
set -uo pipefail

TREE=/Users/kronuz/code/Kronuz/Xapiand-wt/closing-harness
VM=/Users/kronuz/code/Kronuz/strata/tools/vm/linux.sh
QUIET=/Users/kronuz/.local/bin/quiet
ID=$(date -u +%Y%m%dT%H%M%SZ)
RES=/Users/kronuz/.scratch/xapiand-baseline/$ID
LIMIT=200000
DEADLINE_MIN=300
UNITS=B1,B2
for r in 1 2 3 4 5; do for t in t1 t2 t4a t4b t5a t5b t6 t7; do UNITS="$UNITS,$t-r$r"; done; done
mkdir -p "$RES"
log() { echo "$(date '+%F %T') $*" | tee -a "$RES/SUMMARY.txt"; }

log "baseline $ID: harness $(git -C "$TREE" rev-parse --short HEAD), limit $LIMIT, deadline $DEADLINE_MIN min"
[ "$("$VM" status)" = running ] || "$VM" start || { log "VM did not start"; exit 1; }

# Ship the harness's committed tree (no build dirs) to the baseline's own HOME on the VM.
git -C "$TREE" archive --format=tar HEAD |
	"$VM" ssh 'B=$HOME/xbase; mkdir -p "$B"; rm -rf "$B/work"; mkdir -p "$B/work"; tar -xf - -C "$B/work"'
rc=$?; log "sync rc=$rc"; [ "$rc" = 0 ] || exit "$rc"
# free_disk NEED WHAT: stop cleanly when the shared VM has under NEED GiB free.
free_disk() {
	local g
	g=$("$VM" ssh 'df -Pk "$HOME" | awk "NR==2 {print int(\$4/1048576)}"' </dev/null | tr -d '\r')
	log "VM free disk ${g} GiB before $2 (needs $1)"
	[ "${g:-0}" -ge "$1" ] || { log "under $1 GiB free on the shared VM before $2; stopping (free space there first)"; exit 3; }
}
# C-wiki's 2.5 GB download is already on the VM (strata's bench corpus): link it, never download it twice.
"$VM" ssh 'B=$HOME/xbase; W=$HOME/bench/search-benchmark-game/wiki-articles.json.bz2; mkdir -p "$B/data/dl"
	[ -e "$B/data/dl/wiki-articles.json.bz2" ] || { [ -f "$W" ] && ln "$W" "$B/data/dl/wiki-articles.json.bz2"; }; true' </dev/null
# Setup needs about 6 GiB and the rounds about 8 GiB at 200,000 documents; the checks ask for several times that.
free_disk 40 setup

# Setup: packages, the pinned Elasticsearch and Quickwit, ab_compare, a Release build of Xapiand (idempotent).
"$QUIET" light xbase-setup 90 "Xapiand baseline: VM setup" -- \
	"$VM" ssh 'B=$HOME/xbase; HOME=$B bash "$B/work/harness/closing/setup.sh"' </dev/null >"$RES/setup.log" 2>&1
rc=$?; log "setup rc=$rc"; [ "$rc" = 0 ] || exit "$rc"

free_disk 30 "the rounds"

# The rounds, alone on the host: every timing in them is published-grade only under an exclusive hold.
"$QUIET" exclusive xbase-bench $((DEADLINE_MIN + 40)) "Xapiand baseline: 5 rounds on the VM" -- \
	"$VM" ssh "B=\$HOME/xbase; mkdir -p \$B/out/$ID
		HOME=\$B OUT=\$B/out/$ID CLOSING_LIMIT=$LIMIT DEADLINE_MIN=$DEADLINE_MIN \
			UNITS=$UNITS bash \$B/work/harness/closing/bench_smoke.sh" </dev/null >"$RES/bench.log" 2>&1
rc=$?; log "bench rc=$rc"

# The verdicts (D11) over whatever rounds completed, then everything home.
"$VM" ssh "B=\$HOME/xbase; cd \$B/work && HOME=\$B AB_COMPARE=\$B/data/tools/ab_compare.py \
	python3 harness/closing_report.py report \$B/out/$ID/results >\$B/out/$ID/report.txt 2>&1; echo report rc=\$?" \
	</dev/null >>"$RES/SUMMARY.txt" 2>&1
"$VM" ssh "tar -C \$HOME/xbase/out/$ID -cf - ." </dev/null | tar -xf - -C "$RES"
log "results home: $(du -sh "$RES" | cut -f1)"
[ -f "$RES/steps.tsv" ] && { echo "--- steps (name rc seconds)"; cat "$RES/steps.tsv"; } >>"$RES/SUMMARY.txt"
[ -f "$RES/report.txt" ] && { echo "--- report"; cat "$RES/report.txt"; } >>"$RES/SUMMARY.txt"
exit "$rc"
