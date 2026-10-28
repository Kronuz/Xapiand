# strata-engine-callbacks (S3, phase 7a)

> Status: DONE, MERGED LOCALLY (2026-10-28) workstream `strata-engine-callbacks`; branch `phase7a/strata-engine-callbacks` (merged into Xapiand master, not pushed, no CI run).

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
  refusals; later commits on the branch: the once-per-value check and the spy-terms test.

## Proof

- Tests (Release build, own deps in `.scratch/build-s3`): `test_engine_callbacks` 942 checks, `test_engine_strata`
  9568, `test_engine_search_strata` 231,298, `test_engine_search` 4423: all pass.
- Mutants for this round (each caught): `_terms` never from values (harness: 62 searches differ), no refusal in
  the spy's view (new `TermsSpy` test fails 2 checks, and the harness), dedupe removed (survived: a document stores
  each distinct value once, so the dedupe was removed as redundant). Revert check: removing the refusal fails
  `test_spy`; removing the values path fails the check. The earlier 16 mutants (previous round) still apply.
- A comparison that did not hold: the harness once loaded no documents on either engine (a keyword array was
  refused); it now asserts both indexes hold all documents and that the buckets sum correctly.
- ASan+UBSan: see the log below.

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

- 2026-10-27: branch created from S2's head (Xapiand:90cfd9775); tests, mutants, ASan, fuzz, server checks.
- 2026-10-28: R-P2 ruling: merged master twice, repinned prospect to main, `_terms` over values, term lists refused,
  checks rerun; build dirs deleted.
