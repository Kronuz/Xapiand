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

# lua_mem.sh PROBE [ARGS...]: builds xapiand and the Lua test in ./build (the suite's configuration), then runs
# PROBE: `seq` (lua_mem_seq.py), `same` (lua_mem_same.py), `test` (xapiand_test_lua), or `t10 [N]` (Phase 7c's
# T10 row, gate7c_rows.sh t10_lua, on N reactors when given). Output goes to $JOB_OUT when set, else .scratch/lua_mem.
set -u
. "$(dirname "$0")/env.sh"
H="$(cd "$(dirname "$0")" && pwd)"; REPO="$(dirname "$H")"
OUT=${JOB_OUT:-$REPO/.scratch/lua_mem}; mkdir -p "$OUT"
SRC=${LUA_MEM_SRC:-$REPO}   # the source to build: another checkout (a job's --ref) measures it with these probes
probe=$1; shift
cmake -G Ninja -S "$SRC" -B "$SRC/build" -DFETCHCONTENT_BASE_DIR="$SRC/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo \
	-DBUILD_TESTS=ON -DASSERTS=ON -DLTO=OFF >"$OUT/configure.log" 2>&1 || { tail -30 "$OUT/configure.log"; exit 3; }
cmake --build "$SRC/build" -j4 --target xapiand xapiand_test_lua >"$OUT/build.log" 2>&1 || { tail -30 "$OUT/build.log"; exit 3; }
echo "harness $(git -C "$REPO" rev-parse --short HEAD), source $(git -C "$SRC" rev-parse --short HEAD)"
case $probe in
	seq) exec "$PY" -I "$H/lua_mem_seq.py" "$SRC/build/bin/xapiand" "$OUT/seq" "$@" ;;
	same) exec "$PY" -I "$H/lua_mem_same.py" "$SRC/build/bin/xapiand" "$OUT/same" "$@" ;;
	test) exec "$SRC/build/bin/xapiand_test_lua" "$@" ;;
	t10)
		bin="$SRC/build/bin/xapiand"
		if [ -n "${1:-}" ]; then
			bin="$OUT/xapiand-$1"
			printf '#!/bin/sh\nexec "%s" "$@" --processors %s\n' "$SRC/build/bin/xapiand" "$1" >"$bin"; chmod +x "$bin"
		fi
		GATE7C_ROWS=t10_lua bash "$H/gate7c_rows.sh" "$bin" "$bin" "$OUT/rows"; rc=$?
		cat "$OUT/rows/t10_lua.log"; exit $rc ;;
	*) echo "lua_mem.sh: unknown probe $probe" >&2; exit 2 ;;
esac
