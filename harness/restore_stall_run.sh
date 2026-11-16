#!/bin/bash
# One strict hold's worth of restore-stall arms: the LagProbe floor, a clean A, the SPIN/STALL run, the sampled
# run and a clean A again. Usage: restore_stall_run.sh BIN OUTDIR   (run under `quiet strict`)
set -u
BIN=$1; OUT=$2; HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$OUT"; cd "$HERE" || exit 1
P=harness/restore_stall_probe.py
bash harness/restore_stall_floor.sh > "$OUT/floor.txt" 2>&1
python3 $P --bin "$BIN" --docs 20000 --workdir .scratch/rs-a1 --keep "$OUT/a1" --out "$OUT/a1.json" > "$OUT/a1.log" 2>&1
python3 $P --bin "$BIN" --docs 20000 --spin 3000 --workdir .scratch/rs-spin --keep "$OUT/spin" --out "$OUT/spin.json" > "$OUT/spin.log" 2>&1
python3 $P --bin "$BIN" --docs 20000 --sample --phases restore --idle-secs 1 --workdir .scratch/rs-sample --keep "$OUT/sample" --out "$OUT/sample.json" > "$OUT/sample.log" 2>&1
python3 $P --bin "$BIN" --docs 20000 --workdir .scratch/rs-a2 --keep "$OUT/a2" --out "$OUT/a2.json" > "$OUT/a2.log" 2>&1
echo done > "$OUT/DONE"
