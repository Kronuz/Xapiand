# cold-tier-item5: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-item5.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:081b7e7 on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-item5.md`: "the snapshot of records, generation, digests, flag and bit is equal after a refusal" |
| A2 | `docs/status/cold-tier-item5.md`: "`twin_every_record` (eager off, no defer): precondition 2 records unbodied" |
| A3 | `docs/status/cold-tier-item5.md`: "M3a `deletes_dirty`, M3b marker, M3c bits, M3d facts (killed)" |
| A4 | `docs/status/cold-tier-item5.md`: "M4a no forced publication, M4b no registry scan, M4c `views_held` by pointer (killed)" |
| A5 | `docs/status/cold-tier-item5.md`: "`invisible_to_reads` (eager, defer): position, digests, contents equal before, after `demote()`, after `promote()`" |
| A6 | `docs/status/cold-tier-item5.md`: "then `promote()` and the same position accepted with `calls()` 0; defer off: accepted and maps" |
| A7 | `docs/status/cold-tier-item5.md`: "`answer_ends` (eager, defer): flag cleared; `demote()` between `promote()` and the retry refused" |
| A8 | open: carried to step 2's sweep, not run here in `docs/status/cold-tier-item5.md` |
| A9 | `docs/status/cold-tier-item5.md`: "then two 120-round runs on `work_pool_executor` (defer off and on)" |
| A10 | `docs/status/cold-tier-item5.md`: "at most one of each answer, at most 3 retries; the exact orders asserted" |
| A11 | `docs/status/cold-tier-item5.md`: "`reads_after_promote` (eager, defer): a lease-less read before maps and `unleased_maps()` rises" |

## Result

Landed at strata:081b7e7 on 2026-11-02. All 30 mutants died in the RelWithDebInfo `warm_test`. Reviews: `docs/reviews/cold-tier-item5-code.md` -- APPROVE TO LAND (2026-11-01, code strata:3918fdc; notes I5-N1 to I5-N5, none blocking); `docs/reviews/cold-tier-design.md` (re-check 4).

- Merge: Merge branch 'phase/cold-tier-item5' (warm indexes: demote, promote, map_owed).
- Landed as strata:3918fdc; design/K5 text strata:72a77f3.
- Crash tests cover 66 of 66 mutation points, both defer modes, on rel, ASan+UBSan and TSan.
- Rel ctest: 61 of 61; ASan+UBSan and TSan: 61 of 61, zero reports.
- Lazy rel ctest: 61 of 61; mac-hardened and mac-asan-ubsan preflight are ok.
- Linux and FreeBSD preflight are reported ok; CI is pushed and watched by the lead.
- The code review approved landing (`docs/reviews/cold-tier-item5-code.md`); item 6 and step 2 handle remaining work.
- Docs: `docs/mechanisms/warm-indexes.md`, `docs/mechanisms/defer-io.md`.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
