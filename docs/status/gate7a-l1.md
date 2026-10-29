> Status: CURRENT (2026-10-29) gate 7a blocker L1 and condition C3, branch `phase7a/gate-l1` (from master `Xapiand:3bc11f31f`); workstream gate7a-l1, developer.

# Gate 7a L1 and C3

## State

L1 and C3 are done on the branch; ctest, the docs E2E on both engines, the removed-features check and ASan+UBSan on the engine search tests are green. Nothing is pushed.

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

## Remaining refusals in `src/engine/strata` (listed, not fixed)

`grep -n "unsupported("` finds the five uses in `validate` (`search.cc:175-222`), all of query shapes Xapian serves:

1. PHRASE or NEAR of anything but terms (`search.cc:175`)
2. SYNONYM of anything but terms (`search.cc:183`)
3. an operator that does not join, as a combine operator (`search.cc:189`) and as an append operator onto a non-empty accumulation (`search.cc:203`)
4. a wildcard or edit-distance expansion combined by an operator other than SYNONYM, OR or MAX (`search.cc:222`)

Their message is "The strata engine does not search ... yet". Whether each is reachable from the HTTP query DSL is not checked here.

## Done (commits, Xapiand)

- Xapiand:1b8954691 delete the nearest and fuzzy parameters and the weight cutoffs
- Xapiand:985417616 serve collapse on the strata engine
- Xapiand:22e56602a check the generated terms with searches (C3)

(later commits on the branch: the census and handler-include follow-up, the quiet removed-features check, this ledger)

## Next

- Lead: read the findings (collapse_max above 1; the remaining refusals); decide whether Xapian's `collapse_max` above 1 stays as is.
- Re-check of L1 by `probes/refusals.sh` and `removed_features_check.sh` on the branch head.

## Blocked on

Nothing.
