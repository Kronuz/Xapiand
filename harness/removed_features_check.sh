#!/usr/bin/env bash
# removed_features_check.sh BIN: on each engine, the removed features answer as removed: `_elite_set` and `_terms`
# over a text field are 400, a document's INFO has no `terms`, `_terms` over a numeric field still counts, the
# `fuzzy` and `nearest` parameters (and their settings) change nothing, `collapse` still collapses, and the query shapes
# that are not valid queries (a `_synonym` over a wildcard, a fuzzy word or a bracketed group, a `_phrase` or `_near` over a
# wildcard, a fuzzy word or a bracketed group, `_scale_weight` or `_wildcard` given a list) are 400 saying so.
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
  curl -s "$U/bank/" -XSEARCH -G --data-urlencode 'q=personality:kind' -o "$D/plain"
  for PARAMS in 'fuzzy' 'nearest' 'fuzzy=true&nearest=true' 'nearest&nearest.n_rset=3&nearest.n_eset=10&nearest.n_term=2&nearest.field=personality&nearest.type=text'; do
    curl -s "$U/bank/?$PARAMS" -XSEARCH -G --data-urlencode 'q=personality:kind' -o "$D/with"
    if python3 -c 'import json,sys; a,b=[json.load(open(f)) for f in sys.argv[1:]]; sys.exit(0 if a["total"]>0 and a["total"]==b["total"] and [h["_id"] for h in a["hits"]]==[h["_id"] for h in b["hits"]] else 1)' "$D/plain" "$D/with" 2>/dev/null; then echo "ok   $ENG ?$PARAMS changes nothing"; else echo "FAIL $ENG ?$PARAMS changed the answer"; rc=1; fi
  done
  curl -s "$U/bank/?collapse=eyeColor" -XSEARCH -o "$D/out"
  if python3 -c 'import json,sys; d=json.load(open(sys.argv[1])); sys.exit(0 if d["total"]==3 and sorted(h["eyeColor"] for h in d["hits"])==["blue","brown","green"] else 1)' "$D/out" 2>/dev/null; then echo "ok   $ENG ?collapse=eyeColor keeps one hit per color"; else echo "FAIL $ENG ?collapse"; rc=1; fi
  bad() { # NAME BODY: 400 with "not a valid query" in the message
    local c; c=$(code -XSEARCH $U/bank/ -H 'Content-Type: application/json' -d "$2")
    if [ "$c" = 400 ] && grep -q "not a valid query" "$D/out"; then echo "ok   $ENG $1 is 400, not a valid query"; else echo "FAIL $ENG $1: got $c: $(head -c 160 "$D/out")"; rc=1; fi
  }
  bad "_synonym over a wildcard" '{"_query":{"personality":{"_synonym":"hard desc*"}}}'
  bad "_synonym over a fuzzy word" '{"_query":{"personality":{"_synonym":"hard descr~"}}}'
  bad "_synonym over a bracketed group" '{"_query":{"personality":{"_synonym":"hard (to too) describe"}}}'
  bad "_phrase over a bracketed group" '{"_query":{"personality":{"_phrase":"hard (to too) describe"}}}'
  bad "_near over a bracketed group" '{"_query":{"personality":{"_near":"hard (to too) describe"}}}'
  bad "_phrase over a wildcard" '{"_query":{"personality":{"_phrase":"first se*"}}}'
  bad "_near over a fuzzy word" '{"_query":{"personality":{"_near":"first seen~"}}}'
  bad "_scale_weight given a list" '{"_query":{"_scale_weight":[{"personality":"kind"},2]}}'
  bad "_wildcard given a list" '{"_query":{"_wildcard":[{"personality":"kin*"}]}}'
  kill -9 $P 2>/dev/null; wait $P 2>/dev/null
done
rm -rf "$D"; exit $rc
