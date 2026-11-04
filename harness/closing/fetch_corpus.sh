#!/usr/bin/env bash
# Fetch a closing-benchmark corpus on the machine that uses it (D3), check the download's sha256 against corpora.pins
# and the document count, and leave ndjson files under $CLOSING_DATA/corpora (default ~/data/corpora, outside ~/work).
#
#   fetch_corpus.sh logs        C-logs: logs.ndjson (documents-181998 then -191998, 12,406,628 lines), logs-1M.ndjson
#   fetch_corpus.sh logs-writes C-logs writes: logs-writes.ndjson (documents-201998, 13,053,463 lines)
#   fetch_corpus.sh wiki        C-wiki: wiki.ndjson (5,032,104 lines), wiki-500k.ndjson
#   options: --limit N  stop after N lines of each output (a sample; the sha256 and count are then not checked)
#            --record   a download whose pin is "-" is accepted and printed as a pins line
# A finished file is kept; a rerun skips it. A sample goes to corpora/sample-N/ (wiki is streamed, not stored whole).
set -euo pipefail

here=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
data=${CLOSING_DATA:-$HOME/data}/corpora
limit=0 record=0 what=""
while [ $# -gt 0 ]; do
	case $1 in
	--limit) limit=${2:?N}; shift ;;
	--record) record=1 ;;
	-*) echo "unknown option $1" >&2; exit 2 ;;
	*) what=$1 ;;
	esac
	shift
done
[ -n "$what" ] || { sed -n '2,10p' "${BASH_SOURCE[0]}" >&2; exit 2; }
mkdir -p "$data/dl"
out=$data
if [ "$limit" -gt 0 ]; then out=$data/sample-$limit; mkdir -p "$out"; fi

sha256() { if command -v sha256sum >/dev/null; then sha256sum "$@"; else shasum -a 256 "$@"; fi; }
pin() { awk -v n="$1" '$1 == n { print; found = 1 } END { exit !found }' "$here/corpora.pins"; }

# download NAME: the pinned file in dl/, verified; prints its path
download() {
	local url want path got
	read -r _ url want _ < <(pin "$1") || { echo "no pin for $1" >&2; exit 1; }
	path=$data/dl/$(basename "${url%%\?*}")
	if [ ! -s "$path" ]; then
		curl -fsSL --retry 3 -o "$path.part" "$url"
		mv "$path.part" "$path"
	fi
	got=$(sha256 "$path" | cut -d' ' -f1)
	if [ "$want" = - ]; then
		[ "$record" = 1 ] || { echo "$1 is not pinned (sha256 $got); rerun with --record to accept it" >&2; exit 1; }
		echo "pin: $1 $url $got $(wc -c <"$path" | tr -d ' ')" >&2
	elif [ "$got" != "$want" ]; then
		echo "$1: sha256 $got, pinned $want" >&2
		exit 1
	fi
	echo "$path"
}

# emit PATH...: decompressed lines of the files in order, cut at $limit when set
emit() { if [ "$limit" -gt 0 ]; then (set +o pipefail; bzip2 -dc "$@" | head -n "$limit"); else bzip2 -dc "$@"; fi; }

# done_file OUT EXPECTED: rename OUT.part to OUT after checking its line count (unless a sample)
finish() {
	local out=$1 want=$2 n
	n=$(wc -l <"$out.part" | tr -d ' ')
	if [ "$limit" -eq 0 ] && [ "$want" != - ] && [ "$n" != "$want" ]; then
		echo "$out: $n lines, expected $want" >&2
		exit 1
	fi
	mv "$out.part" "$out"
	echo "$out: $n lines, sha256 $(sha256 "$out" | cut -d' ' -f1)"
}

case $what in
logs)
	[ -s "$out/logs.ndjson" ] || {
		a=$(download logs-181998) b=$(download logs-191998)
		emit "$a" "$b" >"$out/logs.ndjson.part"
		finish "$out/logs.ndjson" 12406628
	}
	[ -s "$out/logs-1M.ndjson" ] || {
		head -n 1000000 "$out/logs.ndjson" >"$out/logs-1M.ndjson.part"
		finish "$out/logs-1M.ndjson" 1000000
	}
	;;
logs-writes)
	[ -s "$out/logs-writes.ndjson" ] || {
		emit "$(download logs-201998)" >"$out/logs-writes.ndjson.part"
		finish "$out/logs-writes.ndjson" 13053463
	}
	;;
wiki)
	[ -s "$out/wiki.ndjson" ] || {
		if [ "$limit" -gt 0 ]; then
			read -r _ url _ <<<"$(pin wiki-articles)"
			# curl is cut off by head: a broken pipe is expected here
			(set +o pipefail; curl -fsSL "$url" 2>/dev/null | bzip2 -dc | head -n "$limit" | python3 -I "$here/wiki_transform.py") >"$out/wiki.ndjson.part"
			finish "$out/wiki.ndjson" -
		else
			emit "$(download wiki-articles)" | python3 -I "$here/wiki_transform.py" >"$out/wiki.ndjson.part"
			finish "$out/wiki.ndjson" "$(pin wiki-articles | awk '{print $5}')"
		fi
	}
	[ -s "$out/wiki-500k.ndjson" ] || {
		head -n 500000 "$out/wiki.ndjson" >"$out/wiki-500k.ndjson.part"
		finish "$out/wiki-500k.ndjson" -
	}
	;;
*) echo "unknown corpus $what" >&2; exit 2 ;;
esac
