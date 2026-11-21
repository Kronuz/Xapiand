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

# engine_crash_repeat.sh KIND N: repeat crash scenario D N times on build-KIND/bin/xapiand, after a mutant (one poke left
# undeleted) proves a wrong row prints whole. Failing runs (rundir and log) are kept in ~/.scratch/xapiand/crash-wrong-row/KIND.
set -u
K=$1; N=$2; BIN=$PWD/build-$K/bin/xapiand; OUT=$HOME/.scratch/xapiand/crash-wrong-row/$K; mkdir -p "$OUT" "$PWD/.scratch/cwr"
. harness/env.sh
export ASAN_OPTIONS=detect_leaks=0
sed 's/-XDELETE "localhost:\$PORT\/\$1\/poke\$i"/-XDELETE "localhost:$PORT\/$1\/poke$((i + 1))"/' harness/engine_crash_check.sh > .scratch/cwr/mutant.sh
cmp -s harness/engine_crash_check.sh .scratch/cwr/mutant.sh && { echo "mutant: sed did not apply"; exit 3; }
R=$PWD/.scratch/cwr/$K-mutant; RUNDIR=$R BIN=$BIN PORT=$(free_port) bash .scratch/cwr/mutant.sh --only D > "$R.log" 2>&1; rc=$?
echo "mutant rc=$rc"; grep -E "wrong row|writer .* n |poke:|FAIL" "$R.log"
[ $rc != 0 ] && grep -q "D wrong row.*'poke0 None'.*GET 200" "$R.log" || { echo "mutant: the wrong row did not print"; exit 4; }
f=0
for i in $(seq "$N"); do
	R=$PWD/.scratch/cwr/$K-$i
	KEEP_RUNDIR=1 RUNDIR=$R BIN=$BIN PORT=$(free_port) bash harness/engine_crash_check.sh --only D > "$R.log" 2>&1; rc=$?
	echo "$K $i rc=$rc $(grep -E "FAIL|wrong row|writer .* n |poke:|writers, [0-9]+ documents" "$R.log" | sed 's/;.*//' | tr '\n' ' ' | cut -c1-400)"
	if [ $rc != 0 ]; then f=$((f + 1)); cp -R "$R" "$R.log" "$OUT/"; fi
	rm -rf "$R"
done
echo "$K: $f of $N iterations failed"
[ $f = 0 ]
