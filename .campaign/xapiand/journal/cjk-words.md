# cjk-words: journal

State: waiting on jobs 261120-180508-3004d5 261120-180509-eba9f4 261120-180518-6b069d 261120-180527-da1cd7 (round 1 fixes at bf63b5546)
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

1. Read the jobs at bf63b5546: build-test 261120-180508-3004d5, e2e 261120-180509-eba9f4,
   F1 confirmation (reviewer's probe2.sh rerun, same command) 261120-180518-6b069d, revert check
   of the new test (`--fails-without`, text.cc at 02e890123) 261120-180527-da1cd7.

## Blocked on

F3 is outside my file set: `docs/src/content/docs/upgrade.md:80` and `changelog.md:316` still say
`_cjk_words` is gone. Recommendation: the lead adds them to the file set, or edits them at landing
(it is back, on analysis's segmenter, no ICU; new text fields store `_cjk_words: false`).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261120-155852-224613 (79 of 79), e2e 261120-155853-9d2286 |
| A2 | build-test 261120-155852-224613 (tests/test_cjk_words.cc); ICU oracle 261120-155914-5a93cf: 0 differences on zh, ja, ko; rerun from tests/test_cjk_words_oracle.sh: 261120-173321-093c44 |
| A3 | docs/mechanisms/cjk-words.md (Differences filled from the oracle) |

## Rulings I made

- Both modes exclusive: setting `_cjk_words` and `_cjk_ngram` together is a ClientError.
- Query side splits the text before queryparser (no queryparser change; its pin is not mine).
- `GenerateTerms::text` takes `cjk_words` as a trailing default so existing callers stand.
- F1 fix uses `analysis::detail::infix_of` rather than a copied list, so the set stays analysis's
  one definition; numeric infixes join only digits and never CJK, so they are left alone.
  Ignorables (ZWJ, U+2060, ...) beside CJK are not spaced: not in the review's set, rare.
