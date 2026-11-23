#!/bin/bash
# ICU's word break iterator against the segmenter on the zh, ja and ko corpora, at the analysis
# commit CMakeLists.txt pins. Needs ICU 78 (ICU_ROOT) and a clone of Kronuz/analysis (ANALYSIS_REPO).
set -euo pipefail
here=$(cd "$(dirname "$0")/.." && pwd)
pin=$(sed -n 's|.*Kronuz/analysis.git GIT_TAG \([0-9a-f]*\).*|\1|p' "$here/CMakeLists.txt")
repo=${ANALYSIS_REPO:-$HOME/code/Kronuz/analysis}
out=${ORACLE_DIR:-$HOME/.scratch/xapiand/cjk-words/oracle-build}
rm -rf "$out/src"; mkdir -p "$out"
git -C "$repo" archive --prefix=src/ "$pin" | tar -x -C "$out"
cmake -G Ninja -S "$out/src" -B "$out/b" -DCMAKE_BUILD_TYPE=Release -DANALYSIS_ICU_TOOLS=ON \
	-DICU_ROOT="${ICU_ROOT:-/usr/local/opt/icu4c@78}" >/dev/null
cmake --build "$out/b" --target analysis_icu_compare -j4 >/dev/null
"$out/b/analysis_icu_compare" corpus "$out"/src/test/corpora/{zh,ja,ko}.txt
