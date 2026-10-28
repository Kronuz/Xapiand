# strata-engine-callbacks (S3, phase 7a)

> Status: CURRENT (2026-10-27) workstream `strata-engine-callbacks`; branch `phase7a/strata-engine-callbacks` from `phase7a/strata-engine-search` (Xapiand:90cfd9775). Local only, not pushed, no CI run.

Design: `strata/docs/design/xapiand-on-strata.md` (strata:8972cf8), row S3, D4, G4, G5, G6, G8.

## State

Ready for review except criterion 6 (term lists) and the aggregations that read them: waits on prospect P2
(`phase4/term-lists`). Local only, no CI. Everything below ran on a scratch merge of prospect
`xapian-ops` + `values-bytes` with strata `phase4/values-rank` (b644708); re-pin when P1 lands.

## Done criteria (design row S3)

1. [x] SYNONYM, XOR, MAX, ELITE_SET, SCALE_WEIGHT with Xapian's scoring (`test_operators`, 29 queries x 1 and 5
   shards, weights to 1e-9). A nested XOR and an elite set chosen by whole-index frequencies; an OR inside a MAX is
   read as Xapian does (found by the test, fixed).
2. [x] WILDCARD (prefix, `?`, `*`) and EDIT_DISTANCE expand over the dictionaries once per search; limits and
   combiners as Xapian's (`test_expansions`). Multi-shard weights of expansions differ from Xapian's (it weighs
   them 1.5 to 5 times higher; ours equal one shard holding every document): documents and counts agree.
3. [x] Posting sources scan the values plugin's column (`test_sources`, with deletes and replacements).
4. [x] Key makers order by key, then weight, then docid in the requested order (`test_sorter`, exact order, windows).
5. [x] A spy sees every match with a lazy `DocView` whatever `check_at_least` and the window say (`test_spy`).
6. [ ] `DocView` values read from the values plugin: done. Term lists (`terms`, `term_details`, `serialise`): not
   wired (P2 not landed); they raise `errc::not_available`.
7. [x] Tests shared with the Xapian engine: `tests/test_engine_callbacks.cc` 934 checks, one and five shards;
   `tests/test_engine_strata.cc` 8091; `tests/test_engine_search_strata.cc` 232,732. ASan+UBSan: callbacks and
   strata tests clean. 16 mutants killed (below). Fuzz: `fuzz_strata_pattern` 3.4 million runs, no failure.
8. [x] `harness/numeric_range_check.py` on a strata index: 797 ranges, 0 wrong. `harness/strata_callbacks_check.py`
   (new): 120 searches on both engines through the server (metric and bucket aggregations with and without a
   query, ranges, sorted, windows): 114 identical, 6 differ, all `_terms` buckets, which need term lists.
9. [x] `docs/mechanisms/strata-callbacks.md`.

## Done

- Xapiand:8e4a37b4b first draft; Xapiand:4a7873574 callbacks test + OR in MAX; Xapiand:95efb3fc3 drop a refusal;
  Xapiand:e8d634c8d pattern split + fuzz target; Xapiand:7b5a62fdf append fix; the check script and note in the
  commits after. Merge of master (Xapiand:523c9f398) brought the numeric-range and CHECK_ALL changes; a rebase
  onto S2 drops it.

## Proof

- Mutants (`.scratch/mutants.py`, one per rule, each fails the callbacks or strata test): spy visits only a
  window, sort ignores keys, sort descending, estimate counts only the window, OR in MAX summed, synonym df of
  one term, elite set keeps the lightest, XOR drops a lone child, source never matches, values not stored,
  wildcard limit ignored, most-frequent keeps the rarest, edit distance without swaps, `?` matches a byte, append
  ignores an empty accumulated query, spy given a shard-local id: 16 killed, 0 survived (an "empty value is
  stored" mutant survived because the document already drops empty values; the redundant check was removed).
- Revert check for the append fix: the mutant "append ignores ..." fails `test_appends`; without the fix every
  server search on a strata index found nothing (`(<nothing> AND (<nothing> AND term))`).
- Found by the tests: MAX(OR) flattening; the Xapian engine reserves value slots 0 to 19 (tests use 21+);
  Xapian's spy is given the id within its shard.

## Findings for others

1. S2 (`strata-engine-search`): the append bug above, in S2's `build`, fixed here in Xapiand:7b5a62fdf; the
   same hunk should land with S2. S2's `544b0ecb0` adds a `SpyView` and removes the spy refusal; this branch's
   `SegmentDocView` (values included) replaces it, so the two will conflict in `search.cc` and `test_refusals`.
2. S2: `xapiand_test_engine_search_strata` under ASan overflows the stack destroying the 20,000-append chain of
   `disjunction_scorer` (recursive destructors; passes without ASan). Not from this branch.
3. The server needs `harness` runs with the lead's quiet queue: a pending strict hold blocked every light build
   for about 40 minutes.

## Dependencies recorded (lead note, key prefix redesign)

`expand.cc::in_key_space`, `Plan::key_prefix` and the flat field+term strings in `expand`/`wildcard_prefix` are the
only places this branch reads the key prefix or the concatenation; they change together when strata's
`design/fields-and-keys` lands. The Pinned key prefix reaches `term_table` through S2's code.

## Next

1. Wire term lists when P2 lands: `SegmentDocView::terms/term_details/serialise` (`doc_view.cc`, `no_term_lists`),
   opt the structure in (`index_options()` in `shard.cc`), key term from the keys plugin, positions through
   `positions_reader`; then rerun `harness/strata_callbacks_check.py` (expect 0 different) and the docs suite on
   strata (S2's `.scratch/e2e.sh`). Layout order in the scratch merge: inverted, keys, stored, values, term_lists.
2. Re-pin prospect/strata (P1, P2, P3 are on prospect main or branches), rebase onto S2.
3. Collapse, cutoffs, `nearest`/`fuzzy` stay refused.

## Blocked on

Term lists: P2 `phase4/term-lists` (committed, last commit unbuilt per its ledger, not landed).

## Log

- 2026-10-27: branch created from S2's head (Xapiand:90cfd9775). Scratch build dir `.scratch/build` configured against the
  merged prospect (xapian-ops + values-bytes) and strata `phase4/values-rank`.
- 2026-10-27: tests, mutants, ASan, fuzz, server checks as above; build dirs deleted.
