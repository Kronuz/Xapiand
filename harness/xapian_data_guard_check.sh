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

# xapian_data_guard_check.sh BIN: a data directory that holds Xapian files (the registry shard, or an index's shard
# directory; iamglass, flintlock, postlist.*, *.glass) makes the node exit 65 (EX_DATAERR) naming the first such path and
# the upgrade page, before it creates anything; a clean directory starts and answers.
BIN=$1; PORT=${PORT:-18830}; D=$(mktemp -d "${TMPDIR:-/tmp}/xguard.XXXXXX"); rc=0
chk() { if [ "$2" = "$3" ]; then echo "ok   $1"; else echo "FAIL $1: got $3, want $2"; rc=1; fi; }
plant() { mkdir -p "$D/$1/$(dirname "$2")"; : > "$D/$1/$2"; }
refuse() { # NAME RELPATH: plant the file, expect exit 65 and an error naming it
  plant "$1" "$2"
  timeout 30 "$BIN" --port $PORT --name xg -D "$D/$1" >"$D/$1.log" 2>&1; local code=$?
  chk "$1: exit code" 65 "$code"
  if grep -q "Xapian index: .*$2" "$D/$1.log" && grep -q "upgrading-from-100-alpha9" "$D/$1.log"; then echo "ok   $1: names $2 and the upgrade page"; else echo "FAIL $1: message"; head -5 "$D/$1.log"; rc=1; fi
  if [ -n "$(find "$D/$1" -name strata -type d)" ]; then echo "FAIL $1: created strata files"; rc=1; fi
}
refuse registry .xapiand/indexes/.__1/iamglass
refuse registry_postlist .xapiand/indexes/.__2/postlist.glass
refuse index_shard bank/.__1/flintlock
refuse index_unsharded solo/record.glass
# control: a clean directory (with strata's own files) starts and answers
mkdir -p "$D/clean"
"$BIN" --port $PORT --name xg -D "$D/clean" >"$D/clean.log" 2>&1 & P=$!
for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
chk "clean: answers" 200 "$(curl -s -o /dev/null -w '%{http_code}' -m5 localhost:$PORT/)"
curl -s -o /dev/null -XPUT localhost:$PORT/x/1 -H 'Content-Type: application/json' -d '{"a":1}'
kill $P 2>/dev/null; wait $P 2>/dev/null
# the restart on that directory (now holding strata's files) is not refused
"$BIN" --port $PORT --name xg -D "$D/clean" >"$D/clean2.log" 2>&1 & P=$!
for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
chk "clean restart: answers" 200 "$(curl -s -o /dev/null -w '%{http_code}' -m5 localhost:$PORT/)"
kill $P 2>/dev/null; wait $P 2>/dev/null
rm -rf "$D"; exit $rc
