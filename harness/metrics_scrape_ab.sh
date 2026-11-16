#!/bin/bash
# A/B of one GET /:metrics loop turn, ABAB over ROUNDS rounds, one process per arm.
# Usage: metrics_scrape_ab.sh BEFORE_BIN AFTER_BIN [ROUNDS]     (run under `quiet strict`)
set -u
B=$1; A=$2; R=${3:-3}; HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE" || exit 1
for i in $(seq 1 "$R"); do
	for arm in before after after before; do
		bin=$B; [ "$arm" = after ] && bin=$A
		echo -n "round $i $arm: "
		python3 harness/metrics_scrape_probe.py --bin "$bin" --spin 3000 --limit 1048576 -n 40 || exit 1
	done
done
