#!/bin/sh
# logger burst A/B at f61d8df vs 6e7ab9e on Xapiand's workpool pin: 5 rounds, then A/A.
set -eu
out=$PWD/.scratch/ab; rm -rf "$out"; mkdir -p "$out"
for n in OLD NEW; do
  eval src=\$JOB_REF_LOGGER_$n
  cmake -S "$src" -B "$out/build-$n" -DCMAKE_BUILD_TYPE=Release -DLOGGER_BUILD_BENCH=ON \
    -DFETCHCONTENT_SOURCE_DIR_WORKPOOL="$JOB_REF_WORKPOOL" >/dev/null
  cmake --build "$out/build-$n" --target logger_bench -j 4 >/dev/null
  cp "$out/build-$n/logger_bench" "$out/$n"
done
py=$JOB_REF_LOGGER_NEW/bench/logger_ab.py
python3 "$py" --before "$out/OLD" --after "$out/NEW" --output "$out/ab" --rounds 5
python3 "$py" --before "$out/OLD" --after "$out/OLD" --output "$out/aa" --rounds 5
