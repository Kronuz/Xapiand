#!/usr/bin/env bash
# removed_features_check.sh BIN: on each engine, the removed features answer as removed: `_elite_set` and `_terms`
# over a text field are 400, a document's INFO has no `terms`, and `_terms` over a numeric field still counts.
BIN=$1; PORT=${PORT:-18820}; D=$(mktemp -d "${TMPDIR:-/tmp}/removed.XXXXXX"); HERE="$(cd "$(dirname "$0")/.." && pwd)"; rc=0
for ENG in xapian strata; do
  XAPIAND_ENGINE=$ENG "$BIN" --port $PORT --solo --name rf -D "$D/$ENG" >"$D/$ENG.log" 2>&1 & P=$!
  for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
  U=localhost:$PORT
  curl -s -o /dev/null -XRESTORE "$U/bank/" -H 'Content-Type: application/x-ndjson' --data-binary @"$HERE/docs/public/assets/accounts.ndjson"
  curl -s -o /dev/null -XPOST "$U/bank/:commit"
  code() { curl -s -o "$D/out" -w '%{http_code}' "$@"; }
  chk() { if [ "$2" = "$3" ]; then echo "ok   $ENG $1"; else echo "FAIL $ENG $1: got $3, want $2"; rc=1; fi; }
  chk "_elite_set is 400" 400 "$(code -XSEARCH $U/bank/ -H 'Content-Type: application/json' -d '{"_query":{"personality":{"_elite_set":"lovable cooperative kind"}}}')"
  chk "_terms over text is 400" 400 "$(code -XSEARCH $U/bank/ -H 'Content-Type: application/json' -d '{"_query":"*","_limit":0,"_aggs":{"x":{"_terms":{"_field":"personality"}}}}')"
  chk "_terms over a number is 200" 200 "$(code -XSEARCH $U/bank/ -H 'Content-Type: application/json' -d '{"_query":"*","_limit":0,"_aggs":{"x":{"_terms":{"_field":"age"}}}}')"
  chk "INFO of a document is 200" 200 "$(code -XINFO $U/bank/1)"
  if LC_ALL=C grep -aq "\"terms\"" "$D/out"; then echo "FAIL $ENG INFO still has terms"; rc=1; else echo "ok   $ENG INFO has no terms"; fi
  kill -9 $P 2>/dev/null; wait $P 2>/dev/null
done
rm -rf "$D"; exit $rc
