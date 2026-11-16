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
# WORKLOADS picks them (default "ctest corpora e2e"). The profiles are merged and llvm-cov exports one summary per source file
# (.cc files; COV_HEADERS=1 adds the headers), grouped in the areas of AREAS below. Writes OUTDIR/coverage.txt (the table),
# OUTDIR/coverage-files.txt (every file) and OUTDIR/coverage.json (the export). `report` redoes the table from OUTDIR's
# merged profile. STRICT=1 exits 3 when an area is below its target. Run it under a hold:
#   quiet light NAME MIN "coverage" -- harness/coverage.sh OUTDIR
# Env: BUILD, JOBS (default 4), LLVM (a prefix with bin/clang++, bin/llvm-cov and bin/llvm-profdata; default: Homebrew's
# llvm@22, llvm, or the tools on PATH), WORKLOADS, COV_HEADERS, STRICT.
. "$(dirname "$0")/env.sh"
HERE=$(cd "$(dirname "$0")" && pwd); ROOT=$(cd "$HERE/.." && pwd)
MODE=build; [ "${1:-}" = report ] && { MODE=report; shift; }
OUT=$(mkdir -p "${1:-$ROOT/.scratch/coverage}" && cd "${1:-$ROOT/.scratch/coverage}" && pwd)
BUILD=${BUILD:-$ROOT/build-cov}; JOBS=${JOBS:-4}; WORKLOADS=${WORKLOADS:-ctest corpora e2e}
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
	[ -n "${LLVM:-}" ] && [ -d "$LLVM/lib/c++" ] && ld="-L$LLVM/lib/c++ -Wl,-rpath,$LLVM/lib/c++"
	cmake -S "$ROOT" -B "$BUILD" -DFETCHCONTENT_BASE_DIR="$ROOT/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DLTO=OFF \
		-DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" "-DCMAKE_C_FLAGS=$flags" "-DCMAKE_CXX_FLAGS=$flags" \
		"-DCMAKE_EXE_LINKER_FLAGS=$flags $ld" "${extra[@]}" || return 1
	cmake --build "$BUILD" -j"$JOBS" && cmake --build "$BUILD" -j"$JOBS" --target strata_check
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
		*) echo "coverage: unknown workload '$w'" >&2; rc=1 ;;
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
		"$first" "${objs[@]}" >"$OUT/coverage.json" 2>"$OUT/llvm-cov.err"
}

report() {
	"$PY" -I - "$ROOT" "$OUT" "${COV_HEADERS:-0}" <<'PYEOF'
import json, os, sys
root, out, headers = sys.argv[1], sys.argv[2], sys.argv[3] == "1"
data = json.load(open(f"{out}/coverage.json"))["data"][0]["files"]
# AREAS: (name, target lines %, target branches %, path prefixes); the first area a file matches takes it. The targets are
# TESTING.md's Coverage table.
AREAS = [
    ("decoders, parsers, serialisation", 98, 95, ["src/engine/strata/document.cc", "src/engine/strata/pattern.cc", "src/engine/codec.cc",
                                                  "src/field_parser.cc", "src/serialise.cc", "src/y2j/"]),
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
	build || { echo "coverage: the build failed" >&2; exit 1; }
	workloads || rc=1
	export_json || { echo "coverage: no profile data merged or exported (see $OUT/llvm-cov.err)" >&2; exit 1; }
fi
report; r=$?
[ $rc != 0 ] && exit $rc
[ "${STRICT:-0}" = 1 ] && exit $r
exit 0
