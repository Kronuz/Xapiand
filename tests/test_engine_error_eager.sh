#!/bin/sh
# Mutant for test_a_damaged_segment_answers_at_first_read: STRATA_EAGER_SEGMENTS=1 maps every segment at open, as the
# old pin did, so the damage fails the open instead of the first read. Exits 0 only when the test fails its own
# check of the first read's kind; 2 when the build fails, 1 when the test passes or fails elsewhere.
set -u
cmake -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF >/dev/null || exit 2
cmake --build build -j4 --target xapiand_test_engine_error || exit 2
out=$(cd build && STRATA_EAGER_SEGMENTS=1 ./bin/xapiand_test_engine_error 2>&1)
status=$?
printf '%s\n' "$out"
[ "$status" -ne 0 ] || exit 1
printf '%s\n' "$out" | grep -q 'test_engine_error.cc:[0-9]*: CHECK(kind == errc::corrupt)' || exit 1
exit 0
