# phase7c-x2: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-x2.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:d25fb70a4 on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x2.md`: "[x] **K1 Pins.** task `9a85e89`, threadpool `e961e44`, strata `c82ac9f`" |
| A2 | `docs/status/phase7c-x2.md`: "[x] **K2 Registry and lease (D5, R4)** (`Xapiand:f4f0fd319`" |
| A3 | `docs/status/phase7c-x2.md`: "`engine::strata::Shard` opens with `defer_io` (`OpenOptions::host`); `apply_t` runs the owed flush or drain" |
| A4 | `docs/status/phase7c-x2.md`: "Open, refresh, commit, close, merge publication and reclaim are chores of the entry's `WorkGroup` (`IndexChores`)" |
| A5 | open: marked partial ([~]); handler and schema waits deferred to X2b in `docs/status/phase7c-x2.md` |
| A6 | `docs/status/phase7c-x2.md`: "Release and Debug (`Debug`, asserts on) builds link; unit tests **26 of 26** ctest targets on each" |
| A7 | `docs/status/phase7c-x2.md`: "[x] **K7** `docs/mechanisms/index-registry-and-lease.md`; changelog" |

## Result

Landed at Xapiand:d25fb70a4 on 2026-10-30. Release and Debug: 26 of 26 ctest targets each; docs E2E 641 of 641 assertions. Review: `docs/reviews/phase7c-x2-code.md` -- APPROVE WITH CONDITIONS (the code review, named by `docs/status/phase7c-x2b.md`); `docs/reviews/phase7c-design.md` is the design's.

- Merge: Merge branch 'phase7c/x2b-handlers-views' (7c X2 and X2b: index registry, turns, host mode, chores, reader views).
- Xapiand:f8657f445 pins; Xapiand:f4f0fd319 registry, lease, host mode, and chores; Xapiand:dcfdf5af8, Xapiand:afd7bf042, Xapiand:325f3e6d2, Xapiand:fcbeb8ebc, and final block-guard change.
- ASan/UBSan: 26 of 26 tests and E2E 641 of 641, no report; TSan concurrency tests 8 of 8, 0 warnings.
- TSan mixed load: 12,237 documents, model equals final dump, 0 failures, 0 sanitizer reports.
- Crash check recovered 9,711 documents; WAL recovery PASS (50/50); shutdown scenarios pass.
- stall_load.py dry run loaded 20,000 documents, 0 errors; no timing claim.
- K5 remains partial; X2b handles views and handler tasks.
- Docs: `docs/mechanisms/index-registry-and-lease.md`.
- Left open: A5.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
