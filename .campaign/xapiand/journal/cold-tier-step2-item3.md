# cold-tier-step2-item3: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step2-item3.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:be5c5af on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step2.md`: "`catalog_test` `schedules`: 300 seeds x 160 steps" |
| A2 | `docs/status/cold-tier-step2.md`: "`single_flight` (8 requests, one open, all 8 counted when the first resumes)" |
| A3 | `docs/status/cold-tier-step2.md`: "`unpin_during_wake_end` (the hook releases a chore pin between the wake end's load and its store)" |
| A4 | `docs/status/cold-tier-step2.md`: "`resume_outside_mutex` (a scheduler-less waiter whose continuation takes every shard's mutex)" |
| A5 | `docs/status/cold-tier-step2.md`: "`close_and_reopen` (gen + 1 once; the stale handle counts and succeeds by key" |
| A6 | `docs/status/cold-tier-step2.md`: "`second_key_same_directory` (`EWOULDBLOCK`, one resolve, the negative entry answers next)" |
| A7 | `docs/status/cold-tier-step2.md`: "S3 (the sweep reads the index before its compare-exchange: data race)" |

## Result

Landed at strata:be5c5af on 2026-11-02. T6 schedules cover 300 seeds x 160 steps: 48,000 steps; all 18 state edges occurred. Review: `docs/reviews/cold-tier-step2-item3.md` (approve with conditions).

- Merge: Merge branch 'phase/cold-tier-step2-item3' (the catalog: slots, table, pins, the word's protocol; I3-B1, C16).
- Landed as strata:73b54cf, strata:0613c35, strata:bd6a815; review fixes strata:f407bd0 and strata:9476224.
- TSan stress: 4 threads x 1,500 ops; 5,950 plain and 5,855 deferred pins, zero reports.
- Release stress: 6 threads x 20,000 ops; 119,871 and 118,525 pins, zero stuck.
- Mutants: 26 of 26 edits killed, plus X2 and S3 in TSan stress.
- Preflight on the tested tree: mac-hardened 66/66, mac-asan-ubsan 66/66, Linux 67/67, FreeBSD 66/66.
- Docs: `docs/design/limits.md`, `docs/mechanisms/catalog.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
