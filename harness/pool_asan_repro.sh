#!/usr/bin/env bash
# Builds test_pool at this checkout under ASan+UBSan (the flags of ci.yml's sanitizer job) and runs it
# RUNS times (default 5); fails on the first failing run. STRATA_PIN=<sha> builds against that strata
# commit instead of the pinned one (the old pin's torn-down-scheduler reproducer check). A failed
# configure or build exits 126, so a --fails-without job never counts a build break as the failure.
# TEST_FILTER=<name> runs only the tests whose name contains it. A run stuck past RUN_TIMEOUT seconds
# (default 600) is killed and reported as the failure (exit 1), well inside a job's deadline.
set -euo pipefail
broken() { echo 'build failed' >&2; exit 126; }
cd "$(dirname "$0")/.."
RUNS=${RUNS:-5}
RUN_TIMEOUT=${RUN_TIMEOUT:-600}
stamp() { echo "[$(date +%T)] $*"; }
tag=${STRATA_PIN:+oldpin-${STRATA_PIN:0:8}}
tag=${tag:-pinned}
build=build-asan-$tag
deps=$PWD/.deps
if [[ -n ${STRATA_PIN:-} ]]; then
  # Its own deps, fetched fresh: a copied .deps fails, since each subbuild cache names its directory.
  deps=$PWD/.deps-pin-${STRATA_PIN:0:8}
  trap 'git checkout -q -- CMakeLists.txt' EXIT
  sed -E "/FetchContent_Declare\(strata /s/GIT_TAG [0-9a-f]{40}/GIT_TAG $STRATA_PIN/" CMakeLists.txt > CMakeLists.txt.new
  mv CMakeLists.txt.new CMakeLists.txt
  grep -q "GIT_TAG $STRATA_PIN" CMakeLists.txt
fi
flags="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -D_GLIBCXX_ASSERTIONS"
extra=()
if [[ $(uname) == Darwin ]]; then
  sdk=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk/usr/include
  mkdir -p sdkinc
  ln -sf "$sdk/zlib.h" "$sdk/zconf.h" sdkinc/
  ln -sfn "$sdk/uuid" sdkinc/uuid
  L=/usr/local/opt/llvm@22
  extra=(-DCMAKE_C_COMPILER=$L/bin/clang -DCMAKE_CXX_COMPILER=$L/bin/clang++
         -DZLIB_INCLUDE_DIR="$PWD/sdkinc" -DUUID_INCLUDE_DIR="$PWD/sdkinc")
  flags_ld="$flags -L$L/lib/c++ -Wl,-rpath,$L/lib/c++"
  leaks=0
else
  flags_ld=$flags
  leaks=1
fi
stamp configure
cmake -G Ninja -S . -B "$build" -DFETCHCONTENT_BASE_DIR="$deps" \
  -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_C_FLAGS=-O1 $flags" "-DCMAKE_CXX_FLAGS=-O1 $flags" \
  "-DCMAKE_EXE_LINKER_FLAGS=$flags_ld" "-DCMAKE_SHARED_LINKER_FLAGS=$flags_ld" \
  -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF ${extra[@]+"${extra[@]}"} || broken
cmake --build "$build" -j4 --target xapiand_test_pool || broken
export ASAN_OPTIONS=detect_leaks=$leaks:halt_on_error=1:abort_on_error=1
export UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1
stamp "built; running"
for i in $(seq 1 "$RUNS"); do
  stamp "run $i/$RUNS"
  rc=0
  timeout -k 10 "$RUN_TIMEOUT" "$build/bin/xapiand_test_pool" ${TEST_FILTER:+"$TEST_FILTER"} || rc=$?
  if [[ $rc == 124 ]]; then
    echo "run $i hung: no exit within ${RUN_TIMEOUT}s" >&2
    exit 1
  elif [[ $rc != 0 ]]; then
    stamp "run $i failed (exit $rc)"
    exit "$rc"
  fi
done
stamp "all $RUNS runs passed"
