> Status: CURRENT (2026-10-29) gate 7a blocker L1 and condition C3, branch `phase7a/gate-l1` (from master `Xapiand:3bc11f31f`); workstream gate7a-l1, developer. Round 2 (the remaining refusals) is below.

# Gate 7a L1 and C3

## State

L1 and C3 are done on the branch, and the remaining refusals are closed (round 2 below); ctest, the docs E2E on both engines, the removed-features check and ASan+UBSan on the engine search tests are green. Xapiand is not pushed; prospect `phase8/phrase-alternatives` is.

## Rulings applied (lead, 2026-10-29)

| # | Ruling | What changed |
|---|---|---|
| 1 | Serve `?collapse` and `collapse_max` on strata | `src/engine/strata/search.cc`: a collapse makes the search visit every match and offers each to prospect's `collapse::collapser`, keyed by the slot's value as the host wrote it (the same bytes Xapian groups by). Hits rank by sorter key, weight, then id in the request's order; the window applies after the collapse; the estimate is the hits kept. The refusal is gone |
| 2 | Remove `?fuzzy` and `?nearest` from both engines | The parameters and their settings (`search_views.cc`), `similar_field_t` and `query_field_t` members, `DatabaseHandler::get_similar` and the `match`/`get_mset` arguments, `engine::Similar` and the request fields, the Xapian engine's elite-set expansion and `PrefixDecider`, the strata refusal, census counters, tests, changelog under Removed. The `_fuzzy` query (edit distance) is untouched. A request that still carries them is an ordinary search |
| 3 | Delete the weight cutoffs | `percent_threshold` and `weight_threshold` leave `DatabaseHandler`, `SearchRequest`, the Xapian engine call and the strata refusal |
| 4 | C3 | Searches replace the removed `.terms` assertions; `terms_diff.py` and `vocab_diff.py` are ported (below) |

## C3

Seven docs pages gained 41 search requests (`tests/datastructures/{arrays,objects}`, `tests/datatypes/check-data-types`,
`tests/namespace/namespace`, `tests/schema/{define-prefix,define-slot,uuid-field}`), each with a total assertion. They cover what the 193 removed lines checked:
the words of arrays and objects, the stems of the long `personality` text, full and partial namespace paths (with a negative for
`_partial_paths: false`), numeric, date, datetime, time and geo values by equality and by range, custom prefixes, slots (the id
and a range on the slot value), and uuid keys against the shared uuid field (positive and negative). Leftover assignments from the
earlier deletion (`t1 = ...`, `field_name = ...`) and an assertion-free test were cleaned out of the same pages.

`harness/terms_diff.py` becomes `analysis_diff.py`: for each of 130 analysis configurations it indexes the 15-text corpus and records,
for every corpus word and every adjacent word pair (as a phrase), the ids a search finds. `harness/vocab_diff.py` becomes
`stem_diff.py`: one document per snowball sample word (500 per language, 18 languages, 2 variants), and for each word the documents a
search finds (the words that stem alike). `docs/dev/testing.md` describes both.

## Proof (RelWithDebInfo, Apple clang, one binary for both engines)

- ctest: 28 of 28 pass (`.scratch/ctest.log`).
- Docs E2E (`harness/engine_e2e.sh`, newman 6.2.3): Xapian 649 of 649, strata 649 of 649, 377 requests (335 and 566 at the gate), 0 regressions,
  0 status differences. `bodydiff.py`: the same 5 of 377 as at the gate (a generated id, three 404 wordings, `:metrics`).
  `search_order_diff.py`: 134 of 134 responses with hits identical in order, score, total and count (92 at the gate).
- `removed_features_check.sh` (extended): 10 of 10 per engine: `?fuzzy`, `?nearest`, both, and `nearest` with every setting return the same hits and total as the plain search;
  `?collapse=eyeColor` keeps one hit per color. Against the pre-change binary (`t11-barriers`, `Xapiand:25d2b20c0`) it fails: on Xapian the four parameter checks fail (the answer
  changes), on strata they fail with a 400, and so does `collapse`.
- Collapse parity (`test_collapse` in `tests/test_engine_search_strata.cc`): 144 cases (4 queries x 2 id orders x with and without a sorter x collapse_max 1 to 3,
  over 1, 2 and 5 shards), each also at 5 windows. Strata equals an oracle (the uncollapsed ranking cut to the first `collapse_max` of each group) in every case.
  With `collapse_max` 1, Xapian and strata return the same hits in the same order with the same ranks, weights, estimate and spied documents. With `collapse_max` 2 and 3
  both engines return the same estimate and the same number of hits per group, but Xapian keeps a different document in 718 hits (see Findings). HTTP: `?collapse=eyeColor`,
  `&collapse_max=2`, `&sort=-balance`, `&q=personality:kind` agree on both engines for `collapse_max` 1; `collapse_max=2` differs as above.
- Mutants, each killed by `test_collapse` (`.scratch/mutant.sh`): A collapse never applied (revert check; fails at the first case), B sorter key ignored in the rank (2010 checks fail),
  C weight order reversed (2477), D documents without the value collapsed together (fails).
- `analysis_diff.py` and `stem_diff.py` on one node per engine: byte-identical dumps (42,798 probes, 22,974 with hits, 53 distinct dumps across the 130 configs;
  18,006 stem probes, 84 conflating); 28 PUTs fail the same way on both (a `_language` change on an existing index in the basque config).
- ASan+UBSan (Homebrew LLVM 22, `-fsanitize=address,undefined -fno-sanitize-recover=undefined`, RelWithDebInfo): `test_engine_search_strata` (168,970 checks, including
  `test_collapse` against Xapian), `test_engine_search` (1,786), `test_engine_callbacks` (922) and `test_engine_query` (22,440) pass with no report. `test_engine_sort` aborts
  on the known RapidJSON `Stack::Push` null-offset finding (`rapidjson/internal/stack.h:117`, recorded in `gate7a-fixes.md`), not in this branch's code.

## Findings

1. **Xapian's collapse is inexact above `collapse_max` 1.** It keeps the right number per group and the right estimate but not always the best documents (the second
   brown eye color of the bank is account 6 on Xapian and 3 on strata, whose ranking is the exact one). With a window smaller than the answer (`limit` below the
   number of groups) it can also return a lower-ranked document of a group whose better one it pushed out of its window (`?collapse=age&limit=5` on the bank returns
   accounts 6, 11, 16, 21, 1000 on Xapian and 1 to 5 on strata). Strata is the exact answer, so parity is claimed for `collapse_max` 1 with a covering window and
   the oracle for the rest. This is replaced-code behaviour; not changed.
2. `Hit::percent` differs between the engines (Xapian truncates its scaled weight, strata rounds the share of the best); known from the S2 ledger, unchanged here, and it only
   shows in `#percent` of a debug response.
3. Memory: the collapse keeps one entry per distinct value of the slot for the length of a search (documents with no value are kept as hits). A collapse on a field with
   millions of values holds millions of entries. Not capped.
4. `percent_threshold` and `weight_threshold` have no remaining reader except the Xapian remote protocol (`remote_protocol_views.cc`, which decodes them from the wire and
   which I did not read or change).

## Remaining refusals (round 2, 2026-10-29): closed

`grep -n "unsupported(" src/engine/strata` finds nothing; `validate` and the helper are deleted. Every shape is either searched or refused when
the `Query` is built (`src/engine/query.cc`), by both engines, with a 400 that says the query is not valid.

### Reachability (probed on both engines before the change; bank, `personality` is a text field)

| Shape (strata refusal) | From HTTP? | Request, and the answer before |
|---|---|---|
| PHRASE/NEAR of non-terms (`search.cc:175`) | No. `queryparser` refuses a NEAR/PHRASE operand that is not a term (`queryparser.cc:1087`, kind `unsupported`, Xapian's text) | `{"personality":{"_phrase":"first se*"}}`, `"first seen~"`, `"hard (to too) describe"`, `_near` likewise: 400 "OP_NEAR and OP_PHRASE only currently support leaf subqueries" on both engines; the strata check was dead code. Only a `Query` built by the factories (the tests) reaches it |
| SYNONYM of non-terms (`:183`) | Yes | `{"personality":{"_synonym":"hard desc*"}}` (also `hard descr~`, `hard (to too) describe`): Xapian 200 (77 hits for the first), strata 400 "does not search SYNONYM of anything but terms yet" |
| Non-joining operator as a combine operator (`:189`) | Only as an array: `{"_query":{"_scale_weight":[{"personality":"kind"},2]}}`, `_wildcard`, `_edit_distance` lists | 400 "InvalidArgumentError: op not valid with a list of subqueries" on both engines (Xapian's constructor, which `engine::prepare` runs for every engine); the strata check was dead code |
| Non-joining operator as an append operator onto a query (`:203`) | No: the DSL appends with a non-joining operator only onto nothing (leaf maps) | |
| Expansion combined by anything but SYNONYM/OR/MAX (`:222`) | No: the DSL and the parser build every wildcard and edit distance with SYNONYM | |

`engine::prepare` converts every query to a Xapian query first, whatever the engine, so a shape Xapian's constructor refuses never reached strata's `validate`.

### Rulings applied

- Served: PHRASE/NEAR whose positions hold alternatives (a wildcard or edit-distance expansion, an OR or SYNONYM of words), by the strata engine, with
  prospect's new `multi_phrase_scorer` (`prospect:14dbbbb`, branch `phase8/phrase-alternatives`, pushed, Xapiand repinned): each position is the scorer
  of its alternatives, the phrase is found on the merged positions of their terms, and a document scores the positions' scores summed. The Xapian engine
  does not search them (its matcher takes terms only, which is why the premise "Xapian serves them" did not hold): `engine::prepare` skips the Xapian
  conversion for them and `xapian::search` answers a client error. Since `queryparser` hands PHRASE/NEAR terms only, these are not reachable over HTTP
  until the parser lets a wildcard, fuzzy or partial word through (see Blocked on).
- Removed, from both engines: SYNONYM of anything but words (the Xapian engine used to search it), PHRASE/NEAR of anything but words and their
  alternatives, SCALE_WEIGHT/WILDCARD/EDIT_DISTANCE/INVALID over a list, an append of a non-joining operator onto a query, an expansion combined by
  anything else. The factories throw `QueryParserError` (400) "... is not a valid query"; `parse_text` reports queryparser's unsupported-operand error in
  the same words ("A PHRASE or NEAR of anything but words is not a valid query"). Changelog: Removed.
- Justified T7 difference: strata's `collapse_max` above 1 is exact (the best `collapse_max` documents of each group within the window); Xapian's
  returns lower-ranked documents of a group whose better one fell outside its window (bank `?collapse=age&limit=5`: Xapian accounts 6, 11, 16, 21, 1000;
  strata 1 to 5). The lead keeps strata's behaviour. Changelog: Changed.

### Proof (RelWithDebInfo, Apple clang, one binary for both engines)

- ctest 28 of 28 (`.scratch/ctest.log`). `test_engine_search_strata` 477,059 checks: `test_invalid_shapes` (every invalid shape refused at construction, every valid
  neighbour built, `alternatives()`), `test_phrase_alternatives` (9 shapes x PHRASE and NEAR windows 3 and 6 x alone, under AND and under OR x 1, 2 and 5 shards x
  3 segments: documents, weights and order equal a model written from the definition; there is no Xapian oracle, its matcher refuses them, so the model is
  the oracle), the Xapian engine answers `syntax`. `test_text` (the 4 recorded SYNONYM rows, the PHRASE/NEAR message), `test_engine_query` (SYNONYM only over words).
- `harness/engine_e2e.sh`: Xapian 649 of 649, strata 649 of 649, 377 requests, 0 regressions, 0 status differences; `bodydiff.py` the same 5 of 377;
  `search_order_diff.py` 134 of 134 identical.
- `removed_features_check.sh` (extended): 10 more checks per engine, all ok (`_synonym` over a wildcard, a fuzzy word and a bracketed group; `_phrase`/`_near` over a
  bracketed group, a wildcard, a fuzzy word; `_scale_weight` and `_wildcard` given a list: 400 "not a valid query"). Before the change (probed above) the answers differ:
  Xapian 200 for the `_synonym` ones, strata "does not search ... yet", both engines Xapian's own messages for the rest.
- Revert checks and mutants (`.scratch/mutant_alt.sh`), each killed by `test_engine_search_strata`: no edit-distance alternatives, no wildcard alternatives, no OR/SYNONYM
  alternatives (these three are the served-shape revert checks), NEAR read as PHRASE, alternatives not served at all, PHRASE accepting an AND, SYNONYM accepting anything,
  any wildcard combiner accepted, any append operator accepted. prospect: `.scratch/mutants.sh`, 7 of 7 killed by `multi_phrase_test` (offset, NEAR window, distinct positions,
  score, confirmation, merged slot terms, first term only), ASan+UBSan clean.
- ASan+UBSan (Homebrew LLVM 22, `-fsanitize=address,undefined -fno-sanitize-recover=undefined`, RelWithDebInfo, RapidJSON patched as in `gate7a-l2`): `test_engine_search_strata` (477,059
  checks, with the Xapian comparison on), `test_engine_query` (22,439), `test_text` (13,205) and `test_engine_search` (1,786) pass with no report.
- `fuzz_text` 90 s: 3,876,800 texts, 2,932,131 parsed, 944,669 rejected, no violation (a phrase over alternatives stands for its tree there).

## Done (commits, Xapiand)

- Xapiand:1b8954691 delete the nearest and fuzzy parameters and the weight cutoffs
- Xapiand:985417616 serve collapse on the strata engine
- Xapiand:22e56602a check the generated terms with searches (C3)
- (the census and handler-include follow-up, the quiet removed-features check, the round 1 ledger)
- Xapiand:3956598af pin prospect with the multi-phrase scorer
- Xapiand:5a261e358 refuse invalid query shapes when the query is built (round 2)
- prospect:14dbbbb `multi_phrase_scorer` (branch `phase8/phrase-alternatives`, pushed)

## Next

- Lead: the Blocked-on decision below.
- Re-check of L1 by `probes/refusals.sh` and `removed_features_check.sh` on the branch head.

## Blocked on

- Whether `queryparser` should hand a wildcard, edit-distance or partial word to PHRASE/NEAR (Lucene's match_phrase_prefix: `{"_phrase": "first se**"}`). Today the
  parser refuses them (`queryparser.cc:1087`, Xapian's behaviour, pinned by the 25 error rows of `parse-ab.jsonl`), so the engine support above is not reachable over HTTP.
  Changing it is a grammar decision in another repository with recorded vectors, so I stopped there. The engine side is ready: the test builds the queries by hand.
