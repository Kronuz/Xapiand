# reclaim-bg: journal

State: landed
Quest: .campaign/xapiand/quests/reclaim-bg.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:45e6b32 on 2026-10-26 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/reclaim-bg.md`: "Tests: `reclaim_bg_test` (ctest `reclaim_bg`, `reclaim_bg_mapped`): posted (30 publishes with the deletes" |
| A2 | `docs/status/reclaim-bg.md`: "Revert and mutants (`.scratch/mut.sh`, logs `.scratch/mut-*.log`): each applied to `index.hh`, rebuilt" |
| A3 | `docs/status/reclaim-bg.md`: "full ctest, 53 tests, passes at HEAD in RelWithDebInfo, under ASan+UBSan (Debug -O1) and under TSan (-O1)." |
| A4 | `docs/status/reclaim-bg.md`: "run-phase throughput main 10,619 ops/s mean, branch 12,319 (1.16x main, every round faster" |

## Result

Landed at strata:45e6b32 on 2026-10-26. `reclaim_bg_test` covers 100 power-off points; reopen reclaims 362 truncated orphans. Review: `docs/reviews/reclaim-bg-code.md` (approved with conditions).

- Merge: Merge branch 'phase4/reclaim-bg'.
- Seeded crash sweep: 3,871 points and 1,707 truncated orphans; mapped sweep: 1,585 points.
- ctest: 53 tests pass in RelWithDebInfo, ASan+UBSan, and TSan.
- Eight A/B rounds: run phase 10,619 to 12,319 ops/s mean (1.16x); every round faster.
- Settled disk: 81,654,036 bytes and 10 files in both arms; get digest a46c3543d251394d in all 16 runs.
- Follow-up: scoped re-check of CR1 and CR2, then land.
- Docs: `docs/perf/reclaim-bg.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
