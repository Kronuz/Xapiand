#!/bin/sh
# Mutant for test_a_damaged_segment_answers_corrupt_until_healed: strata's corrupt_segment translates to errc::database
# instead of errc::corrupt (src/engine/strata/shard.cc, rethrow_translated). Exits 0 only when the test fails its own
# check of the kind; 2 when the mutation or the build fails, 1 when the test passes or fails elsewhere. The source is
# restored on exit, so a reused checkout builds the code as committed.
set -u
src=src/engine/strata/shard.cc
trap 'git checkout -q -- "$src"' EXIT
python3 - "$src" <<'PY' || exit 2
import sys
from pathlib import Path
p = Path(sys.argv[1]); t = p.read_text()
old = "catch (const ::strata::corrupt_segment& exc) {\n\t\tthrow Error(errc::corrupt, exc.what());"
if t.count(old) != 1:
    sys.exit("mutation site not found")
p.write_text(t.replace(old, "catch (const ::strata::corrupt_segment& exc) {\n\t\tthrow Error(errc::database, exc.what());"))
PY
cmake -S . -B build -DFETCHCONTENT_BASE_DIR="$PWD/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF >/dev/null || exit 2
cmake --build build -j4 --target xapiand_test_engine_error || exit 2
out=$(cd build && ./bin/xapiand_test_engine_error 2>&1)
status=$?
printf '%s\n' "$out"
[ "$status" -ne 0 ] || exit 1
printf '%s\n' "$out" | grep -q 'test_engine_error.cc:[0-9]*: CHECK(kind == errc::corrupt)' || exit 1
exit 0
