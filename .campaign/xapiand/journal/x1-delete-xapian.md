# x1-delete-xapian: journal

State: landed
Quest: .campaign/xapiand/quests/x1-delete-xapian.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:0ae3505c9 on 2026-10-29 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/x1-delete-xapian.md`: "`src/xapian/` (180,386 lines), `src/engine/xapian/`, `fix_xapian_includes.py`" |
| A2 | `docs/status/x1-delete-xapian.md`: "`engine::Kind`, `kind_of`, `default_kind`, `parse_kind`, `src/engine/kind.*`, `XAPIAND_ENGINE`" |
| A3 | `docs/status/x1-delete-xapian.md`: "Deleted: `src/server/{remote,replication}_protocol_*`, `discovery.*`, `raft_delegate.h`" |
| A4 | `docs/status/x1-delete-xapian.md`: "Kept, unopened. Users: `serialise.h` (numeric, date and time fields)" |
| A5 | `docs/status/x1-delete-xapian.md`: "Release and Debug build with no errors. Unit tests: 25 of 25 ctest targets pass on Debug" |
| A6 | `docs/status/x1-delete-xapian.md`: "Changelog `[Unreleased]` has four `Removed` bullets; one row added to `docs/dev/extraction-ledger.md`" |

## Result

Landed at Xapiand:0ae3505c9 on 2026-10-29. Release and Debug builds complete without errors; 25 of 25 ctest targets pass on Debug. Review (not named in the ledger): `docs/reviews/x1-delete-xapian-code.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'phase7c/x1-delete-xapian' (Phase 7c X1: Xapian and the cluster deleted).
- Source, CMake, CI, docs, and harness conversion: Xapiand:5a517b052, Xapiand:6dcbb3686, Xapiand:1a0964e2d.
- Extraction ledger row: Xapiand:621dd690b; converted tests: Xapiand:1848ed81e to Xapiand:6dfd8793a.
- Release docs E2E: 371 requests, 641 of 641 assertions; master: 377 requests, 649 of 649.
- ASan/UBSan docs E2E: 641 of 641; ctest: 25 of 25, no sanitizer report.
- Xapian tree removed: 180,386 lines; two findings remain for the lead, including one-shard registry merge failure.
- Left alone as historical material: docs/design, reviews, status, perf, gates, mechanisms, and most docs/dev.
- Docs: `docs/dev/extraction-ledger.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
