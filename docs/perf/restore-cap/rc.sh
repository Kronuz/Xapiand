# rc.sh MODE BASE BIN ARM...: restore-cap arms in the given order, run from the job's checkout. ARM "off" is BASE
# (master); "cN" is BIN with XAPIAND_RESTORE_CHORES=N (c0: uncapped); "kN" is BIN with XAPIAND_RESTORE_CHILDREN=N.
# MODE t45: stall_load's T4/T5 mix, 64 heavy clients (loop-scheduling's A2); census: t45 with sampler.py on the
# node; lt: T12's loadtest (A3); layout: stall_load's setup RESTORE of accounts, then layout.py on its segments.
set -e
MODE=$1; BASE=$2; BIN=$3; shift 3; O=$JOB_OUT; A=$(cd "$(dirname "$0")" && pwd)
shasum "$BASE" "$BIN"
sysctl -n hw.ncpu hw.physicalcpu machdep.cpu.brand_string 2>/dev/null || true
for arm in "$@"; do
  if [ "$arm" = off ]; then printf '#!/bin/sh\nexec "%s" "$@"\n' "$BASE" > "$O/$arm.sh"
  elif [ "${arm#k}" != "$arm" ]; then printf '#!/bin/sh\nXAPIAND_RESTORE_CHILDREN=%s exec "%s" "$@"\n' "${arm#k}" "$BIN" > "$O/$arm.sh"
  else printf '#!/bin/sh\nXAPIAND_RESTORE_CHORES=%s exec "%s" "$@"\n' "${arm#c}" "$BIN" > "$O/$arm.sh"; fi
  chmod +x "$O/$arm.sh"
done
i=0
for arm in "$@"; do i=$((i+1))
  if [ "$MODE" = lt ]; then
    D="$O/lt.data"; rm -rf "$D"; mkdir -p "$D"; "$O/$arm.sh" --port 18993 --name bench -D "$D" > "$O/lt-$i-$arm.node.log" 2>&1 & N=$!
    for _ in $(seq 120); do curl -s localhost:18993/ > /dev/null && break; sleep 0.5; done
    python3 harness/loadtest.py --target localhost:18993 --dataset docs/public/assets/accounts.ndjson --replicate 20 --concurrency 16 --duration 8 --trials 1 --datadir "$D" --label $arm --out "$O/lt-$i-$arm.json" > "$O/lt-$i-$arm.log" 2>&1; echo "arm $i $arm rc $?"; grep -h "\[index\]\|\[query\]" "$O/lt-$i-$arm.log"
    kill $N; wait $N || true
    continue
  fi
  if [ "$MODE" = layout ]; then
    D="$O/ly.data"; rm -rf "$D"; mkdir -p "$D"; "$O/$arm.sh" --port 18995 --name layout -D "$D" > "$O/ly-$i-$arm.node.log" 2>&1 & N=$!
    for _ in $(seq 120); do curl -s localhost:18995/ > /dev/null && break; sleep 0.5; done
    python3 "$A/load_accounts.py" 18995
    sleep 8; echo "== arm $i $arm"; python3 "$A/layout.py" "$D"
    kill $N; wait $N || true
    continue
  fi
  S=""
  if [ "$MODE" = census ]; then python3 "$A/sampler.py" "por[t] 18980 --name stall" "$O/t45-$i-$arm.ps.jsonl" 6 & S=$!; fi
  python3 harness/stall_load.py --bin "$O/$arm.sh" --target 127.0.0.1:18980 --workdir "$O/w" --accounts-docs 20000 --phase both --rate 200 --duration 20 --heavy-clients 64 --no-assert --out "$O/t45-$i-$arm.json" 2> "$O/t45-$i-$arm.log" || echo "arm $i $arm rc $?"
  [ -z "$S" ] || { kill $S; wait $S || true; }
done
rm -rf "$O/w" "$O/lt.data" "$O/ly.data"
[ "$MODE" = layout ] || python3 "$A/report.py" "$O"
