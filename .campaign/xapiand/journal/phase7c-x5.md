# phase7c-x5: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-x5.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:d3f6cc1fd on 2026-11-02 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x5.md`: "[x] **K1. The caches hold no lock across I/O; concurrent first use of one schema loads it once.**" |
| A2 | `docs/status/phase7c-x5.md`: "[x] **K2. The control plane's work runs as tasks on the control loop; nothing blocks it.**" |
| A3 | `docs/status/phase7c-x5.md`: "[x] **K3. Inventory J classified.** `harness/async_check.sh --strict --extern strata=<strata>`: **0 unclassified**" |
| A4 | `docs/status/phase7c-x5.md`: "C10: `test_pool.cc` `test_a_chore_resumes_its_awaiter_with_post` counts `post` and `post_demoted`" |
| A5 | `docs/status/phase7c-x5.md`: "through ctest: Release, Debug and ASan 29 of 29, TSan 11 of 11 with 0 reports." |

## Result

Landed at Xapiand:d3f6cc1fd on 2026-11-02. Release, Debug, and ASan/UBSan ctest: 29 of 29; TSan concurrency targets: 11 of 11, 0 reports. Review: `docs/reviews/phase7c-x5-code.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'phase7c/x5-control' (7c X5: single-flight caches, the control plane on its loop).
- Xapiand:17bcf2f1b, Xapiand:ebdd26b19, Xapiand:f337d1e05, Xapiand:f7543779f, Xapiand:451d88b80, Xapiand:32ee5feb2, Xapiand:7150e3cca, Xapiand:1a6e3e140, Xapiand:e6c18eada.
- Docs E2E: 641 of 641 assertions; graceful shutdown, WAL recovery, and CLI roundtrip checks PASS.
- TSan mixed load: 11,809 documents, final dump equals model, 0 failures, 0 sanitizer reports.
- Housekeeping probe at 10,000 entries: call 0.001 to 0.006 ms; drained in 3.0 to 4.7 ms; max ticker gap 1.2 to 1.5 ms.
- Shutdown abandon test logs 5 acknowledged PUTs and verifies all 5 after restart; bound 1.5 s, logged 1508 ms.
- Review blocker and N1, N4, N5 are fixed; remaining lead findings include --strict --owed and shutdown-order choices.
- Docs: `docs/mechanisms/control-plane.md`, `docs/src/content/docs/options.md`, `docs/src/content/docs/changelog.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
