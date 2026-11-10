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

# gate7c_rows.sh REL ASAN OUT: Phase 7c's functional gate rows (docs/design/phase7c.md section 4) on a Release and an
# ASan+UBSan binary. Each row runs to its end; OUT/rows.tsv gets "row gating rc seconds" and OUT/<row>.log its output.
# Rows marked info are reported, not gated. Exit 0 when every gating row passes. No timing row runs here.
# GATE7C_ALLOW overrides T1's thread prefixes (macOS names: HTTP loops are `unnamed`, the main thread `main`).
. "$(dirname "$0")/env.sh"
REL=$1; ASAN=$2; OUT=$3
H="$(cd "$(dirname "$0")" && pwd)"; REPO="$(dirname "$H")"
ALLOW=${GATE7C_ALLOW:-main,unnamed,Xapiand:W,Xapiand:LOG,Xapiand:WL,Xapiand:FS}
mkdir -p "$OUT"; printf 'row\tgating\trc\tseconds\n' >"$OUT/rows.tsv"; failed=0

row() {  # row NAME gate|info CMD...
	local n=$1 g=$2 s rc; shift 2; s=$(date +%s)
	"$@" >"$OUT/$n.log" 2>&1; rc=$?
	printf '%s\t%s\t%s\t%s\n' "$n" "$g" "$rc" "$(( $(date +%s) - s ))" | tee -a "$OUT/rows.tsv"
	[ "$rc" = 0 ] || [ "$g" = info ] || failed=1
}

# check_json KIND FILE: T1's census holds only ALLOW's prefixes (and as many `unnamed` as reactors on macOS); an
# overload phase answered at least one 503 and no other error.
check_json() {
	"$PY" -I - "$1" "$2" "$ALLOW" <<'PY'
import json, sys
kind, path, allow = sys.argv[1], sys.argv[2], set(sys.argv[3].split(","))
out = json.load(open(path)); bad = []
for p in out["phases"]:
    if kind == "census":
        c = p.get("census") or {}
        threads = c.get("threads") or {}
        print(p["name"], "census", json.dumps(threads), "total", c.get("total"), "reactors", out["meta"]["reactors"])
        if not threads: bad.append("no census")
        bad += [f"offender {t}" for t in threads if t not in allow]
        if sys.platform == "darwin" and threads.get("unnamed", 0) != out["meta"]["reactors"]:
            bad.append(f"unnamed {threads.get('unnamed', 0)} != reactors {out['meta']['reactors']}")
    elif p["name"].startswith("overload"):
        es = [(p.get(k) or {}).get("errors", {}) for k in ("light", "heavy")]
        print(p["name"], "errors", json.dumps(es), "peaks", json.dumps(p["peaks"]), "rss_kb", p["rss_kb"])
        if not sum(e.get("http_503", 0) for e in es): bad.append(f"{p['name']}: no 503")
        if any(e.get("http_other") or e.get("exceptions") for e in es): bad.append(f"{p['name']}: other errors")
print("FAIL" if bad else "PASS", bad)
sys.exit(1 if bad else 0)
PY
}

stall() {  # stall NAME ARGS...: stall_load.py on REL at a free port, then its check
	local n=$1; shift
	"$PY" "$H/stall_load.py" --bin "$REL" --target "127.0.0.1:$(free_port)" --workdir "$OUT/$n.d" \
		--dry-run --out "$OUT/$n.json" "$@" && rm -rf "$OUT/$n.d" && check_json "$( [ "$n" = t1_census ] && echo census || echo overload )" "$OUT/$n.json"
}

row t1_strict_owed gate "$H/async_check.sh" --strict --owed --extern "strata=$REPO/.deps/strata-src"
row t1_census gate stall t1_census --census --phase loaded
row t2_e2e_asan gate env PORT="$(free_port)" "$H/engine_e2e.sh" "$ASAN" "$OUT/e2e-asan"
row t2_stored_asan gate env PORT="$(free_port)" "$H/stored_stream.sh" "$ASAN"
row t2_restore_stream_asan info env PORT="$(free_port)" "$H/restore_stream_check.sh" "$ASAN"
row t3_crash gate env BIN="$REL" PORT="$(free_port)" "$H/engine_crash_check.sh" --reactors 2
row t3_crash_asan gate env BIN="$ASAN" PORT="$(free_port)" "$H/engine_crash_check.sh" --reactors 2
row t7_asan gate "$PY" "$H/engine_mixed_load.py" "$ASAN" "$OUT/t7-asan" --engine strata --secs 600 --term-at 300 --port "$(free_port)"
row t8_overload_light gate stall t8_overload_light --overload light 4
row t8_overload_heavy gate stall t8_overload_heavy --overload heavy 4
row t10_lua gate "$PY" "$H/lua_bounds.py" "$REL" --port "$(free_port)" --loops 4 --out "$OUT/t10.json"
row t10_lua_asan info "$PY" "$H/lua_bounds.py" "$ASAN" --port "$(free_port)" --loops 4 --out "$OUT/t10-asan.json"
row x6_shutdown gate env BIN="$REL" PORT="$(free_port)" "$H/graceful_shutdown_check.sh"
row gate_tools gate "$PY" "$H/test_gate_tools.py"
echo "gate7c_rows: $( [ "$failed" = 0 ] && echo PASS || echo FAIL ) ($OUT/rows.tsv)"
exit "$failed"
