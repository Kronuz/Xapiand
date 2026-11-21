# cjk-words review: correctness

Commit: Xapiand:02e8901234c3 (quest/xapiand/cjk-words, full: 02e890123)
Verdict: CHANGES
Date: 2026-11-20

CHANGES: one blocking defect, the CJK words joined by an infix character (U+00B7, U+2027, `&`, `'`)
are indexed as one term, so a search by either word misses. Everything else asked holds.

## Evidence I relied on

- build-test 261120-155852-224613 (79 of 79) and e2e 261120-155853-9d2286 ran at 3c6f6ed6b. The
  three commits after it change only the journal, `docs/mechanisms/cjk-words.md` and
  `tests/test_cjk_words_oracle.sh` (`git diff 3c6f6ed6b..02e890123 --stat`), so the code they tested is
  the code at 02e890123. `campaign status` flags them "ran other code" for that reason only; a rerun
  is a formality, not a doubt.
- ICU oracle 261120-173321-093c44 (and 261120-155914-5a93cf): 0 differences on zh, ja, ko. It
  compares the segmenter with ICU, not Xapiand's wrapper (`text::cjk_words`) plus the term generator,
  which is where F1 sits.
- analysis diff 158c81b..d72eb656 read in full (Q3 below).

## What I checked myself

Scripts in `.scratch/xapiand/review-cjk-words-correctness/` (`.purpose` there); jobs run the quest
slot's built `xapiand` at 02e890123's code.

1. 261120-173705-fbbc5d (`probe.sh`): words semantics through the real server, and 450 corpus
   lines (150 per language) indexed and queried back. Mixed Latin and CJK, digits (`2020年`), katakana
   with the long vowel mark, Hangul, halfwidth katakana, wildcards, partial and fuzzy all index and
   query the same words. 448 of 450 lines find themselves; the two misses are a punctuation-only line
   and a line pushed past the first page of 10 hits (page size, not a mismatch).
2. 261120-174024-f81a55 (`words.sh`): the real `text::cjk_words` from the quest's text.cc on edge
   cases, beside ICU's own segments for the same lines (261120-173821-57a81b, `icu.sh`). `ニュース・ヘッド
   ライン`, `ジョン・スミス` (U+30FB) and `漢字abc` split as ICU's words do downstream.
3. 261120-174814-f68518 (`probe2.sh`): infix characters between CJK words. This is F1.
4. 261120-175138-aa76a0 (`probe3.sh`): exclusivity through `_schema`, through a document field,
   through an object's properties; the change of an existing field; a Latin phrase control.
5. 261120-173757-9b2ea8 (`mutants.sh`): mutants of the feature. M1 `cjk_words` returns its input
   unchanged: KILLED (test_cjk_words.cc:60 fails). M4 every CJK character its own word (n-gram-like):
   KILLED (same check). M2 and M3 in that job did not apply (my sed was wrong; the job says
   SURVIVED, which is not a result). Redone as 261120-175143-6acca9 (`mutants2.sh`): query side off
   and index side off; see Not run if it has not finished.

Answers to the lead's questions:

1. Index and query use the same function (`engine::text::cjk_words`), so they read the same words in
   every path that exists: `GenerateTerms::text` is called from three places in `Schema::index_item`,
   each passing `cjk_words`; `parse_text` has one production caller, `QueryDSL::get_term_query`, which
   sets `CJK_WORDS` from the field spec (the spec is read in `get_data_field` and `get_slot_field`,
   both updated). Mixed CJK/Latin, numbers and text boundaries agree (probe 1; each array value or
   field is indexed on its own, so no word spans two values). The gap is not index versus query
   (both read `中文·英文` as one word) but both versus ICU (F1).
2. Exclusivity holds in every path I could reach (probe 4): `_schema` both true is 400 in either
   order, a document field with both true is 400, an object with both true is 400, and false/true
   in either order passes. The flag is not inherited into a child (the stored child schema shows its
   own value), so there is no stale inherited `_cjk_ngram` that rejects a valid child. Counted by
   diff, the `cjk_ngram` and `cjk_words` mentions in schema.cc are 43 each; the only extra lines are
   the two exclusivity checks. The reserved-word tables (feed, process, concrete, `has_dispatch`)
   all have the new key. An existing index with `_cjk_ngram: true` keeps working unchanged; a
   document that tries to switch it to words gets "It is not allowed to change '_cjk_words' [false -> true]",
   and one that tries ngram off plus words gets the same for `_cjk_ngram`. A `_schema` PUT that adds
   `_cjk_words` to an existing field returns 204 and changes nothing (the stored value stays
   false); I did not find out whether `_cjk_ngram` behaves the same there (F4). Old binaries ignore the key
   on read (`_dispatch_feed_properties` result is unused), so the `_cjk_words: false` now written to
   every new text field's schema does not break a downgrade.
3. The analysis range changes no token for fields without `_cjk_words`. `analysis.hh` (the stemmer,
   stop lists, case folding, scanner, `cjk_ngram` path) is not in the diff. The 28 changed files are the
   new `cjk/` library, its 316k-line dictionary, tests, tools, docs, the CI file, and a refactor of
   `test/test.cc` (JSON reader moved to `test/json.hh`). The only build change is a new `fst`
   FetchContent inside analysis (7338f564) for the dictionary; unicode and snowball pins are
   unchanged. See F3 and F5 for the costs.
4. Yes. M1 and M4 make `tests/test_cjk_words.cc` fail at line 60; the unmutated baseline passes.
   The test is thin where F1 lives (see F2).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A CJK word joined to a neighbor by a letter-infix character is indexed together with it: `text::cjk_words` copies the character unchanged, so the downstream scanner joins across it (`&`, `'`, U+2019, U+00B7, U+2027; analysis.hh `detail::infix_of`). ICU splits them. With `_cjk_words`: `乔治·华盛顿` is one term, so `body:华盛顿` and `body:乔治` find nothing; `가나다·라마` (Korean middle dot, common) misses `가나다` and `라마`; `中文&英文`, `中文'英文`, `中文‧英文` miss `英文`. The results equal those of a field without the flag. (`ジョン・スミス` with U+30FB works: it is not an infix.) Chinese transliterated names and Korean lists use U+00B7. | probe 261120-174814-f68518 (the `words=` columns); `src/engine/text.cc` `cjk_words`; `/Users/kronuz/code/Kronuz/analysis/analysis.hh:145` | In `cjk_words`, when a non-word character that can join (the infix set) sits directly between two word characters and either side is CJK, emit a space on each side of it. Operators such as `*`, `-`, `+` and `field:` never sit between two word characters in a way the infix set touches, so queries are safe. The set is in analysis's private `detail`; ask analysis for a public `is_infix`, or list the five characters in text.cc with a comment. Add them to `samples` in test_cjk_words.cc (`乔治·华盛顿`, `中文&英文`, `가나다·라마`) and a search-level check, which the docs example could carry. |
| F2 | no | gap | The tests cannot see F1 or any wrapper-versus-ICU difference. `test_indexing_splits_words` and `test_queries_split_words` compare the wrapper's output with the same text spaced by the wrapper's own `words` column, so both sides carry the same infix join; the oracle checks the segmenter, not the wrapper. A2's "words Xapiand indexes equal the segmenter's vectors" holds for the three samples (none has an infix character) and for the corpora (so does the doc's "0 differences") only at the segmenter. No test of the schema paths exists: exclusivity, the stored property and the change refusal were proved only by my probe. | `tests/test_cjk_words.cc:41-48`; probe 261120-173705-fbbc5d and 261120-175138-aa76a0 | After F1, add a test that runs the wrapper plus `term_generator` over the oracle's corpus lines and compares the terms with the segmenter's words; add a `Schema`-level test (`_cjk_words` and `_cjk_ngram` together is 400; the change is refused). `tests/test_coverage_database.cc:291` lists every reserved property and omits `_cjk_words`. |
| F3 | no | nit | `docs/src/content/docs/upgrade.md:80` still says "`_cjk_words` is gone. The property is unknown, and queries no longer split CJK words", and `changelog.md:316` lists it as removed. Both are false once this lands. | `git diff master...HEAD` touches neither; grep | Update both when landing; say it is back, on the segmenter, with no ICU. Behavior to document: new text fields now store `_cjk_words: false` in their schema, visible in `._schema`. |
| F4 | no | nit | A `_schema` PUT adding `_cjk_words: true` to an existing field answers 204 and silently leaves it false; the document path refuses with a clear 400. Probably the same as `_cjk_ngram` today (not checked). | probe 261120-175138-aa76a0 ("schema put words") | Leave, or confirm it matches `_cjk_ngram` and say so in the docs page: switching an existing field needs a new index. |
| F5 | no | nit | Costs not stated: `fst` is a new transitive dependency not pinned in Xapiand's CMakeLists beside `unicode` and `snowball` (whose comment says they are "pinned on the commits analysis itself pins"), and every binary now carries the 2.09 MB dictionary (analysis's docs) whether or not a field uses `_cjk_words`. | CMakeLists.txt:730-737; analysis `docs/cjk-segmenter.md:87` | Declare `fst` at analysis's pin the same way; give the binary growth in `docs/mechanisms/cjk-words.md`. I did not measure it. |
| F6 | no | nit | Word-level searches are as good as the dictionary is, which the docs do not say: the query is segmented alone, the document in its context, so `京都` does not match `東京都` (as in ES's analyzers). Separately, the wrapper cuts a CJK run at a non-CJK mark (`東京︀都` with a variation selector becomes `東京 ︀ 都`; ICU keeps the selector with the word) and drops a stray segment of kind none inside a run. Both rare. | probe 261120-174024-f81a55 vs 261120-173821-57a81b | A sentence in the docs page; mark handling can wait. |

## Not run

- The M2 and M3 mutants (query side off, index side off) were queued behind the host's job queue when
  I wrote this (261120-175143-6acca9); read its log. By construction `test_queries_split_words` and
  `test_indexing_splits_words` compare split against spaced text, so they should fail. M1 and M4
  already answer question 4.
- No phrase proof: on this build a quoted query behaves as an OR even for Latin words
  (`body:"alpha beta"` returns the document that has only `alpha`, probe 261120-175138-aa76a0), so I
  could not show that a phrase of CJK words is a phrase of terms. Pre-existing, not this quest's.
- The FreeBSD and arm64 CI of this commit (still running), speed of the wrapper, memory of the
  segmenter per call, and indexing throughput.
- Thai, Lao, Myanmar and Khmer also go through the segmenter, because `is_ngram_char` covers them;
  the segmenter keeps Thai whole (analysis commit 8ce3f0c). I did not test them.
