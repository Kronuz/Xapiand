#!/usr/bin/env bash
# grid.sh OUTDIR ROUNDS ARM... : per round, xapian (binary A) then each strata arm (arm name = binary suffix, a trailing digit-free "A2" reuses A).
cd "$(dirname "$0")/.."
OUT=$1; ROUNDS=$2; shift 2
mkdir -p "$OUT"
for r in $(seq 1 "$ROUNDS"); do
  s=$(date +%s)
  XAPIAND_ENGINE=xapian harness/profile_bulk.sh .scratch/bin/xapiand.A "$OUT" --label xapian_$r --port 18990 >/dev/null 2>&1 || echo "FAILED xapian $r"
  for arm in "$@"; do
    bin=${arm%%:*}; label=${arm##*:}
    XAPIAND_ENGINE=strata harness/profile_bulk.sh .scratch/bin/xapiand.$bin "$OUT" --label ${label}_$r --port 18990 >/dev/null 2>&1 || echo "FAILED $label $r"
  done
  echo "round $r $(( $(date +%s) - s ))s"
done
