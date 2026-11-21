#!/usr/bin/env bash
# coverage_one.sh TEST FILE...: builds tests/test_TEST.cc with the coverage instrumentation of harness/coverage.sh (LLVM's libc++
# only), runs it, and prints for each source FILE (src/...) its line and branch coverage and the uncovered lines and branches.
# Env: BUILD (default build-cov-TEST), JOBS (default 4), REPEAT (run the test N times; exit nonzero when any run fails), LLVM (default: Homebrew's llvm@22 or llvm).
set -u
TEST=$1; shift
ROOT=$(cd "$(dirname "$0")/.." && pwd); cd "$ROOT" || exit 126
BUILD=${BUILD:-$ROOT/build-cov-$TEST}; JOBS=${JOBS:-4}
for p in ${LLVM:-} /usr/local/opt/llvm@22 /usr/local/opt/llvm /opt/homebrew/opt/llvm@22 /opt/homebrew/opt/llvm; do
	[ -x "$p/bin/llvm-cov" ] && { LLVM=$p; break; }
done
F="-fprofile-instr-generate -fcoverage-mapping -fno-omit-frame-pointer"
mkdir -p sdkinc; for f in zlib.h zconf.h uuid; do ln -sfn "$(xcrun --show-sdk-path)/usr/include/$f" sdkinc/$f; done
cmake -G Ninja -S . -B "$BUILD" -DFETCHCONTENT_BASE_DIR="$ROOT/.deps" -DCMAKE_BUILD_TYPE=RelWithDebInfo -DBUILD_TESTS=ON -DLTO=OFF \
	-DCMAKE_C_COMPILER=$LLVM/bin/clang -DCMAKE_CXX_COMPILER=$LLVM/bin/clang++ "-DCMAKE_C_FLAGS=$F" "-DCMAKE_CXX_FLAGS=$F" \
	"-DCMAKE_EXE_LINKER_FLAGS=$F -L$LLVM/lib/c++ -Wl,-rpath,$LLVM/lib/c++" -DLIBCPP_LIBRARIES=$LLVM/lib/c++/libc++.dylib \
	-DLIBCPPABI_LIBRARIES=$LLVM/lib/c++/libc++abi.dylib -DZLIB_INCLUDE_DIR="$ROOT/sdkinc" -DUUID_INCLUDE_DIR="$ROOT/sdkinc" >"$BUILD.log" 2>&1 || { tail -20 "$BUILD.log"; exit 1; }
cmake --build "$BUILD" -j"$JOBS" --target "xapiand_test_coverage_$TEST" >>"$BUILD.log" 2>&1 || { grep -E "(error|FAILED|undefined symbol|ld: )" "$BUILD.log" | grep -vE "error_code|error_category" | head -30; echo "--- tail of $BUILD.log:"; tail -25 "$BUILD.log"; exit 1; }
rm -f "$BUILD"/prof-*.profraw
rc=0; bad=0
for _ in $(seq "${REPEAT:-1}"); do
	LLVM_PROFILE_FILE="$BUILD/prof-%p.profraw" "$BUILD/bin/xapiand_test_coverage_$TEST" || { rc=$?; bad=$((bad + 1)); }
done
[ "${REPEAT:-1}" -gt 1 ] && echo "runs: $bad of $REPEAT failed; libc++ copies linked: $(otool -L "$BUILD/bin/xapiand_test_coverage_$TEST" | grep -c 'libc++\.')"
echo "test exit: $rc"
$LLVM/bin/llvm-profdata merge -sparse -o "$BUILD/merged.profdata" "$BUILD"/prof-*.profraw || exit 1
$LLVM/bin/llvm-cov export -format=lcov -instr-profile "$BUILD/merged.profdata" "$BUILD/bin/xapiand_test_coverage_$TEST" 2>/dev/null >"$BUILD/coverage.lcov"
python3 -I - "$BUILD/coverage.lcov" "$@" <<'PY'
import sys
lcov, wanted = sys.argv[1], sys.argv[2:]
files = {}
cur = None
for l in open(lcov):
    l = l.strip()
    if l.startswith("SF:"):
        cur = files.setdefault(l[3:], ({}, {}))
    elif cur is not None and l.startswith("DA:"):
        n, c = l[3:].split(",")[:2]; cur[0][int(n)] = cur[0].get(int(n), 0) + int(c)
    elif cur is not None and l.startswith("BRDA:"):
        n, b, k, t = l[5:].split(",")
        key = (int(n), b, k); cur[1][key] = cur[1].get(key, 0) + (0 if t == "-" else int(t))
def ranges(ns):
    out, ns = [], sorted(ns)
    i = 0
    while i < len(ns):
        j = i
        while j + 1 < len(ns) and ns[j + 1] == ns[j] + 1: j += 1
        out.append(str(ns[i]) if i == j else f"{ns[i]}-{ns[j]}"); i = j + 1
    return ",".join(out)
for src in wanted:
    hit = [f for f in files if f.endswith("/" + src) or f == src]
    if not hit:
        print(f"{src}: not in the profile's mapping"); continue
    lines, branches = files[hit[0]]
    lc = sum(1 for c in lines.values() if c); bt = len(branches); bc = sum(1 for t in branches.values() if t)
    print(f"{src}: lines {lc}/{len(lines)} ({100*lc/max(1,len(lines)):.1f}%), branches {bc}/{bt} ({100*bc/max(1,bt):.1f}%)")
    print("  uncovered lines:", ranges([n for n, c in lines.items() if not c]))
    print("  branch lines with an untaken branch:", ranges({k[0] for k, t in branches.items() if not t}))
PY
exit $rc
