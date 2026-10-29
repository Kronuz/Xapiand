#!/usr/bin/env bash
# restore_schema_check.sh BIN: on each engine, PUT an index with a foreign _schema (the Twitter one) and RESTORE two
# documents into it, N times (default 5); every document must index. Exit 0 when all do.
BIN=$1; N=${N:-5}; PORT=${PORT:-18810}; rc=0
S='{"_schema":{"_type":"foreign/object","_foreign":".schemas/00000000-0000-1000-8000-010000000000","_meta":{"description":"Twitter Schema"},"_id":{"_type":"integer"},"user":{"_type":"keyword"},"postDate":{"_type":"datetime"},"message":{"_type":"text"}}}'
DOCS='[{"user":"Kronuz","postDate":"2016-11-15T13:12:00","message":"Trying out Xapiand","_id":1},{"user":"Kronuz","postDate":"2016-10-15T10:31:18","message":"Another tweet","_id":2}]'
for ENG in strata; do
  D=$(mktemp -d "${TMPDIR:-/tmp}/restore_schema.XXXXXX")
  "$BIN" --port $PORT --name rs -D "$D/n" >"$D/log" 2>&1 & P=$!
  for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
  bad=0
  for i in $(seq $N); do
    curl -s -XPUT "localhost:$PORT/tw$i/tweets/" -H 'Content-Type: application/json' -d "$S" -o /dev/null
    out=$(curl -s -XRESTORE "localhost:$PORT/tw$i/tweets/" -H 'Content-Type: application/json' -d "$DOCS")
    case "$out" in *'"indexed":2'*) ;; *) bad=$((bad+1)); echo "$ENG tw$i: $out";; esac
  done
  echo "$ENG: $bad of $N restores lost a document"
  [ $bad -eq 0 ] || rc=1
  kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"
done
exit $rc
