# cjk-words: journal

State: waiting on jobs 261120-182340-d04e6e 261120-182351-be88ad 261120-182354-5059f0 and CI at 0505cd7cd
Quest: .campaign/xapiand/quests/cjk-words.md (the lead's)

## Doing

Review round 1 (`.campaign/xapiand/reviews/cjk-words-correctness.md`):

- F1 (blocking): `text::cjk_words` sets a letter infix (`analysis::detail::infix_of`, kind letter:
  `&`, `'`, U+2019, U+201B, U+00B7, U+05F4, U+2027, U+00AD) beside a CJK character apart with a
  space on each side. Test `test_infix_splits_cjk_words` (zh, ja, ko, CJK/Latin both ways; U+30FB
  and Latin controls); docs example searches `body:华盛顿` in `乔治·华盛顿`.
- F2: `test_schema_property` (both flags refused; stored; does not change).
- F3: not mine (upgrade.md, changelog.md outside the file set); see Blocked on.
- F4: left (as the review allows).
- F5: `fst` pinned at analysis's commit in CMakeLists.txt; dictionary size in the note.
- F6: the note says a word search is as good as the dictionary; mark handling left.

## Next

1. Read the jobs at 0505cd7cd: targeted build of `xapiand` and `test_cjk_words` plus its run
   261120-182340-d04e6e; F1 confirmation (reviewer's probe2.sh, same command, on that binary)
   261120-182351-be88ad; revert check (`--fails-without`, text.cc at 02e890123)
   261120-182354-5059f0. The suite is CI's (D57): `ci:Build + unit + smoke`,
   `ci:Functional E2E (docs)`. Local build-test/e2e at bf63b5546 were cancelled by the lead.
2. CI at bf63b5546 failed test_cjk_words:169 (arm64, FreeBSD): `Schema::write` replaces the whole
   schema, so the second write was no change. Fixed in 0505cd7cd (the change goes through
   `Schema::update` and must throw ClientError naming `_cjk_words`).

## Blocked on

F3 is outside my file set: `docs/src/content/docs/upgrade.md:80` and `changelog.md:316` still say
`_cjk_words` is gone. Recommendation: the lead adds them to the file set, or edits them at landing
(it is back, on analysis's segmenter, no ICU; new text fields store `_cjk_words: false`).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `ci:Build + unit + smoke` and `ci:Functional E2E (docs)` at 0505cd7cd (pending); 261120-182340-d04e6e |
| A2 | `ci:Build + unit + smoke` (tests/test_cjk_words.cc); 261120-182340-d04e6e; fails without the F1 fix: 261120-182354-5059f0; ICU oracle 261120-173321-093c44: 0 differences on zh, ja, ko |
| A3 | docs/mechanisms/cjk-words.md (Differences filled from the oracle) |
| F1 | reviewer's probe2.sh rerun: 261120-182351-be88ad |

## Rulings I made

- Both modes exclusive: setting `_cjk_words` and `_cjk_ngram` together is a ClientError.
- Query side splits the text before queryparser (no queryparser change; its pin is not mine).
- `GenerateTerms::text` takes `cjk_words` as a trailing default so existing callers stand.
- F1 fix uses `analysis::detail::infix_of` rather than a copied list, so the set stays analysis's
  one definition; numeric infixes join only digits and never CJK, so they are left alone.
  Ignorables (ZWJ, U+2060, ...) beside CJK are not spaced: not in the review's set, rare.
