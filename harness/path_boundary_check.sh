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

# path_boundary_check.sh BIN: an empty data directory, and an index path that is empty or normalizes outside
# or onto the data directory (relative, or absolute in any spelling), or that holds a byte a name cannot hold (NUL, a
# control byte, a backslash, malformed UTF-8), are refused: 400 from the API, a usage error (64) from the CLI. Nothing
# is created, read or deleted outside the data directory. The node's own routes (GET /, HEAD /, GET /:info,
# GET /:metrics, GET /.name) name no index and answer 200. Exit 0 when all hold.
BIN=$1; PORT=${PORT:-18837}; rc=0
# D is absolute: the check spells "$D/out" as an absolute path, which a relative D (TMPDIR unset) would turn into
# an index path inside the data directory.
W=$(cd "${HARNESS_TMP:-${TMPDIR:-.}}" && pwd -P) || exit 2
D=$(mktemp -d "$W/path_boundary.XXXXXX")
trap 'kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
fail() { echo "FAIL: $*"; rc=1; }

# CLI: the data directory must be named, and a dump or restore names an index inside it.
"$BIN" --port $PORT --name pb -D "" >"$D/empty.log" 2>&1; c=$?
[ $c = 64 ] && echo "ok   -D '' exits 64" || fail "-D '' exited $c (want 64)"
for name in . ./ a/.. .. ../x a/../../x "$D/out" "$D/out/" /etc/x //etc/x; do
	"$BIN" --port $PORT --name pb -D "$D/cli" --dump "$name" --out "$D/out.mp" >"$D/dump.log" 2>&1; c=$?
	[ $c = 64 ] && echo "ok   --dump '$name' exits 64" || fail "--dump '$name' exited $c (want 64)"
	"$BIN" --port $PORT --name pb -D "$D/cli" --restore "$name" --in /dev/null >"$D/restore.log" 2>&1; c=$?
	[ $c = 64 ] && echo "ok   --restore '$name' exits 64" || fail "--restore '$name' exited $c (want 64)"
done

# API: an index path that is empty or names the data directory answers 400 (the router already folds a plain
# ".." into the base; an encoded one reaches the Endpoint). A path that stays inside is served, nothing lands outside.
"$BIN" --port $PORT --name pb -D "$D/n" >"$D/log" 2>&1 & P=$!
for _ in $(seq 60); do curl -s -m2 -o /dev/null localhost:$PORT/ && break; sleep .5; done
for path in .. a/.. a/b/../.. %2e%2e a/%2e%2e a/%2e%2e/%2e%2e/x; do
	for method in PUT GET; do
		code=$(curl -s -m 10 --path-as-is -o /dev/null -w '%{http_code}' -X $method "localhost:$PORT/$path/doc" -H 'Content-Type: application/json' -d '{"a":1}')
		[ "$code" = 400 ] && echo "ok   $method /$path/doc -> 400" || fail "$method /$path/doc -> $code (want 400)"
	done
done
for path in ../x a/../../y ok/../fine; do
	code=$(curl -s -m 10 --path-as-is -o /dev/null -w '%{http_code}' -X PUT "localhost:$PORT/$path/doc" -H 'Content-Type: application/json' -d '{"a":1}')
	case $code in 2??) echo "ok   PUT /$path/doc -> $code";; *) fail "PUT /$path/doc -> $code (want 2xx)";; esac
done
# An absolute index path is outside the data directory: every spelling that reaches the Endpoint as one (an encoded
# slash first) answers 400, a leading "//" is folded into the data directory by the router, and nothing is ever
# created, read or deleted outside it, nor linked as a foreign schema.
OUT="$D/out"; mkdir -p "$OUT"; echo '{"a":1}' > "$OUT/keep"
ENC=${OUT//\//%2F}
for url in "/$ENC" "/%2F${OUT#/}" "/%2Fetc%2Ffoo" "/$ENC%2F..%2Fout" "/$ENC/%2e%2e/out" "/%2e%2e/$ENC" "/a/$ENC"; do
	for method in PUT GET DELETE; do
		code=$(curl -s -m 10 --path-as-is -o /dev/null -w '%{http_code}' -X $method "localhost:$PORT$url/doc" -H 'Content-Type: application/json' -d '{"a":1}')
		case $url in
		/a/*) case $code in 2??|404) echo "ok   $method $url/doc -> $code (inside)";; *) fail "$method $url/doc -> $code (want 2xx or 404)";; esac;;
		*) [ "$code" = 400 ] && echo "ok   $method $url/doc -> 400" || fail "$method $url/doc -> $code (want 400)";;
		esac
	done
done
for url in "/$OUT" "//$OUT" "/./$OUT" "/$OUT/.." "//etc/foo"; do
	for method in PUT GET DELETE; do
		code=$(curl -s -m 10 --path-as-is -o /dev/null -w '%{http_code}' -X $method "localhost:$PORT$url/doc" -H 'Content-Type: application/json' -d '{"a":1}')
		case $code in 5??) fail "$method $url/doc -> $code";; *) echo "ok   $method $url/doc -> $code (folded into the data directory)";; esac
	done
done
curl -s -m 10 -o /dev/null -X PUT "localhost:$PORT/linked/"
code=$(curl -s -m 10 -o /dev/null -w '%{http_code}' -X PUT "localhost:$PORT/linked/" -H 'Content-Type: application/json' \
	-d "{\"_schema\":{\"_type\":\"foreign/object\",\"_foreign\":\"$OUT/sch\"}}")
code2=$(curl -s -m 10 -o /dev/null -w '%{http_code}' -X PUT "localhost:$PORT/linked/d1" -H 'Content-Type: application/json' -d '{"a":1}')
[ "$code2" = 400 ] && echo "ok   a foreign schema in an absolute path: link $code, first write 400" || fail "a foreign schema in an absolute path: link $code, first write $code2 (want 400)"
[ "$(ls -A "$OUT")" = keep ] && echo "ok   nothing appeared outside the data directory" || fail "the outside directory holds: $(ls -A "$OUT" | tr '\n' ' ')"
[ -e "$OUT/keep" ] || fail "an outside file was deleted"
# SR1: a NUL is one byte of a name to the boundary check ("..%00" is three bytes) but ends the string for the system
# calls, which would act on ".." one level above the data directory. It is refused with the other bytes a name
# cannot hold, on the index route and in a foreign schema (spelled in the URL or as a JSON \u0000).
before=$(ls -A "$D" | sort)
for path in ..%00 a/..%00 ..%00/a %00 a%00b ..%00..%00 a%5Cb ..%5Cx a%01b a%7Fb %c0%af %c0%ae%c0%ae a%e2%82 %ed%a0%80; do
	for method in PUT GET DELETE; do
		code=$(curl -s -m 10 --path-as-is -o /dev/null -w '%{http_code}' -X $method "localhost:$PORT/$path/doc" -H 'Content-Type: application/json' -d '{"a":1}')
		[ "$code" = 400 ] && echo "ok   $method /$path/doc -> 400" || fail "$method /$path/doc -> $code (want 400)"
	done
done
for name in "caf%c3%a9" "%ef%bc%8f%ef%bc%8e" "%f0%9f%98%80"; do
	code=$(curl -s -m 10 --path-as-is -o /dev/null -w '%{http_code}' -X PUT "localhost:$PORT/$name/doc" -H 'Content-Type: application/json' -d '{"a":1}')
	case $code in 2??) echo "ok   PUT /$name/doc -> $code (a UTF-8 name)";; *) fail "PUT /$name/doc -> $code (want 2xx)";; esac
done
for spelling in '..\u0000/sch' '%2e%2e%00/sch' 'a\u0000b/sch' 'a\\b/sch'; do
	curl -s -m 10 -o /dev/null -X PUT "localhost:$PORT/nulink/"
	curl -s -m 10 -o /dev/null -X PUT "localhost:$PORT/nulink/" -H 'Content-Type: application/json' \
		-d "{\"_schema\":{\"_type\":\"foreign/object\",\"_foreign\":\"$spelling\"}}"
	code=$(curl -s -m 10 -o /dev/null -w '%{http_code}' -X PUT "localhost:$PORT/nulink/d1" -H 'Content-Type: application/json' -d '{"a":1}')
	[ "$code" = 400 ] && echo "ok   a foreign schema '$spelling': first write 400" || fail "a foreign schema '$spelling': first write $code (want 400)"
	curl -s -m 10 -o /dev/null -X DELETE "localhost:$PORT/nulink/"
done
after=$(ls -A "$D" | sort)
[ "$before" = "$after" ] && echo "ok   nothing appeared beside the data directory" || fail "the directory above the data directory changed: $(echo "$before" | tr '\n' ' ') -> $(echo "$after" | tr '\n' ' ')"
for name in 'a\b' $'a\x01b' '..\x'; do
	"$BIN" --port $PORT --name pb -D "$D/cli" --dump "$name" --out "$D/out.mp" >"$D/dump.log" 2>&1; c=$?
	[ $c = 64 ] && echo "ok   --dump '$name' exits 64" || fail "--dump '$name' exited $c (want 64)"
	"$BIN" --port $PORT --name pb -D "$D/cli" --restore "$name" --in /dev/null >"$D/restore.log" 2>&1; c=$?
	[ $c = 64 ] && echo "ok   --restore '$name' exits 64" || fail "--restore '$name' exited $c (want 64)"
done

# The node's own routes name no index (the base itself is not one): they answer, and a verb that needs an index is a 400.
for req in "GET /" "HEAD /" "GET /:info" "GET /:metrics" "GET /.name" "GET /.state"; do
	set -- $req
	if [ $1 = HEAD ]; then code=$(curl -s -m 10 -I -o /dev/null -w '%{http_code}' "localhost:$PORT$2")
	else code=$(curl -s -m 10 -o /dev/null -w '%{http_code}' "localhost:$PORT$2"); fi
	[ "$code" = 200 ] && echo "ok   $req -> 200" || fail "$req -> $code (want 200)"
done
body=$(curl -s -m 10 "localhost:$PORT/"); case $body in *'"versions"'*'"server"'*|*'"server"'*'"versions"'*) echo "ok   GET / names the server and its versions";; *) fail "GET / body: $body";; esac
for req in "PUT /" "POST /"; do
	set -- $req
	code=$(curl -s -m 10 -o /dev/null -w '%{http_code}' -X $1 "localhost:$PORT$2" -H 'Content-Type: application/json' -d '{"a":1}')
	[ "$code" = 400 ] && echo "ok   $req -> 400 (no index named)" || fail "$req -> $code (want 400)"
done
code=$(curl -s -m 10 -o /dev/null -w '%{http_code}' -X DELETE "localhost:$PORT/")
case $code in 400|405) echo "ok   DELETE / -> $code";; *) fail "DELETE / -> $code (want 400 or 405)";; esac
for name in x y fine; do
	[ -d "$D/n/$name" ] && echo "ok   index $name is inside the data directory" || fail "index $name is not inside the data directory"
	[ -e "$D/$name" ] && fail "$name was created outside the data directory"
done
exit $rc
