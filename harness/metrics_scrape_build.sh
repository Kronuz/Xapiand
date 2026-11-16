#!/bin/bash
# Builds the xapiand binary for harness/metrics_scrape_probe.py at this checkout, with the spin probe
# (harness/metrics_scrape_reactor_lag.patch, inert unless XAPIAND_LAG_SPIN is set) applied to the fetched reactor.
# Usage: metrics_scrape_build.sh OUT_BINARY     (run under `quiet light`)
set -eu
OUT=$1; HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
cmake -S . -B build-scrape -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF -DBUILD_TESTS=OFF > /dev/null
H=$(ls .deps/reactor-src/reactor_lag.h)
grep -q spin_probe "$H" || patch -p1 -d "$(dirname "$H")" < harness/metrics_scrape_reactor_lag.patch
cmake --build build-scrape -j4 --target xapiand
mkdir -p "$(dirname "$OUT")"
cp build-scrape/bin/xapiand "$OUT"
