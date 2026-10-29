> Status: CURRENT (2026-10-28) gate 7a fix round, branch `phase7a/gate-fixes` (from `phase7a/gate`, base master 9fa890ecc); workstream gate7a-fixes, developer.

# Gate 7a fixes

## State

Fixes 1 to 4 done and proved by unit tests and mutants (below). Items 5 to 7 (lead's widened scope: delete ELITE_SET,
`_terms` over text, INFO term lists) in progress. Rule: a feature strata won't serve naturally is removed, not refused;
when done, T6 on strata should be 100% of the remaining assertions with no refusals.

## Fixes

| ID | Root cause | Fix | Test (fails with the fix reverted) |
|---|---|---|---|
| R1 | The strata shard left the version slot (`DB_SLOT_VERSION`) at whatever the caller asked for (the expected version, 0 on a new document), while Xapian's adapter stamps the version the write landed at. The host reads the slot back to check the next versioned write, so the second save of the foreign schema document (`.schemas/...`) always conflicted and `update_schema` ran out of retries | `stamp_version` in `src/engine/strata/shard.cc` writes the slot at the three write sites | `version_slot` in `tests/test_engine_strata.cc`; `harness/restore_schema_check.sh` (E2E, both engines) |
| D5 | `validate` refused a non-joining operator anywhere in an append chain; the query DSL appends PHRASE/NEAR onto an empty accumulation (which `build` already skips) | `validate` ignores the operator of an append onto an empty accumulation (`search.cc`) | four new cases in `tests/test_engine_search_strata.cc` (both engines agree) plus refusals that stay |
| D1 | `DatabaseHandler::check` called `engine::xapian::check` for every index | `engine::check(path)` in `src/engine/open.{h,cc}` picks the engine by `kind_of` | `check_by_kind` (good shard passes, flipped byte fails, no Xapian text) |
| D3 | `Shard::stats` left `doclen_lower/upper` at 0 (TODO P4) although prospect keeps `min_length/max_length` per segment | `stats` takes the min/max over the segments that hold documents | `length_bounds` |

The host still shows `doc_len_lower` as `-lower` (an unsigned wrap), as it does for Xapian.

## Proof

Mutants (each killed): R1 drop the slot write (fails `version_slot`, line 1108); D5 never skip (fails the search case list); D1
always the Xapian check (fails `check_by_kind`); D3 drop the upper bound (fails `length_bounds`).

## Next / Blocked on

Items 5 to 7, then the full proof run (Release ctest, T6 on both engines, T7, callbacks and numeric_range checks, ASan+UBSan ctest).
