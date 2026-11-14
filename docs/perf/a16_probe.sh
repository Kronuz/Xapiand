#!/bin/bash
# a16_probe.sh BIN HARNESS PORT: the A16 gate (2,500 indexes, 3 searches) with `sample` over the timed searches,
# then the same with --database-pool-size 5000 (retention not capped at 200). Prints the top-of-stack summary.
BIN=$1; H=$2; PORT=$3; O=$JOB_OUT
echo "binary $("$BIN" --version 2>&1 | head -1)"
python3 "$H" gate "$BIN" "$O/a" "$PORT" --indexes 2500 --runs 3 > "$O/a.txt" 2>&1 &
g=$!
until grep -q "open indexes" "$O/a.txt" 2>/dev/null || ! kill -0 $g 2>/dev/null; do sleep 1; done
pid=$(pgrep -f "^$BIN .*-D $O/a/data" | head -1)
[ -n "$pid" ] && sample "$pid" 15 1 -mayDie -file "$O/sample.txt" > /dev/null 2>&1
wait $g; echo "arm default rc $?"; cat "$O/a.txt"
if [ -f "$O/sample.txt" ]; then
  echo "== sample, top of stack"; sed -n '/Sort by top of stack/,/^$/p' "$O/sample.txt" | head -25
  echo "== frames naming a sync, in the call graph (max count per name)"
  grep -oE "^[ +!:|]*[0-9]+ [^ ]+.*(fcntl|fsync|sync_files_and_directory|sync_directory|open_existing|basic_index[^ ]*open)[^ ]*" "$O/sample.txt" | sed -E 's/^[ +!:|]*//' | sort -k2 | awk '{n=$1; $1=""; if (n>m[$0]) m[$0]=n} END {for (k in m) print m[k], k}' | sort -rn | cut -c1-200 | head -12
fi
rm -rf "$O/a/data"
printf '#!/bin/sh\nexec "%s" --database-pool-size 5000 "$@"\n' "$BIN" > "$O/pool5000.sh"; chmod +x "$O/pool5000.sh"
python3 "$H" gate "$O/pool5000.sh" "$O/b" "$((PORT + 2))" --indexes 2500 --runs 3; echo "arm pool5000 rc $?"
rm -rf "$O/b/data"
