# cold-tier-step2-item1: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step2-item1.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:8d7467b on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step2.md`: "`unleased_per_index` (B lazy, no defer; A eager): a map of B under A's `lease_scope`" |
| A2 | `docs/status/cold-tier-step2.md`: "`unleased_threads_and_moves` (lazy): 8 reader threads on distinct records, count equals the wrapper's maps" |
| A3 | `docs/status/cold-tier-step2.md`: "`queue_clause` (eager; deletes inline, and posted to an `inline_executor`): 6 queued, 7 segment files" |
| A4 | `docs/status/cold-tier-step2.md`: "`uncommitted_clause` (eager): buffered, flushed-unsynced, and a delete alone each refuse" |
| A5 | `docs/status/cold-tier-step2.md`: "`warm_clauses` (eager; defer for the owed turn; merges on a `stepping_executor`): an outstanding `map_owed` turn" |

## Result

Landed at strata:8d7467b on 2026-11-02. Rel ctest 62 of 62; lazy rel 62 of 62; ASan+UBSan 62 of 62 and TSan 62 of 62. Review: `docs/reviews/cold-tier-step2-items1-2.md` (findings folded in).

- Merge: Merge branch 'phase/cold-tier-step2-item1' (index prerequisites; review fixes for items 1 and 2).
- Landed as strata:8724081, strata:c2a6580, strata:5469e5c and strata:fec0f73.
- Moved-from lease rebinding fixes an ASan heap-use-after-free.
- U1-U7 and C1-C2, C4-C10 are killed; U2d and C3 survive as equivalent.
- Preflight: mac-hardened and mac-asan-ubsan ok; Linux ok; FreeBSD not run.
- CI is pushed and watched by the lead; no decoder or parser was added.
- Docs: `docs/mechanisms/warm-indexes.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
