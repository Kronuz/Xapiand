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

# light_share_t12.sh BIN OUTDIR ARM...
#
# T12's loadtest (x7-gate's command: a fresh node, RESTORE of 20,000 accounts, then 16 clients for 8 s) once per
# ARM, in the order given. ARM is "off" or a heavy share (LS_HEAVY_SHARE). BIN may be "ARM=PATH" pairs instead,
# comma-separated, to time different binaries (e.g. before=build-a/bin/xapiand,after=build-b/bin/xapiand).
set -u
BINS=$1; OUT=$(mkdir -p "$2" && cd "$2" && pwd); shift 2
HERE=$(cd "$(dirname "$0")" && pwd); REPO=$(dirname "$HERE")
bin_for() { case $BINS in *=*) tr ',' '\n' <<< "$BINS" | sed -n "s/^$1=//p" ;; *) echo "$BINS" ;; esac; }
for b in $(tr ',' '\n' <<< "$BINS" | sed 's/^[^=]*=//'); do [ -x "$b" ] || { echo "no binary $b" >&2; exit 1; }; done
i=0; FAILED=0
for arm in "$@"; do
	i=$((i + 1)); b=$(bin_for "$arm")
	case $BINS in *=*) unset LS_HEAVY_SHARE ;; *) if [ "$arm" = off ]; then unset LS_HEAVY_SHARE; else export LS_HEAVY_SHARE=$arm; fi ;; esac
	D="$OUT/lt.data"; rm -rf "$D"; mkdir -p "$D"
	"$b" --port 18993 --name bench -D "$D" > "$OUT/lt-$i-$arm.node.log" 2>&1 & N=$!
	for _ in $(seq 120); do curl -s localhost:18993/ > /dev/null && break; sleep 0.5; done
	python3 "$HERE/loadtest.py" --target localhost:18993 --dataset "$REPO/site/public/assets/accounts.ndjson" --replicate 20 \
		--concurrency 16 --duration 8 --trials 1 --datadir "$D" --label "$arm" --out "$OUT/lt-$i-$arm.json" > "$OUT/lt-$i-$arm.log" 2>&1
	rc=$?; echo "arm $i $arm rc $rc"; [ $rc -eq 0 ] || FAILED=1; grep -h "\[index\]\|\[query\]" "$OUT/lt-$i-$arm.log"
	kill $N 2>/dev/null; wait $N 2>/dev/null
done
unset LS_HEAVY_SHARE
rm -rf "$OUT/lt.data"
python3 - "$OUT" $# <<'PY' || FAILED=1
import glob, json, os, re, statistics as st, sys
arms = {}
for f in glob.glob(os.path.join(sys.argv[1], "lt-*.json")):
    m = re.search(r"lt-(\d+)-([^.]+)\.json$", f)
    d = json.load(open(f))
    arms.setdefault(m[2], []).append((d["index_phase"]["docs_per_sec"], d["query_phase"]["qps"]))
if sum(len(v) for v in arms.values()) != int(sys.argv[2]):
    sys.exit("missing arms: a run failed")
for a, v in arms.items():
    print(f"{a}: RESTORE docs/s {[round(x[0]) for x in v]} median {st.median(x[0] for x in v):.0f}; qps {[round(x[1]) for x in v]} median {st.median(x[1] for x in v):.0f}")
PY
exit $FAILED
