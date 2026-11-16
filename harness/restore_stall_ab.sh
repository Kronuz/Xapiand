#!/bin/bash
# The restore-stall A/B, in one `quiet strict` hold: ABBA twice (base, head, head, base) with the trivial GET
# before, during and after a RESTORE stream, then one spin-probe run of each binary for the loop's turns over
# 3 ms in the RESTORE phase. Prints each arm and the verdict (restore_stall_verdict.py).
# Usage: harness/restore_stall_ab.sh BASE_BIN HEAD_BIN OUTDIR   (after restore_stall_build.sh)
set -u
B=$1
H=$2
OUT=$3
mkdir -p "$OUT"
P=harness/restore_stall_probe.py
ARGS=(--docs 20000 --idle-secs 1 --phases trivial,restore,trivial3)
i=0
for arm in base head head base base head head base; do
	i=$((i + 1))
	bin=$B
	[ "$arm" = head ] && bin=$H
	python3 $P --bin "$bin" "${ARGS[@]}" --workdir .scratch/rs-ab --keep "$OUT/ab$i-$arm" --out "$OUT/ab$i-$arm.json" > "$OUT/ab$i-$arm.log" 2>&1 || echo "arm $i ($arm) failed"
done
for arm in base head; do
	bin=$B
	[ "$arm" = head ] && bin=$H
	python3 $P --bin "$bin" --docs 20000 --idle-secs 1 --phases restore --spin 3000 --workdir .scratch/rs-spin --keep "$OUT/spin-$arm" --out "$OUT/spin-$arm.json" > "$OUT/spin-$arm.log" 2>&1 || echo "spin $arm failed"
	python3 harness/restore_stall_stalls.py "$bin" "$OUT/spin-$arm/node.log" --probe-log "$OUT/spin-$arm.log" --phase restore --top 5 --frames 8 > "$OUT/spin-$arm.stalls.txt" 2>&1
done
python3 harness/restore_stall_verdict.py "$OUT"
