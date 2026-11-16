#!/bin/bash
# Second strict hold: the fd poll cost, the scrape cost, a spin run with a quiet stretch, and the slice experiment
# against master in ABBA. Usage: restore_stall_run2.sh MASTER_BIN SLICE_BIN OUTDIR   (under `quiet strict`)
set -u
M=$1; S=$2; OUT=$3; HERE=$(cd "$(dirname "$0")/.." && pwd)
mkdir -p "$OUT"; cd "$HERE" || exit 1
P=harness/restore_stall_probe.py
ARGS="--docs 20000 --idle-secs 1 --phases trivial,restore,trivial3"
.scratch/bin/fd_poll_cost > "$OUT/fd_poll_cost.txt" 2>&1
python3 harness/restore_stall_scrape.py --bin "$M" --limits 1048576,65536,10240,1024 > "$OUT/scrape.txt" 2>&1
python3 $P --bin "$M" --docs 20000 --spin 3000 --quiet-secs 60 --idle-secs 5 --workdir .scratch/rs-spin2 --keep "$OUT/spin-master" --out "$OUT/spin-master.json" > "$OUT/spin-master.log" 2>&1
i=0
for arm in master slice slice master; do
  i=$((i+1)); bin=$M; [ $arm = slice ] && bin=$S
  python3 $P --bin "$bin" $ARGS --workdir .scratch/rs-ab --keep "$OUT/ab$i-$arm" --out "$OUT/ab$i-$arm.json" > "$OUT/ab$i-$arm.log" 2>&1
done
python3 $P --bin "$S" --docs 20000 --spin 3000 --idle-secs 5 --workdir .scratch/rs-spin3 --keep "$OUT/spin-slice" --out "$OUT/spin-slice.json" > "$OUT/spin-slice.log" 2>&1
echo done > "$OUT/DONE"
