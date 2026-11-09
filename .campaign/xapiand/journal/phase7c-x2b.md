# phase7c-x2b: journal

State: landed
Quest: .campaign/xapiand/quests/phase7c-x2b.md (the lead's; never edit it)

## Doing

Nothing: landed at Xapiand:d25fb70a4 on 2026-10-30 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/phase7c-x2b.md`: "[x] **K1. No `sync_wait` on a server path.**" |
| A2 | `docs/status/phase7c-x2b.md`: "[x] **K2. Reader views replace `core.mtx` and `Pinned`.**" |
| A3 | `docs/status/phase7c-x2b.md`: "[x] **K3. C16's held-guard test.** `test_released_views_are_reclaimed_by_the_housekeeping`" |
| A4 | `docs/status/phase7c-x2b.md`: "[x] **K4. `too_many_readers` is a 503.**" |
| A5 | `docs/status/phase7c-x2b.md`: "Unit tests, 26 of 26 ctest targets on **Release**, **Debug** (asserts on, `-Werror=return-type`) and **ASan+UBSan**" |
| A6 | `docs/status/phase7c-x2b.md`: "[x] **K6.** `docs/mechanisms/reader-views.md` (new), the registry, search and fields notes updated" |
| A7 | `docs/status/phase7c-x2b.md`: "[x] **R1. A registry per index.**" |
| A8 | `docs/status/phase7c-x2b.md`: "[x] **R2. The registry outlives every guard.**" |
| A9 | `docs/status/phase7c-x2b.md`: "[x] **R3. `max_indexes_per_search`.**" |
| A10 | `docs/status/phase7c-x2b.md`: "[x] **R4. The N2 rule** is written in `reader-views.md`" |

## Result

Landed at Xapiand:d25fb70a4 on 2026-10-30. Release, Debug, and ASan+UBSan: 26 of 26 ctest targets; TSan concurrency targets 9 of 9. Review: `docs/reviews/phase7c-x2b-code.md` -- APPROVE WITH CONDITIONS.

- Merge: Merge branch 'phase7c/x2b-handlers-views' (7c X2 and X2b: index registry, turns, host mode, chores, reader views).
- Xapiand:6863b1076, Xapiand:690d2cfc3, Xapiand:1afabbd06, Xapiand:0ab64f5dd, Xapiand:b19f4d17f, Xapiand:393de1dc9, Xapiand:70c40cc78.
- Docs E2E: 641 of 641 assertions; TSan mixed load 12,793 documents, model equals final dump, 0 failures.
- Registry sizing: 174 records on a 16-CPU host; 11,136 B per open index, 22,272 B on arm64.
- Fanout HTTP check: 1,010 -> 200, 1,011 -> 400; default 200 -> 200, 201 and 1,010 -> 400.
- Ruling proof: TSan mixed load 10,135 documents, 0 misses and 0 exceptions in read-your-writes probe.
- Review fixes and ruling are recorded; carried items X3-1 to X3-5 remain for X3.
- Docs: `docs/mechanisms/reader-views.md`, `docs/mechanisms/index-registry-and-lease.md`, `docs/design/phase7c.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
