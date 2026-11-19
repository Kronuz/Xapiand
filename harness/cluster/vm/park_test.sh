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

# park_test.sh -- every guest is parked on every way a run ends (README.md, Parking):
# the caller's group SIGKILLed, the caller SIGTERMed, the guardian SIGKILLed (then sweep), a keeper
# SIGKILLed, down, and the deadline (PARK_TESTS picks them). Each case passes when the
# run's state says parked, none of its QEMU processes lives, and its overlays are gone.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
CLUSTER="$HERE/../../../tools/vm/cluster.sh"
. "$HERE/../../env.sh"
SCRATCH="${CLUSTER_VM_TEST_ROOT:-$HOME/.scratch/cluster-vm-test}/$$"
failed=0

field() { "$PY" -c 'import json, sys; s = json.load(open(sys.argv[1] + "/state.json")); print(eval(sys.argv[2], {}, {"s": s}))' "$@"; }
live_qemus() { ps -axo pid=,command= | awk -v run="$1" 'index($0, run) && /qemu-system/ {print $1}'; }

# expect NAME RUN SECONDS [parked|stopped]: within SECONDS the run's QEMUs are gone (and its state parked).
expect() {
	local name=$1 run=$2 seconds=$3 want=${4:-parked} t=0 status
	while [ $t -lt "$seconds" ]; do
		status=$(field "$run" 's.get("status")' 2>/dev/null)
		if [ -z "$(live_qemus "$run")" ] && { [ "$want" = stopped ] || [ "$status" = parked ]; }; then
			if [ "$want" = parked ] && ls "$run"/node*.qcow2 >/dev/null 2>&1; then break; fi
			echo "ok   $name: $want after ${t} s ($(field "$run" 's.get("reason")' 2>/dev/null))"
			return 0
		fi
		sleep 1
		t=$((t + 1))
	done
	echo "FAIL $name: after ${seconds} s status $(field "$run" 's.get("status")' 2>/dev/null), QEMUs $(live_qemus "$run" | tr '\n' ' ')"
	failed=1
	return 1
}

# start_run ROOT [N]: cluster.sh run in its own process group; sets PID and RUN once the guests are up.
start_run() {
	local root=$1 t=0
	mkdir -p "$root"
	PID="" RUN=""
	set -m
	CLUSTER_VM_ROOT=$SCRATCH "$CLUSTER" run --nodes "${2:-2}" --out "$root/out" -- sh -c 'echo $$ > "$0/cmd"; echo "$CLUSTER_VM_RUN" > "$0/ready"; exec sleep 600' "$root" >"$root/run.log" 2>&1 &
	PID=$!
	set +m
	while [ ! -s "$root/ready" ] && [ $t -lt 400 ]; do sleep 1; t=$((t + 1)); done
	[ -s "$root/ready" ] || { echo "FAIL $root: no run came up"; cat "$root/run.log"; failed=1; return 1; }
	RUN=$(cat "$root/ready")
}

case_group_kill() {
	start_run "$SCRATCH/group-kill" 3 || return
	kill -KILL -- "-$PID"
	expect "caller's group SIGKILLed (3 guests)" "$RUN" 15
}

case_term() {
	local root=$SCRATCH/term
	start_run "$root" || return
	kill -TERM "$PID"
	expect "caller SIGTERMed" "$RUN" 90
	kill -0 "$(cat "$root/cmd")" 2>/dev/null && { echo "FAIL caller SIGTERMed: CMD still runs"; failed=1; }
	kill -KILL -- "-$PID" 2>/dev/null
	[ -s "$root/out/node1/journal.txt" ] && echo "ok   caller SIGTERMed: collected node1's journal" \
		|| { echo "FAIL caller SIGTERMed: nothing collected"; failed=1; }
}

case_up() { # ROOT [ARGS]: cluster.sh up; prints the run
	mkdir -p "$1"
	local root=$1
	shift
	CLUSTER_VM_ROOT=$SCRATCH "$CLUSTER" up --nodes 2 "$@" 2>"$root/up.log" || { echo "FAIL up in $root" >&2; cat "$root/up.log" >&2; }
}

case_guardian_kill() {
	local root=$SCRATCH/guardian-kill run
	run=$(case_up "$root") && [ -n "$run" ] || { failed=1; return; }
	kill -KILL "$(field "$run" 's["guard"]')"
	expect "guardian SIGKILLed: keepers stop the guests" "$run" 20 stopped
	CLUSTER_VM_ROOT=$SCRATCH "$CLUSTER" sweep
	expect "guardian SIGKILLed: sweep parks the run" "$run" 5
}

case_keeper_kill() {
	local root=$SCRATCH/keeper-kill run
	run=$(case_up "$root") && [ -n "$run" ] || { failed=1; return; }
	kill -KILL "$(field "$run" 's["nodes"][0]["keeper"]')"
	expect "a keeper SIGKILLed" "$run" 30
}

case_down() {
	local root=$SCRATCH/down run
	run=$(case_up "$root") && [ -n "$run" ] || { failed=1; return; }
	CLUSTER_VM_ROOT=$SCRATCH CLUSTER_VM_RUN=$run "$CLUSTER" down
	expect "down" "$run" 5
}

case_deadline() {
	local root=$SCRATCH/deadline run
	run=$(case_up "$root" --timeout 150) && [ -n "$run" ] || { failed=1; return; }
	expect "the deadline" "$run" 180
}

for test in ${PARK_TESTS:-group_kill term guardian_kill keeper_kill down deadline}; do
	"case_$test"
done
[ "$failed" = 0 ] && rm -rf "$SCRATCH"
echo "park_test: $([ "$failed" = 0 ] && echo PASS || echo "FAIL (logs in $SCRATCH)")"
exit "$failed"
