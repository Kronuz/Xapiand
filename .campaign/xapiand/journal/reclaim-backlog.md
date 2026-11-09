# reclaim-backlog: journal

State: landed
Quest: .campaign/xapiand/quests/reclaim-backlog.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:b3dcc48 on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/reclaim-backlog.md`: "The step rule: a publish reclaims at least what it obsoleted, in `reclaim_step` (16 MiB) truncations" |
| A2 | `docs/status/reclaim-backlog.md`: "What it keeps: `retain()` runs first, so a file a kept manifest names" |
| A3 | `docs/status/reclaim-backlog.md`: "Driver (`bench/kv/kv_strata.cc`): `settle()` calls `reclaim()` by default" |
| A4 | `docs/status/reclaim-backlog.md`: "80 rounds of merges, no commit, `reclaim_slice` 0: disk segment files equal the live segments after every publish" |
| A5 | `docs/status/reclaim-backlog.md`: "Mutants: reclaim before retain fails test 1; queueing sources directly fails test 2" |

## Result

Landed at strata:b3dcc48 on 2026-10-21. Full ctest 39 tests under ASan+UBSan and TSan; after round 1, 40 tests under ASan+UBSan. Review: `docs/reviews/reclaim-backlog.md` -- approve with conditions.

- Merge: Merge remote-tracking branch 'origin/phase4/reclaim-backlog'.
- Engine adds obsoleted bytes on merge install/discard; each publish pays bounded reclaim debt after retention.
- `strata:reclaim_backlog_test` tests 80 merge rounds; main has 79 dead files; fixed path leaves queue empty.
- Revert check fails with "worst dead files on disk 79"; pending-source mutant aborts on live mapping.
- Digests identical in all 24 runs: sizes1 `a46c3543d251394d`, big1 `1ac557eb7ac4608e`.
- sizes1 disk after settle: A/A2 681-759 MB, B 79-87 MB; B run ops/s 7288-7651.
- big1 disk after settle: A/A2 187-235 MB, B 72.3 MB; B run ops/s 388k-395k.
- Executor deletion remains open on `phase4/reclaim-bg`; review re-check pending.
- Docs: `docs/design/limits.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
