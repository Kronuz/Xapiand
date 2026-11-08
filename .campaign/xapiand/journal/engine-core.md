# engine-core: journal

State: landed
Quest: .campaign/xapiand/quests/engine-core.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:566943e9e on 2026-10-21 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/engine-core.md`: "X2 is complete in two halves. Half A (7eae6786c..d77cf2e0b): types, errors, `Document`, `DocView`" |
| A2 | `docs/status/engine-core.md`: "Migrated: `schema.{h,cc}` (no `Xapian::` left in schema.cc), `generate_terms.{h,cc}` document side" |
| A3 | `docs/status/engine-core.md`: "E2E 339 requests, 576/576 assertions GREEN; `e2e_diff` GREEN against the X1 final capture" |

## Result

Landed at Xapiand:566943e9e on 2026-10-21. Half A: 339 requests, 576/576 assertions; 0 E2E status differences; WAL recovery 50/50. Review: `docs/reviews/engine-core.md` -- CR1-CR4 corrected or superseded; CR8 pre-existing, separate fix.

- Merge: Merge branch 'phase0.5/engine-core'.
- Xapiand:7eae6786c -- seam, types, errors, `Document`, `DocView`, and write-path migration.
- Xapiand:3be508883 -- `engine::Shard`, Xapian implementation, lifecycle migration.
- Half A: `test_engine_document` 840 checks; ASan+UBSan unit test 840/840.
- Half B: 339 requests, 576/576 assertions; inline and offload runs; WAL recovery PASS.
- Half B: `test_engine_shard` 52 checks; seam metric 56 / 1614 to 52 / 1297 files / lines.
- Open: reads still use `Shard::db()` pending X4; retry rules lack a test; remote-protocol teardown remains outside the blocking rule.
- Docs: `docs/design/xapiand-on-strata.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
