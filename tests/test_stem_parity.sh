#!/usr/bin/env bash
# release-rows' stems row (prospect docs/gates/release-rows.md A4) at this tree: every stem_language
# name it checked either is refused by the schema, or stems through the snowball stemmer the schema
# stores for it equal to Xapian 2.1.0 (the oracle's `stem`), word for word.
#   test_stem_parity.sh BUILD   BUILD has xapiand_test_strict_text, whose "stem_language NAME
#                               stemmer STORED" lines are the names the schema accepts
# Inputs: snowball-data's vocabularies ($STEM_WORDS), strata-oracle's `oracle` ($ORACLE) and
# prospect's stem_parity ($STEM_PARITY, built by release-rows' `rows.sh build`).
set -euo pipefail
build=${1:?usage: test_stem_parity.sh BUILD}
test=$(find "$build" -name xapiand_test_strict_text -type f -perm -u+x | head -1)
[ -n "$test" ] || { echo "ASSERT: no xapiand_test_strict_text under $build" >&2; exit 1; }
words=${STEM_WORDS:-$HOME/code/Kronuz/snowball/.scratch/oracle-tsv}
oracle=${ORACLE:-$HOME/code/Kronuz/strata-oracle/vectors/.scratch/oracle}
parity=${STEM_PARITY:-$HOME/.scratch/release-rows/2cc69d4d1e91/stem_parity}
out=${JOB_OUT:-.scratch/stem-parity}
mkdir -p "$out"
"$test" > "$out/accepted.txt" || true
grep -q '^stem_language ' "$out/accepted.txt" || { echo "ASSERT: $test listed no stem languages" >&2; exit 1; }
names="armenian:armenian hy:armenian basque:basque eu:basque catalan:catalan ca:catalan danish:danish da:danish
	dutch:dutch nl:dutch kraaij_pohlmann:dutch english:english en:english earlyenglish:earlymodernenglish
	english_lovins:lovins lovins:lovins english_porter:porter porter:porter finnish:finnish fi:finnish
	french:french fr:french german:german de:german german2:german hungarian:hungarian hu:hungarian
	italian:italian it:italian norwegian:norwegian nb:norwegian nn:norwegian no:norwegian
	portuguese:portuguese pt:portuguese romanian:romanian ro:romanian russian:russian ru:russian
	spanish:spanish es:spanish swedish:swedish sv:swedish turkish:turkish tr:turkish"
rc=0 served=0 refused=0
for x in $names; do
	name=${x%%:*} voc=${x#*:}
	if ! grep -q "^stem_language $name stemmer " "$out/accepted.txt"; then
		echo "$name: refused by the schema"
		refused=$((refused + 1))
		continue
	fi
	stemmer=$(awk -v n="$name" '$1 == "stem_language" && $2 == n { print $4 }' "$out/accepted.txt")
	cut -f1 "$words/$voc.tsv" > "$out/words-$voc"
	if ! "$oracle" stem "$name" "$out/words-$voc" > "$out/xapian-$name.jsonl" 2> "$out/xapian-$name.err"; then
		echo "DIFF $name: the schema serves it as $stemmer, Xapian refuses it ($(head -c 160 "$out/xapian-$name.err"))"
		rc=1
		continue
	fi
	echo -n "$name -> "
	"$parity" "$stemmer" < "$out/xapian-$name.jsonl" | tail -11 || rc=1
	served=$((served + 1))
done
echo "$served names served, $refused refused"
[ "$rc" = 0 ] || { echo "ASSERT: a served name differs from Xapian" >&2; exit 1; }
