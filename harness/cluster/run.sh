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

# run.sh -- the cluster harness's process tier (see README.md).
#
#   run.sh check [--nodes N] [...]        the checklist on N local nodes (checklist.py --help)
#   run.sh history FILE [--level L]       check a recorded history (faultlab history.py)
#   run.sh selftest [--mutant CHECK]      the checker against planted histories (faultlab selftest.py)
#   run.sh bandwidth [--mbit 100] [...]   a capped proxy link against its cap (faultlab proxy.py)
#   run.sh proxytest                      a partitioned link holds every event until heal (faultlab proxy_test.py)
#
# The Python half of faultlab (history, proxy, scenario) comes from a checkout pinned to
# FAULTLAB_REV in .faultlab/, fetched from FAULTLAB_REPO on first use (never vendored).

# The command runs in its own process group with every node it starts; on exit, on a signal and
# after CLUSTER_TIMEOUT seconds (default 1800) the whole group is killed, so no node outlives it.
set -u

HERE="$(cd "$(dirname "$0")" && pwd)"
. "$HERE/../env.sh"

FAULTLAB_REV=a452a8c5e79655293d114d77c2ebc54af5fb0505
FAULTLAB_REPO="${FAULTLAB_REPO:-https://github.com/Kronuz/faultlab.git}"
FAULTLAB="$HERE/.faultlab"
if [ "$(git -C "$FAULTLAB" rev-parse HEAD 2>/dev/null)" != "$FAULTLAB_REV" ]; then
	rm -rf "$FAULTLAB"
	{ git clone -q "$FAULTLAB_REPO" "$FAULTLAB" && git -C "$FAULTLAB" checkout -q "$FAULTLAB_REV"; } \
		|| { echo "run.sh: cannot fetch faultlab $FAULTLAB_REV from $FAULTLAB_REPO" >&2; exit 2; }
fi
export FAULTLAB_PYTHON="$FAULTLAB/python"

case "${1:-}" in
	check) script=$HERE/checklist.py ;;
	history) script=$FAULTLAB_PYTHON/faultlab/history.py ;;
	selftest) script=$FAULTLAB_PYTHON/tests/selftest.py ;;
	bandwidth) script=$FAULTLAB_PYTHON/faultlab/proxy.py ;;
	proxytest) script=$FAULTLAB_PYTHON/tests/proxy_test.py ;;
	*) sed -n '22,34p' "$0" | sed 's/^# \{0,1\}//'; exit 2 ;;
esac
[ "$1" = bandwidth ] || shift

set -m  # the background job below gets its own process group, which every node joins
"$PY" -E -s "$script" "$@" &
child=$!
set +m

watchdog=""
reap() {
	[ -z "$watchdog" ] || kill -- "-$watchdog" 2>/dev/null
	kill -TERM -- "-$child" 2>/dev/null
	for _ in 1 2 3 4 5 6 7 8 9 10; do kill -0 -- "-$child" 2>/dev/null || return 0; sleep 0.5; done
	kill -CONT -- "-$child" 2>/dev/null
	kill -KILL -- "-$child" 2>/dev/null
	return 0
}
trap reap EXIT
trap 'exit 130' INT
trap 'exit 143' TERM HUP

set -m  # the watchdog and its sleep share a group, so one kill ends both; it also ends with run.sh
( t=0; while [ "$t" -lt "${CLUSTER_TIMEOUT:-1800}" ]; do sleep 1; kill -0 $$ 2>/dev/null || exit 0; t=$((t + 1)); done; echo "run.sh: timeout after ${CLUSTER_TIMEOUT:-1800} s" >&2; kill -TERM -- "-$child" 2>/dev/null ) &
watchdog=$!
set +m
wait "$child"
exit $?
