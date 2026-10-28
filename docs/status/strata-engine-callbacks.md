# strata-engine-callbacks (S3, phase 7a)

> Status: DONE, MERGED LOCALLY (2026-10-28); review fixes CR1, CR2 and the follow-ups below on the branch (2026-10-29) workstream `strata-engine-callbacks`; branch `phase7a/strata-engine-callbacks` (merged into Xapiand master, not pushed, no CI run).

Design: `strata/docs/design/xapiand-on-strata.md` (row S3, D4, G4, G5, G6, G8; ruling R-P2: no term lists in strata or prospect).

## State

Every done criterion holds under R-P2: the engine keeps no term lists, `_terms` is served from stored values, and
`DocView::terms`, `term_details`, `serialise` (so the `:info` term view) are refused with a 400. The branch has
master merged (S1, S2, X6 with (field, term) at the seam, copyright, alpha.10 build fixes) and prospect main
(becfe3c, strata c4ea4ce).

## Done criteria (design row S3)

1. [x] SYNONYM, XOR, MAX, ELITE_SET, SCALE_WEIGHT with Xapian's scoring (`test_operators`).
2. [x] WILDCARD and EDIT_DISTANCE expand over the dictionaries once per search (`test_expansions`); a node's field
   and pattern join through `interim::flat_term` as Xapian's flat term (an edit distance's prefix length counts the
   pattern's characters, after the field). Multi-shard weights of expansions differ from Xapian's (documents and
   counts agree).
3. [x] Posting sources scan the values plugin's column (`test_sources`).
4. [x] Key makers order by key, then weight, then docid, through `prospect::search`'s `key` hook (`test_sorter`).
5. [x] A spy sees every match with a lazy `SegmentDocView` (S2's value-less `SpyView` is replaced by it), through
   the `visit` hook; its `did()` is the id within the shard, as Xapian's (`test_spy`).
6. [x] Values read from the values plugin; term lists are not offered (R-P2).
7. [x] Tests shared with the Xapian engine: see Proof.
8. [x] `harness/numeric_range_check.py` on a strata index: 797 ranges, 0 wrong. `harness/strata_callbacks_check.py`:
   176 searches (metrics, buckets, `_terms` over keyword, integer, float, boolean, array and missing fields,
   ranges, sorted, windows) on both engines through the server, 0 different, both indexes asserted to hold the 400
   documents, and the refusals checked.
9. [x] `docs/mechanisms/strata-callbacks.md`.

## `_terms` over values (R-P2)

`TermsHandler` (`src/aggregations/metrics.cc`) counts the field's stored values (the slot), one count per
document and distinct value, when the field is not `text` and keeps its values (`required_spc_t::field_values`, read
from the field's `_index` by `Schema::get_slot_field`); otherwise it reads index terms, as before. Compared with the
Xapian engine's old `_terms` on the same documents (old binary):

| Field | Old Xapian `_terms` | Now (both engines) |
|---|---|---|
| integer, floating, date | the values plus range/accuracy terms (keys like `-453727543426300`, counts 3 to 4) | the values only |
| keyword | folded to lower case (`Red` and `red` one bucket) | raw values, case kept (Elasticsearch's) |
| boolean, array of keywords | the values | the values, a document once per distinct value |
| text (analysed) | words (`quick`, `dogs`) | words on the Xapian engine; refused (400) on the strata engine |
| missing field | no buckets | no buckets |

Dynamic string fields are `text`, so a `_terms` over one is refused on strata (count a keyword field). `_values`
(already value-based) is unchanged. A field indexed without field values (`_index: field_terms`) still reads terms
on the Xapian engine and is refused on strata.

## Done

- First draft Xapiand:8e4a37b4b .. append fix Xapiand:7b5a62fdf, the check script and note (before this round).
- 2026-10-28: Xapiand:3998a95b9 merge of master (S2's append fix and `p::search` wiring kept; the hand-rolled
  collection loop, `Candidate`, `Ranks`, `to_docid` and `SpyView` dropped), Xapiand:95905a5f3 merge of master again
  (alpha.10 build fixes), Xapiand:74d8236e5 prospect repin and API, Xapiand:d43000e22 `_terms` over values and the
  refusals; later commits: the once-per-value check, the spy-terms test, the dedupe removal, the ASan fix, the docs.

## Proof

- Tests (Release build, own deps in `.scratch/build-s3`): `test_engine_callbacks` 942 checks, `test_engine_strata`
  9568, `test_engine_search_strata` 231,298, `test_engine_search` 4423: all pass.
- Mutants for this round (each caught): `_terms` never from values (harness: 62 searches differ), no refusal in
  the spy's view (new `TermsSpy` test fails 2 checks, and the harness), dedupe removed (survived: a document stores
  each distinct value once, so the dedupe was removed as redundant). Revert check: removing the refusal fails
  `test_spy`; removing the values path fails the check. The earlier 16 mutants (previous round) still apply.
- A comparison that did not hold: the harness once loaded no documents on either engine (a keyword array was
  refused); it now asserts both indexes hold all documents and that the buckets sum correctly.
- ASan+UBSan (Apple clang, own build dir and deps; Homebrew LLVM 23 does not find libc++'s C headers here):
  `test_engine_callbacks` 942, `test_engine_strata` 9568 clean; `test_engine_search_strata` clean (231,298) with a
  64 MB stack (`ulimit -s 65520`): with the default stack its test model's recursive `weigh` overflows on the
  20,000-append chain, as S2 recorded. The sanitized server under `strata_callbacks_check.py` found a real
  stack-use-after-scope in the first version of `TermsHandler` (a temporary string viewed by `StringList`), fixed
  in the commit "Keep the stored value alive"; afterwards 176 searches, 0 different, no ASan report. UBSan reports
  RapidJSON's pointer-overflow at startup (vendored library, unrelated).
- Final Release run after the last commit: the four test binaries pass, the callbacks check 176 searches 0 different,
  the numeric-range check on strata 797 ranges 0 wrong. No new caller of the key adapter (the key prefix still
  reaches the engine through `term_key_prefix()` only). No decoder or parser changed, so no new fuzz target.

## Review round (code review CR1, CR2; lead's follow-ups)

1. **CR1.** A spy, key maker or scan sink that throws now leaves the strata engine as thrown: `CallbackGuard`
   (`shard.h`) marks the throw and `rethrow_translated(core, callback_threw)` keeps an exception of no known kind
   (strata's own kinds are still translated). `AggregationError` and `SerialisationError` are 400 again. Tests:
   `test_sorter` and `test_spy` throw a `ClientLike` (a `std::runtime_error`, not an engine error) and check its
   type; the scan sink too. Revert check: ignoring `callback_threw` fails 6 of 944 checks. The five probe shapes
   (`_sum` over keyword, `_avg` over geo and boolean, `_terms` with an unknown `_sort` field, `_terms` and
   `_values` over geo) plus histogram over keyword, `_terms` with no field and two bad sort orders are asserted 400
   on both engines by `strata_callbacks_check.py` (`misuse()`).
2. **CR2.** The harness asserts 200 on every compared search. That exposed that its aggregation names began with
   `_` ("Aggregation name '_count' is not valid") and its `_filter` lacked `_term`: most of the earlier 176
   searches were equal 400s. Fixed; 208 searches now (12 more sorted searches, a geo and a boolean field, the 20
   misuse checks), all 200 or the expected 400, 0 different. The "all aggs" search now compares aggregations only
   (`_limit: 0`): with a window of 5 and all weights equal the two engines pick different hits (docids from the
   same RESTORE batch differ).
3. **Sort shapes.** `{"balance": "desc"}` and `[{"age": "asc"}, ...]` (a field's order as a string, Elasticsearch's)
   work on both engines (`QueryDSL::get_sorter`); an order other than asc/desc, or a number, is a 400. Revert check:
   with the string branch off the harness reports 24 differences and NOT 200 on the sorted searches.
4. **`_terms` memory.** `BucketAggregation` keeps at most about `shard_size` buckets while collecting (`_prune`):
   default `_limit * 1.5 + 10`, `_shard_size` raises it (never below `_limit`); past twice that many it keeps the
   best `shard_size` in its own order (key orders exactly; count orders by count; an order by a sub-aggregation by
   count) and `serialise_results` sends the best `shard_size`. Only `_terms` and `_values` are capped. Counts of
   values pruned and met again restart, so counts fall short past the cap and a flat distribution loses its top
   (guaranteed only for values above `1/shard_size` of the documents), Elasticsearch's caveat. Nested bucket
   aggregations each cap their own buckets. `harness/terms_cap_check.py`: 345,000 documents, 200,000 distinct
   keywords and integers, 5 shards, `_limit` 10, `_shard_size` 100: top 10 right with counts within 1% on both
   engines; strata resident growth during the request 10 and 19 MB with the cap, 104 and 67 MB with the cap
   mutated off (ASan build, `ps` polled every 20 ms; the Xapian engine's number moves with file-backed pages and
   is printed, not asserted).
5. **Geo `_terms` and `_values`** are refused with 400 "terms over geo fields are not supported" ("values over geo
   fields..."): a geo slot holds a range list, not countable values, and a count of range strings is not useful.
6. **Ties inside sort groups** (keys and weight equal) can come back in another order from each engine; not changed.
   For gate 7a's body-difference list.
7. The docs E2E on the strata engine is not a gate (it refuses query text: 410 of 576 pass); on the default engine
   576 of 576.

## Findings for others

1. S2's ASan overflow in `xapiand_test_engine_search_strata` (recursive destructors of the 20,000-append
   `disjunction_scorer` chain) is S2's; see the ASan log below for whether it still occurs.
2. The Xapian engine's old `_terms` returned range and accuracy terms for numeric and date fields (table above);
   fixed here for both engines by reading values.
3. Master's `hashes` pin was declared after msgpack's own, so it never applied; it is now declared before msgpack
   (strata needs `wordwise::hash32`). Other pins matched.
4. `Shard::pin()` holds a `segment_list` now (prospect main / strata c4ea4ce), which owns the delete bits the views
   point into.

## Dependencies recorded (lead note, key prefix redesign)

`expand.cc::in_key_space`, `Plan::key_prefix` and the flat field+term strings in `expand`/`wildcard_prefix` are the
only places this branch reads the key prefix or the concatenation; they change together when strata's
`design/fields-and-keys` lands. The Pinned key prefix reaches `term_table` through S2's code.

## Next

1. Collapse, weight cutoffs, `nearest`/`fuzzy` stay refused (not in S3's row).
2. When strata makes fields first-class, `interim_terms.h`, `expand.cc` and `group_of` change together.

## Blocked on

Nothing.

## Log

- 2026-10-29: review fixes CR1, CR2, sort shapes, `_terms` cap, geo refusal (above). ASan+UBSan (own dir and deps):
  `test_engine_callbacks` 944, `test_engine_strata` 9568, `test_engine_search_strata` 231,298 (`ulimit -s 65520`),
  `test_engine_search` 4423 pass; `strata_callbacks_check.py` 208 searches 0 different (UBSan: only lz4 and
  RapidJSON); `numeric_range_check.py` 797 ranges 0 wrong on both engines; `terms_cap_check.py` right on both;
  docs E2E 576/576 (newman 6.2.3, default engine). No fuzz target: no decoder changed (the sort shape is JSON
  through the existing `QueryDSL`). Build dirs deleted.

- 2026-10-27: branch created from S2's head (Xapiand:90cfd9775); tests, mutants, ASan, fuzz, server checks.
- 2026-10-28: R-P2 ruling: merged master twice, repinned prospect to main, `_terms` over values, term lists refused,
  checks rerun; build dirs deleted.
