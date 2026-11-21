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

# coverage.sh [OUTDIR] | coverage.sh report OUTDIR: source-based line and branch coverage of src/, per area, beside the targets of
# TESTING.md.
#
# Builds with clang -fprofile-instr-generate -fcoverage-mapping into $BUILD (default build-cov), then runs the workloads:
#   ctest      every unit test
#   corpora    harness/restore_corpora.sh check, on the instrumented node
#   e2e        harness/engine_e2e.sh, the docs E2E on the instrumented node (needs newman 6 first on PATH; skipped without it)
#   <name>     harness/coverage_e2e_<name>.* (any extension), run as `DRIVER BINARY OUTDIR/<name>`; every such driver is discovered
#              and run by default (main: the node's flags, start-up refusals, signals, shutdowns, dump and restore)
# WORKLOADS picks them (default "ctest corpora e2e" and every discovered driver). The profiles are merged and llvm-cov exports one summary per source file
# (.cc files; COV_HEADERS=1 adds the headers), grouped in the areas of AREAS below. Writes OUTDIR/coverage.txt (the table),
# OUTDIR/coverage-files.txt (every file) and OUTDIR/coverage.json (the export). `report` redoes the table from OUTDIR's
# merged profile; OUTDIR/coverage-uncovered.txt lists each file's uncovered lines and unexecuted branches (from the lcov export). STRICT=1 exits 3 when an area is below its target. Run it under a hold:
#   quiet light NAME MIN "coverage" -- harness/coverage.sh OUTDIR
# Env: BUILD, JOBS (default 4), LLVM (a prefix with bin/clang++, bin/llvm-cov and bin/llvm-profdata; default: Homebrew's
# llvm@22, llvm, or the tools on PATH), WORKLOADS, COV_HEADERS, STRICT, NOBUILD=1 (skip the build and reuse the instrumented binaries in BUILD).
. "$(dirname "$0")/env.sh"
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/.." && pwd)
MODE=build; [ "${1:-}" = report ] && { MODE=report; shift; }
OUT=$(mkdir -p "${1:-$ROOT/.scratch/coverage}" && cd "${1:-$ROOT/.scratch/coverage}" && pwd)
BUILD=${BUILD:-$ROOT/build-cov}; JOBS=${JOBS:-4}; DRIVERS=""
for f in "$(dirname "$0")"/coverage_e2e_*.*; do
	[ -f "$f" ] && { n=${f##*/coverage_e2e_}; DRIVERS="$DRIVERS ${n%%.*}"; }
done
WORKLOADS=${WORKLOADS:-"ctest corpora e2e$DRIVERS"}
export HARNESS_TMP=${HARNESS_TMP:-$OUT}

if [ -z "${LLVM:-}" ]; then
	for p in /usr/local/opt/llvm@22 /usr/local/opt/llvm /opt/homebrew/opt/llvm@22 /opt/homebrew/opt/llvm; do
		[ -x "$p/bin/llvm-cov" ] && { LLVM=$p; break; }
	done
fi
if [ -n "${LLVM:-}" ]; then CXX=$LLVM/bin/clang++; CC=$LLVM/bin/clang; COV=$LLVM/bin/llvm-cov; PROFDATA=$LLVM/bin/llvm-profdata
else CXX=$(command -v clang++) CC=$(command -v clang) COV=$(command -v llvm-cov) PROFDATA=$(command -v llvm-profdata); fi
[ -x "$CXX" ] && [ -x "$COV" ] && [ -x "$PROFDATA" ] || { echo "coverage: no clang++, llvm-cov and llvm-profdata (set LLVM=<prefix>)" >&2; exit 2; }

build() {
	local flags="-fprofile-instr-generate -fcoverage-mapping -fno-omit-frame-pointer" extra=() ld=
	if [ "$(uname)" = Darwin ]; then
		mkdir -p "$ROOT/sdkinc"
		for f in zlib.h zconf.h uuid; do ln -sfn "$(xcrun --show-sdk-path)/usr/include/$f" "$ROOT/sdkinc/$f"; done
		extra=(-DZLIB_INCLUDE_DIR="$ROOT/sdkinc" -DUUID_INCLUDE_DIR="$ROOT/sdkinc")
	fi
	# The build's find_library("c++") would add the SDK's runtime beside LLVM's: name LLVM's, so each binary loads one.
	if [ -n "${LLVM:-}" ] && [ -d "$LLVM/lib/c++" ]; then
		ld="-L$LLVM/lib/c++ -Wl,-rpath,$LLVM/lib/c++"
		extra+=(-DLIBCPP_LIBRARIES="$LLVM/lib/c++/libc++.dylib" -DLIBCPPABI_LIBRARIES="$LLVM/lib/c++/libc++abi.dylib")
	fi
	cmake -G Ninja -S "$ROOT" -B "$BUILD" -DFETCHCONTENT_BASE_DIR="$ROOT/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DLTO=OFF \
		-DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_CXX_FLAGS=$flags" \
		"-DCMAKE_EXE_LINKER_FLAGS=$flags $ld" "${extra[@]}" || return 1
	cmake --build "$BUILD" -j"$JOBS" && cmake --build "$BUILD" -j"$JOBS" --target strata_check && one_runtime
}

# Fails when a binary links two C++ runtimes (the tests then throw through one and catch in the other, or lock a mutex
# the other destroyed).
one_runtime() {
	[ "$(uname)" = Darwin ] || return 0
	local f n m bad=0
	for f in "$BUILD"/bin/*; do
		[ -f "$f" ] && [ -x "$f" ] || continue
		n=$(otool -L "$f" 2>/dev/null | awk '/libc\+\+\.[0-9.]*dylib/ {c++} END {print c+0}')
		m=$(otool -L "$f" 2>/dev/null | awk '/libc\+\+abi\.[0-9.]*dylib/ {c++} END {print c+0}')
		[ "$n" -le 1 ] && [ "$m" -le 1 ] || { echo "coverage: $f links $n libc++ and $m libc++abi" >&2; bad=1; }
	done
	return $bad
}

workloads() {
	local w rc=0
	rm -rf "$OUT/prof" && mkdir -p "$OUT/prof"
	export LLVM_PROFILE_FILE="$OUT/prof/%p-%m.profraw"
	for w in $WORKLOADS; do
		case $w in
		ctest) (cd "$BUILD" && ctest -j3 --output-on-failure) >"$OUT/ctest.log" 2>&1 || { echo "FAIL: ctest (see $OUT/ctest.log)"; rc=1; }
			tail -3 "$OUT/ctest.log" ;;
		corpora) "$HERE/restore_corpora.sh" check "$BUILD/bin/xapiand" >"$OUT/corpora.log" 2>&1 || { echo "FAIL: corpora (see $OUT/corpora.log)"; rc=1; }
			tail -2 "$OUT/corpora.log" ;;
		e2e) if command -v newman >/dev/null; then
				"$HERE/engine_e2e.sh" "$BUILD/bin/xapiand" "$OUT/e2e" >"$OUT/e2e.log" 2>&1 || { echo "FAIL: e2e (see $OUT/e2e.log)"; rc=1; }
				tail -2 "$OUT/e2e.log"
			else echo "skip: e2e (no newman on PATH)"; fi ;;
		*) local drv; drv=$(ls "$HERE"/coverage_e2e_"$w".* 2>/dev/null | head -1)
			if [ -n "$drv" ]; then
				"$drv" "$BUILD/bin/xapiand" "$OUT/$w" >"$OUT/$w.log" 2>&1 || { echo "FAIL: $w (see $OUT/$w.log)"; rc=1; }
				tail -1 "$OUT/$w.log"
			else echo "coverage: unknown workload '$w'" >&2; rc=1; fi ;;
		esac
	done
	return $rc
}

export_json() {
	local objs=() f
	"$PROFDATA" merge -sparse -o "$OUT/merged.profdata" "$OUT"/prof/*.profraw || return 1
	for f in "$BUILD"/bin/*; do [ -f "$f" ] && [ -x "$f" ] && objs+=(-object "$f"); done
	# The first object is the main one; llvm-cov takes the rest as -object.
	local first=${objs[1]}; unset 'objs[0]' 'objs[1]'
	"$COV" export -summary-only -instr-profile "$OUT/merged.profdata" -ignore-filename-regex='(/\.deps/|/_deps/|/tests/|/oldtests/|/usr/|/Cellar/|/sdkinc/)' \
		"$first" "${objs[@]}" >"$OUT/coverage.json" 2>"$OUT/llvm-cov.err" || return 1
	"$COV" export -format=lcov -instr-profile "$OUT/merged.profdata" -ignore-filename-regex='(/\.deps/|/_deps/|/tests/|/oldtests/|/usr/|/Cellar/|/sdkinc/)' \
		"$first" "${objs[@]}" >"$OUT/coverage.lcov" 2>>"$OUT/llvm-cov.err"
}

report() {
	"$PY" -I - "$ROOT" "$OUT" "${COV_HEADERS:-0}" <<'PYEOF'
import json, os, sys
root, out, headers = sys.argv[1], sys.argv[2], sys.argv[3] == "1"
data = json.load(open(f"{out}/coverage.json"))["data"][0]["files"]
# AREAS: (name, target lines %, target branches %, path prefixes); the first area a file matches takes it. The targets are
# TESTING.md's Coverage table.
AREAS = [
    ("decoders, parsers, serialization", 98, 95, ["src/engine/strata/document.cc", "src/engine/strata/pattern.cc", "src/engine/codec.cc",
                                                  "src/field_parser.cc", "src/serialize.cc", "src/y2j/"]),
    ("collectors, scorers, counting", 95, 90, ["src/engine/strata/search.cc", "src/engine/strata/expand.cc", "src/engine/search.cc"]),
    ("engine: strata", 90, 80, ["src/engine/strata/"]),
    ("engine: seam", 90, 80, ["src/engine/"]),
    ("server", 90, 80, ["src/server/"]),
    ("database", 90, 80, ["src/database/"]),
    ("aggregations", 90, 80, ["src/aggregations/"]),
    ("geospatial", 90, 80, ["src/geospatial/"]),
    ("lua", 90, 80, ["src/lua/"]),
    ("multivalue, reserved", 90, 80, ["src/multivalue/", "src/reserved/"]),
    ("core (src/*.cc)", 90, 80, ["src/"]),
]
rows = {a[0]: [0, 0, 0, 0] for a in AREAS}
files = []
for f in data:
    rel = os.path.relpath(f["filename"], root)
    if not rel.startswith("src/") or not (headers or rel.endswith(".cc")):
        continue
    s = f["summary"]
    L, B = s["lines"], s["branches"]
    if L["count"] == 0:
        continue
    for name, _, _, prefixes in AREAS:
        if any(rel.startswith(p) for p in prefixes):
            r = rows[name]
            r[0] += L["covered"]; r[1] += L["count"]; r[2] += B["covered"]; r[3] += B["count"]
            files.append((name, rel, L["covered"], L["count"], B["covered"], B["count"]))
            break
pct = lambda a, b: 100.0 * a / b if b else 100.0
lines = [f"{'area':<34} {'files':>5} {'lines':>16} {'target':>6}  {'branches':>16} {'target':>6}  verdict"]
below = 0
tot = [0, 0, 0, 0]
for name, tl, tb, _ in AREAS:
    r = rows[name]
    if r[1] == 0:
        continue
    n = sum(1 for x in files if x[0] == name)
    lp, bp = pct(r[0], r[1]), pct(r[2], r[3])
    v = "met" if lp >= tl and bp >= tb else "below (" + ", ".join(k for k, ok in (("lines", lp >= tl), ("branches", bp >= tb)) if not ok) + ")"
    below += v != "met"
    for i in range(4):
        tot[i] += r[i]
    lines.append(f"{name:<34} {n:>5} {r[0]:>7}/{r[1]:<8} {tl:>5}%  {r[2]:>7}/{r[3]:<8} {tb:>5}%  {lp:5.1f}% lines, {bp:5.1f}% branches: {v}")
lines.append(f"{'all of src/':<34} {len(files):>5} {tot[0]:>7}/{tot[1]:<8} {'':>6}  {tot[2]:>7}/{tot[3]:<8} {'':>6}  {pct(tot[0], tot[1]):5.1f}% lines, {pct(tot[2], tot[3]):5.1f}% branches")
open(f"{out}/coverage.txt", "w").write("\n".join(lines) + "\n")
# Uncovered lines and unexecuted branches per file, as line ranges, from the lcov export.
def ranges(nums):
    out_, start = [], None
    for n in sorted(set(nums)):
        if start is not None and n == prev + 1:
            prev = n
            continue
        if start is not None:
            out_.append(f"{start}-{prev}" if prev != start else f"{start}")
        start = prev = n
    if start is not None:
        out_.append(f"{start}-{prev}" if prev != start else f"{start}")
    return ",".join(out_)
if os.path.exists(f"{out}/coverage.lcov"):
    with open(f"{out}/coverage-uncovered.txt", "w") as fh:
        keep = {rel for _, rel, *_ in files}
        cur, miss, brmiss = None, [], []
        for raw in open(f"{out}/coverage.lcov"):
            raw = raw.strip()
            if raw.startswith("SF:"):
                cur, miss, brmiss = os.path.relpath(raw[3:], root), [], []
            elif raw.startswith("DA:"):
                n, c = raw[3:].split(",")[:2]
                if c == "0":
                    miss.append(int(n))
            elif raw.startswith("BRDA:"):
                n, _, _, c = raw[5:].split(",")
                if c in ("0", "-"):
                    brmiss.append(int(n))
            elif raw == "end_of_record" and cur in keep:
                fh.write(f"{cur}\n  lines: {ranges(miss) or '-'}\n  branches: {ranges(brmiss) or '-'}\n")
with open(f"{out}/coverage-files.txt", "w") as fh:
    fh.write("area\tfile\tlines covered\tlines\tline %\tbranches covered\tbranches\tbranch %\n")
    for name, rel, lc, lt, bc, bt in sorted(files):
        fh.write(f"{name}\t{rel}\t{lc}\t{lt}\t{pct(lc, lt):.1f}\t{bc}\t{bt}\t{pct(bc, bt):.1f}\n")
print("\n".join(lines))
sys.exit(3 if below else 0)
PYEOF
}

rc=0
if [ $MODE = build ]; then
	[ "${NOBUILD:-0}" = 1 ] || build || { echo "coverage: the build failed" >&2; exit 1; }
	workloads || rc=1
	export_json || { echo "coverage: no profile data merged or exported (see $OUT/llvm-cov.err)" >&2; exit 1; }
fi
report; r=$?
[ $rc != 0 ] && exit $rc
[ "${STRICT:-0}" = 1 ] && exit $r
exit 0
