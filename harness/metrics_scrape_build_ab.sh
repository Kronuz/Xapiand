#!/bin/bash
# Builds the two arms of harness/metrics_scrape_ab.sh in one build directory at this checkout, with the spin probe
# (harness/metrics_scrape_reactor_lag.patch, inert unless XAPIAND_LAG_SPIN is set) applied to the fetched reactor:
# before = Kronuz/system 563880a (the descriptor poll up to the limit), after = the pinned system. Only system.cc
# and the link differ between the arms. Writes build-scrape/ab/{before,after}. Run under `quiet light`.
set -eu
HERE=$(cd "$(dirname "$0")/.." && pwd); cd "$HERE"
OLD=563880aa1303673dfc8b9bd3cb5ad39bd6f652b8
CFG=(-S . -B build-scrape -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DLTO=OFF -DBUILD_TESTS=OFF)
mkdir -p build-scrape/ab
[ -d .deps/system-before ] || git clone -q https://github.com/Kronuz/system.git .deps/system-before
git -C .deps/system-before checkout -q "$OLD"
cmake "${CFG[@]}" -DFETCHCONTENT_SOURCE_DIR_SYSTEM="$PWD/.deps/system-before" > /dev/null
H=.deps/reactor-src/reactor_lag.h
grep -q spin_probe "$H" || patch -p1 -d "$(dirname "$H")" < harness/metrics_scrape_reactor_lag.patch
cmake --build build-scrape -j4 --target xapiand
cp build-scrape/bin/xapiand build-scrape/ab/before
cmake "${CFG[@]}" -UFETCHCONTENT_SOURCE_DIR_SYSTEM > /dev/null
cmake --build build-scrape -j4 --target xapiand
cp build-scrape/bin/xapiand build-scrape/ab/after
