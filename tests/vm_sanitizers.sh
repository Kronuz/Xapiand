#!/usr/bin/env bash
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
# vm_sanitizers.sh SAN [STRATA_DIR]
#
# The registry's sanitizer proof on Linux (x-pool XP6): ships this checkout to the local QEMU VM through strata's
# tools/vm/aws-linux.sh run (a per-run directory, a light hold), builds it with clang and SAN (asan: ASan+UBSan;
# tsan), runs the registry's suites, graceful_shutdown_check.sh, path_boundary_check.sh and the docs E2E, and fails
# on any failing check or sanitizer report. Results land in $JOB_OUT (or .scratch/vm-SAN).
set -euo pipefail

SAN="${1:?usage: vm_sanitizers.sh asan|tsan [STRATA_DIR]}"
STRATA="${2:-$HOME/code/Kronuz/strata}"
REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${JOB_OUT:-$REPO/.scratch}/vm-$SAN"
TREE="$OUT/tree"
rm -rf "$TREE"; mkdir -p "$TREE/xapiand"
git -C "$REPO" archive HEAD | tar -xf - -C "$TREE/xapiand"
# UBSan reports rapidjson's and LZ4's pointer arithmetic on null (pre-existing, third-party).
printf 'pointer-overflow:rapidjson\npointer-overflow:lz4.c\n' > "$TREE/ubsan.supp"

cat > "$OUT/run.sh" <<EOF
SAN=$SAN
EOF
cat >> "$OUT/run.sh" <<'EOF'
set -u
W=$PWD; B=$W/build-$SAN; fail=0
cc=clang-18; cxx=clang++-18
command -v $cxx >/dev/null || { cc=clang; cxx=clang++; }
if [ $SAN = tsan ]; then f="-fsanitize=thread -fno-omit-frame-pointer"; else f="-fsanitize=address,undefined -fno-omit-frame-pointer"; fi
reports() { grep -cE 'WARNING: ThreadSanitizer|ERROR: AddressSanitizer|runtime error' "$@" 2>/dev/null | awk -F: '{s+=$NF} END {print s+0}'; }
cmake -S $W/xapiand -B $B -GNinja -DCMAKE_C_COMPILER=$cc -DCMAKE_CXX_COMPILER=$cxx \
	-DCMAKE_BUILD_TYPE=RelWithDebInfo -DASSERTS=ON -DLTO=OFF -DBUILD_TESTS=ON \
	"-DCMAKE_CXX_FLAGS=$f" "-DCMAKE_C_FLAGS=$f" "-DCMAKE_EXE_LINKER_FLAGS=$f" > $OUT/cfg.log 2>&1 || { tail -30 $OUT/cfg.log; exit 1; }
ninja -C $B -j4 xapiand xapiand_test_pool xapiand_test_reactor_requests xapiand_test_engine_host xapiand_test_node xapiand_test_wide_search xapiand_test_blocking_guard > $OUT/build.log 2>&1 || { grep -E "error:|FAILED" $OUT/build.log | head -20; exit 1; }
export TSAN_OPTIONS=halt_on_error=0:second_deadlock_stack=1:die_after_fork=0
export ASAN_OPTIONS=detect_leaks=0:halt_on_error=1:abort_on_error=1 UBSAN_OPTIONS=print_stacktrace=1:suppressions=$W/ubsan.supp
for t in pool reactor_requests engine_host node wide_search blocking_guard; do
	extra=""; [ $SAN = tsan ] && [ $t = node ] && extra=":report_thread_leaks=0"
	rc=0; (cd $B && TSAN_OPTIONS="$TSAN_OPTIONS$extra" timeout 1500 bin/xapiand_test_$t > $OUT/$t.log 2>&1) || rc=$?
	n=$(reports $OUT/$t.log); echo "$SAN $t rc=$rc reports=$n $(grep -E '(checks|FAIL )' $OUT/$t.log | tail -2 | tr '\n' ' ')"
	[ $rc = 0 ] && [ $n = 0 ] || fail=1
done
rc=0; (cd $W/xapiand && BIN=$B/bin/xapiand PORT=18905 timeout 900 harness/graceful_shutdown_check.sh > $OUT/graceful.log 2>&1) || rc=$?
echo "graceful rc=$rc $(tail -1 $OUT/graceful.log)"; [ $rc = 0 ] || fail=1
rc=0; (cd $W/xapiand && D=$W/path PORT=18837 timeout 600 harness/path_boundary_check.sh $B/bin/xapiand > $OUT/path.log 2>&1) || rc=$?
n=$(grep -c '^FAIL' $OUT/path.log || true); echo "path rc=$rc fails=$n"; [ $rc = 0 ] && [ "$n" = 0 ] || fail=1
mkdir -p $W/nm $W/e2etmp; printf '#!/bin/sh\nexec npx -y newman@6.2.3 "$@"\n' > $W/nm/newman; chmod +x $W/nm/newman
(cd $W/xapiand && PATH=$W/nm:$PATH E2E_TMP=$W/e2etmp PORT=18880 timeout 1500 harness/e2e_capture.sh $B/bin/xapiand $W/e2e.json > $OUT/e2e-capture.log 2>&1)
python3 $W/xapiand/harness/e2e_assert.py $W/e2e.json > $OUT/e2e.log 2>&1 || true
cp $W/e2etmp/xe2e_node.log $OUT/e2e-node.log 2>/dev/null || true
n=$(reports $OUT/e2e-node.log); echo "e2e $(head -1 $OUT/e2e.log) node reports=$n"
grep -q '^GREEN' $OUT/e2e.log && [ $n = 0 ] || fail=1
echo "vm $SAN: $([ $fail = 0 ] && echo PASS || echo FAIL)"
exit $fail
EOF
unset STRATA_AWS_VM_ALLOW
"$STRATA/tools/vm/aws-linux.sh" run "$TREE" "$OUT/run.sh"
