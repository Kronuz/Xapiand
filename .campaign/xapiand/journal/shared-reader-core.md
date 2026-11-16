# shared-reader-core: journal

State: landed
Quest: .campaign/xapiand/quests/shared-reader-core.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:c576997 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/shared-reader-core.md`: "`6373595` item 1: `strata/core/hazard.hh`: H records, each on its own line." |
| A2 | `docs/status/shared-reader-core.md`: "`302a414` items 2 and 4: `deleted_bits` is copy-on-write in one allocation (D3)." |
| A3 | `docs/status/shared-reader-core.md`: "`461ac14` `bench/shared_reader/acquire.cc`, T3's single-index arm as far as step 1 goes" |
| A4 | `docs/status/shared-reader-core.md`: "Review fixes (CR1 to CR3, plus CR6's test), each with its revert check:" |

## Result

Landed at strata:c576997 on 2026-10-21. ASan+UBSan ctest 35 of 35; TSan ctest 35 of 35; mutants 13 of 13 killed. Review: `docs/reviews/shared-reader-core.md` -- approved with conditions.

- Merge: Merge remote-tracking branch 'origin/phase4/shared-reader-core'.
- `strata:6373595`: hazard records and tests; `strata:302a414`: deleted bits, owner cleanup, shared reads and snapshot; `strata:461ac14`: T3 benchmark.
- T8 seq_cst: 0 free-while-read outcomes in 10^7 retry and 2.5×10^6 claim iterations; weakened orders produced failures.
- Snapshot differential: 3 seeds, about 115 publishable points and about 17,500 gets each; 0 segments or shares dropped off owner.
- T3 acquire+release at 8 threads costs 1.10× and 1.08× 1-thread; guard adds 1.0% and 5.9% to get at 8 threads.
- Shared_ptr arm costs 58× hazard pair at 8 threads; cross-thread registry adds about 90 ns over hand-off control.
- Review approved with conditions; CR1-CR3 fixed; CI not green when session stopped.
- Docs: `docs/design/shared-reader.md` and `docs/reviews/shared-reader-design.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
