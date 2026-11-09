> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-10-28) gate 7a fix round, branch `phase7a/gate-fixes` (from `phase7a/gate`, base master 9fa890ecc); workstream gate7a-fixes, developer.

# Gate 7a fixes

## State

Fixes 1 to 4 and the owner's removals (items 5 to 7) are done, with one open item: the PHRASE window (below). Rule from the
lead: a feature strata won't serve naturally is removed, not refused; so T6 on strata should be 100% of the remaining
assertions with no refusals. It is 562 of 566, the four being the one window-PHRASE request.

## Fixes

| ID | Root cause | Fix | Test (fails with the fix reverted) |
|---|---|---|---|
| R1 | The strata shard left the version slot (`DB_SLOT_VERSION`) at whatever the caller asked for (the expected version, 0 on a new document), while Xapian's adapter stamps the version the write landed at. The host reads the slot back to check the next versioned write, so the second save of the foreign schema document (`.schemas/...`) always conflicted and `update_schema` ran out of retries | `stamp_version` in `src/engine/strata/shard.cc` writes the slot at the three write sites | `version_slot` in `tests/test_engine_strata.cc`; `harness/restore_schema_check.sh` (E2E, both engines) |
| D5 | `validate` refused a non-joining operator anywhere in an append chain; the query DSL appends PHRASE/NEAR onto an empty accumulation (which `build` already skips) | `validate` ignores the operator of an append onto an empty accumulation (`search.cc`) | cases in `tests/test_engine_search_strata.cc` (both engines agree) plus refusals that stay |
| D1 | `DatabaseHandler::check` called `engine::xapian::check` for every index | `engine::check(path)` in `src/engine/open.{h,cc}` picks the engine by `kind_of` | `check_by_kind` (good shard passes, flipped byte fails, no Xapian text) |
| D3 | `Shard::stats` left `doclen_lower/upper` at 0 (TODO P4) although prospect keeps `min_length/max_length` per segment | `stats` takes the min/max over the segments that hold documents | `length_bounds` |
| 5 | ELITE_SET deleted: reserved word, DSL, default operator, `Op::ELITE_SET`, strata group, docs page/example, tests | | `harness/removed_features_check.sh` (`_elite_set` is 400) |
| 6 | `_terms` reads stored values only; a text field or a field without values answers 400 | `TermsHandler` | same harness; callbacks check |
| 7 | INFO of a document lists no terms on either engine; `DocView::terms/term_details`, `Document::get_terms/hash` and the response names are gone; docs assertions on `terms` removed | | same harness; callbacks check |

The host still shows `doc_len_lower` as `-lower` (an unsigned wrap), as it does for Xapian. The similar-documents search
(`nearest`, `fuzzy`) keeps its own direct use of Xapian's elite set inside the Xapian engine; it is not the operator.
`DocView::serialise()` stays (the strata message now says it does not serialise a stored document).

## Proof (Release, one binary for both engines)

- Release ctest: 28 of 28 pass.
- T6 (`harness/engine_e2e.sh`, newman 6.2.3): Xapian 566 of 566 (335 requests, the removed assertions are gone: 576 before);
  strata 562 of 566. The 4 failures are one request: `SEARCH /bank/` with `personality: {_phrase: "these days are few and far
  between"}`.
- T7 (`harness/bodydiff.py`): 6 of 335 differ (41 before): a generated id, three 404 wordings (D9), the metrics counters, and
  the same PHRASE search (200 against 400). R1 and D3 are gone.
- `strata_callbacks_check.py`: 210 searches, 0 different. `numeric_range_check.py`: 797 ranges, 0 wrong.
- Mutants, each killed: R1 drop the slot write (`version_slot`); D5 never skip (search case list); D1 always the Xapian check
  (`check_by_kind`); D3 drop the upper bound (`length_bounds`); 6 drop the text refusal (removed-features check); 7 re-add an
  empty `terms` to INFO (removed-features check, after fixing its grep for binary output).
- ASan+UBSan (Apple clang, `-fsanitize=address,undefined -fno-sanitize-recover=undefined`, RelWithDebInfo): ctest 26 of 28 pass,
  no ASan report. The two aborts (`test_engine_sort`, `test_text`) are one UBSan finding inside RapidJSON, `Stack::Push` adding
  an offset to a null base (`rapidjson/internal/stack.h:117`, reached from `Document::Parse("null")` and `Writer`); a 2-line program
  with RapidJSON alone reproduces it, so it is not from this branch. Runtime suppression did not silence it (compile-time
  `-fno-sanitize=pointer-overflow` would; not rebuilt).

## Done (commits)

- Xapiand:acd2cd42d search PHRASE and NEAR over text on strata (D5)
- Xapiand:42016bba9 fix strata version slot, :check and length bounds (R1, D1, D3)
- Xapiand:5525ce86a ledger
- Xapiand:0c7d4bb59 delete the ELITE_SET operator
- Xapiand:dc78c182c delete _terms over text and INFO term lists

## Next

The owner or architect rules on the PHRASE window (below); then the lead repins prospect and removes the refusal.

## Blocked on

PHRASE with a window longer than its terms (stop words dropped from the phrase: "these days are few and far between" is 5
terms in a window of 7). Xapian matches the terms in order within the window; prospect's `phrase_scorer` serves consecutive
terms (window 0) and an unordered window (NEAR), nothing between. Serving it takes an ordered-window mode in prospect's
`phrase_scorer` (next to `near_freq`, about 15 lines) and a repin: a library change for the architect or the owner. Until then
`validate` refuses it with "a PHRASE with a window other than its length". NEAR over the same text passes.
