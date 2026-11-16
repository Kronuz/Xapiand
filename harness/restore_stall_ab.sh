#!/bin/bash
# The restore-stall A/B, one `quiet strict` hold per mode (each under 10 minutes):
#   abba  base, head, head, base, with the trivial GET before, during and after a RESTORE stream;
#   spin  one spin-probe run of each binary: the loop's turns over 3 ms in the RESTORE phase.
# Prints each arm and the verdict of what OUTDIR holds (restore_stall_verdict.py).
# Usage: harness/restore_stall_ab.sh abba|spin BASE_BIN HEAD_BIN OUTDIR   (after restore_stall_build.sh)
set -u
MODE=$1
B=$2
H=$3
OUT=$4
mkdir -p "$OUT"
P=harness/restore_stall_probe.py
bin_of() {
	if [ "$1" = head ]; then echo "$H"; else echo "$B"; fi
}
case $MODE in
abba)
	i=0
	for arm in base head head base; do
		i=$((i + 1))
		python3 $P --bin "$(bin_of $arm)" --docs 20000 --idle-secs 1 --phases trivial,restore,trivial3 \
			--workdir .scratch/rs-ab --keep "$OUT/ab$i-$arm" --out "$OUT/ab$i-$arm.json" > "$OUT/ab$i-$arm.log" 2>&1 || echo "arm $i ($arm) failed"
	done
	;;
spin)
	for arm in base head; do
		python3 $P --bin "$(bin_of $arm)" --docs 20000 --idle-secs 1 --phases restore --spin 3000 \
			--workdir .scratch/rs-spin --keep "$OUT/spin-$arm" --out "$OUT/spin-$arm.json" > "$OUT/spin-$arm.log" 2>&1 || echo "spin $arm failed"
		python3 harness/restore_stall_stalls.py "$(bin_of $arm)" "$OUT/spin-$arm/node.log" --probe-log "$OUT/spin-$arm.log" \
			--phase restore --top 5 --frames 8 > "$OUT/spin-$arm.stalls.txt" 2>&1
	done
	;;
*)
	echo "mode: abba or spin" >&2
	exit 2
	;;
esac
python3 harness/restore_stall_verdict.py "$OUT"
