#!/usr/bin/env bash
# path_boundary_check.sh BIN: an empty data directory, and an index path that is empty or normalizes outside
# or onto the data directory, are refused: 400 from the API, a usage error (64) from the CLI. Exit 0 when all hold.
BIN=$1; PORT=${PORT:-18837}; rc=0
W=${HARNESS_TMP:-${TMPDIR:-.}}
D=$(mktemp -d "$W/path_boundary.XXXXXX")
trap 'kill -9 $P 2>/dev/null; wait $P 2>/dev/null; rm -rf "$D"' EXIT
fail() { echo "FAIL: $*"; rc=1; }

# CLI: the data directory must be named, and a dump or restore names an index inside it.
"$BIN" --port $PORT --name pb -D "" >"$D/empty.log" 2>&1; c=$?
[ $c = 64 ] && echo "ok   -D '' exits 64" || fail "-D '' exited $c (want 64)"
for name in . ./ a/.. .. ../x a/../../x; do
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
for name in x y fine; do
	[ -d "$D/n/$name" ] && echo "ok   index $name is inside the data directory" || fail "index $name is not inside the data directory"
	[ -e "$D/$name" ] && fail "$name was created outside the data directory"
done
exit $rc
