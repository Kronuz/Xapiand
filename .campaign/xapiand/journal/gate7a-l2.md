# gate7a-l2: journal

State: landed
Quest: .campaign/xapiand/quests/gate7a-l2.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:addacf386 on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/gate7a-l2.md`: "L2: T10 under TSan, clean (611 s and 307 s mixed loads)" |
| A2 | `docs/status/gate7a-l2.md`: "C1: `profile_bulk.sh` reports device-flush wait as its own line" |
| A3 | `docs/status/gate7a-l2.md`: "C2: `xapian_1` of hold 1 is marked invalid" |
| A4 | `docs/status/gate7a-l2.md`: "C4: traced to a term-prefix collision (shard-local docid 78 and the schema's `_version` field terms)" |
| A5 | `docs/status/gate7a-l2.md`: "`convert` ends on a self-linked tree; test `test_hate_groups_terminate`; fuzz watchdog and replay" |

## Result

Landed at Xapiand:addacf386 on 2026-10-29. TSan mixed loads: 611 s and 307 s; ctest 28 of 28 under TSan. Review: strata `docs/reviews/phase-7a-gate.md` named.

- Merge: Merge branch 'phase7a/gate-l2' (gate 7a L2, C1, C2, C4: every-gate checks).
- Xapiand:db2a9d813 profiling flush wait and rejecting truncated profiles; Xapiand:bf25fb518 mixed-load harness.
- Xapiand:1839276a4 Xapian `stored_version` reads value slot; Xapiand:0e01adabc converter termination and watchdog.
- ASan+UBSan docs E2E 566 of 566 on both engines; fuzz 1 CPU-hour each of four targets.
- Coverage 91.6% lines, 89.1% branches; T11 CPU 0.357x closing and 0.414x rerun.
- Open: queryparser duplicate-node cycle fix and CI/re-check.
- Docs: `docs/gates/phase7a.md` and `docs/dev/testing.md` named.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
