#!/bin/sh
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
# SPDX-License-Identifier: MIT
# Build one cache experiment; the cache can be shared by two fresh checkouts.
set -eu
if [ "$#" -ne 5 ]; then
 echo 'usage: ccache_build.sh SOURCE BUILD EVIDENCE DEPENDENCY_SEED release|asan|tsan|tests' >&2
 exit 2
fi
src=$1
build=$2
evidence=$3
seed=$4
kind=$5
mkdir -p "$evidence" "$src/.scratch/ccache-deps"
export CCACHE_DIR="$evidence/cache"
export CCACHE_MAXSIZE=0
export CCACHE_COMPRESS=true
export CCACHE_NOFILECLONE=true
export CCACHE_BASEDIR="${CCACHE_BASEDIR:-$src}"
export CCACHE_NOHASHDIR=true
set --
for dependency in "$seed"/*-src; do
 dirname=${dependency##*/}
 key=$(printf '%s' "${dirname%-src}" | tr '[:lower:]' '[:upper:]')
 target="$src/.scratch/ccache-deps/$dirname"
 [ -d "$target" ] || cp -R "$dependency" "$target"
 set -- "$@" "-DFETCHCONTENT_SOURCE_DIR_$key=$target"
done
case "$kind" in
 release) set -- "$@" -DCMAKE_BUILD_TYPE=Release ;;
 asan|tsan)
  set -- "$@" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
   "-DCMAKE_C_COMPILER=${SANITIZER_CC:?Set SANITIZER_CC}" \
   "-DCMAKE_CXX_COMPILER=${SANITIZER_CXX:?Set SANITIZER_CXX}" \
   "-DCMAKE_CXX_FLAGS=${SANITIZER_CXX_FLAGS:-}"
  if [ "$kind" = asan ]; then set -- "$@" -DASAN=ON -DUBSAN=ON; else set -- "$@" -DTSAN=ON; fi
  ;;
 tests) set -- "$@" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON ;;
 *) echo "unknown configuration: $kind" >&2; exit 2 ;;
esac
unset CCACHE_STATSLOG CCACHE_DEBUG CCACHE_DEBUGDIR
cmake -S "$src" -B "$build" -DLTO=OFF -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
 -DCMAKE_C_COMPILER_LAUNCHER=ccache -DCMAKE_CXX_COMPILER_LAUNCHER=ccache "$@"
name=$(basename "$build")
git -C "$src" rev-parse HEAD > "$evidence/$name.commit"
export CCACHE_STATSLOG="$evidence/$name.stats"
export CCACHE_DEBUG=1
export CCACHE_DEBUGDIR="$evidence/debug/$name"
cmake --build "$build" --parallel 4
ccache --print-stats > "$evidence/$name.counters"
ccache --show-stats --verbose
