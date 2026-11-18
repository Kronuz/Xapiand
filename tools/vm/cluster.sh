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

# cluster.sh -- the cluster harness's VM tier (harness/cluster/vm/README.md).
#
#   cluster.sh check [--nodes N] [--out DIR]       A1: up, multicast between every pair, a replay of a planted write, down
#   cluster.sh run [--nodes N] [--out DIR] -- CMD  up, CMD with CLUSTER_VM_RUN set, collect into DIR, down
#   cluster.sh up [--nodes N] [--timeout S]        boot N guests (default 3); prints the run directory
#   cluster.sh ssh I [CMD...]                      a shell or a command on guest I of $CLUSTER_VM_RUN
#   cluster.sh collect OUT [PATH...]               each guest's journal (and PATHs) into OUT/nodeI
#   cluster.sh down                                stop every guest of $CLUSTER_VM_RUN and delete its disks
#   cluster.sh sweep                               park the runs whose guardian died
#
# The guests' Python half comes from faultlab pinned at FAULTLAB_REV in harness/cluster/vm/.faultlab.
# check and run park every guest when they exit, on a signal, and when killed (the guardian sees the
# owner die); a run started by up parks at `down` or after CLUSTER_TIMEOUT seconds (default 1800).
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
VM="$HERE/../../harness/cluster/vm"
. "$HERE/../../harness/env.sh"

FAULTLAB_REV=a3b71d1b9bdcfa3664dbce8725f14c07bb14845f
FAULTLAB_REPO="${FAULTLAB_REPO:-https://github.com/Kronuz/faultlab.git}"
export CLUSTER_VM_FAULTLAB="$VM/.faultlab"
if [ "$(git -C "$CLUSTER_VM_FAULTLAB" rev-parse HEAD 2>/dev/null)" != "$FAULTLAB_REV" ]; then
	rm -rf "$CLUSTER_VM_FAULTLAB"
	{ git clone -q "$FAULTLAB_REPO" "$CLUSTER_VM_FAULTLAB" && git -C "$CLUSTER_VM_FAULTLAB" checkout -q "$FAULTLAB_REV"; } \
		|| { echo "cluster.sh: cannot fetch faultlab $FAULTLAB_REV from $FAULTLAB_REPO" >&2; exit 2; }
fi
vm() { "$PY" -E -s "$VM/vm.py" "$@"; }

# up, CMD, collect, down: the guardian parks the guests if this shell dies without its EXIT trap.
one_shot() {
	local nodes=3 out="" run
	while [ $# -gt 0 ]; do
		case $1 in
			--nodes) nodes=$2; shift 2 ;;
			--out) out=$2; shift 2 ;;
			--) shift; break ;;
			*) echo "cluster.sh: unknown option $1" >&2; exit 2 ;;
		esac
	done
	run=$(vm up --nodes "$nodes" --owner $$) || exit 2
	out=${out:-build/cluster-vm/$(basename "$run")}
	trap 'vm collect --run "$run" "$out" >/dev/null; vm down --run "$run"' EXIT
	trap 'exit 130' INT
	trap 'exit 143' TERM HUP
	CLUSTER_VM_RUN=$run CLUSTER_VM_OUT=$out "$@"
	exit $?
}

case "${1:-}" in
	check) shift; one_shot "$@" -- "$PY" -E -s "$VM/vm.py" check ;;
	run) shift; one_shot "$@" ;;
	up | down | sweep | collect) vm "$@" ;;
	ssh) shift; exec "$PY" -E -s "$VM/vm.py" ssh "$@" ;;
	*) sed -n '22,34p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
