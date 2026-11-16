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


# thread_stacks.sh BIN [PROCESSORS]: start BIN as stall_load.py does (PROCESSORS reactors, default 4), then print
# every thread as "tid comm" and, for each thread, gdb's backtrace (its start routine is the last frame), so a
# census offender (e.g. extra Xapiand:MAIN threads) can be traced to the code that creates it. Linux only; needs gdb
# and ptrace rights (sudo gdb when ptrace_scope forbids a sibling).
. "$(dirname "$0")/env.sh"
BIN=$1; N=${2:-4}
W=$(mktemp -d "$(cd "$(dirname "$0")/.." && pwd)/.scratch/thread_stacks.XXXXXX"); trap 'kill "$P" 2>/dev/null; rm -rf "$W"' EXIT
"$BIN" --port "$(free_port)" --name stacks -D "$W/data" --processors "$N" >"$W/node.log" 2>&1 & P=$!
sleep 15
for t in /proc/$P/task/*; do printf '%s %s\n' "${t##*/}" "$(cat "$t/comm")"; done
G=gdb; [ "$(id -u)" = 0 ] || G="sudo gdb"
$G -p "$P" -batch -ex 'set pagination off' -ex 'thread apply all bt 12' 2>&1 | grep -v '^\[New LWP\|^warning'
