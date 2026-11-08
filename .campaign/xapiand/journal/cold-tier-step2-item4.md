# cold-tier-step2-item4: journal

State: landed
Quest: .campaign/xapiand/quests/cold-tier-step2-item4.md (the lead's; never edit it)

## Doing

Nothing: landed at strata:4795b7e on 2026-11-03 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/cold-tier-step2.md`: "WT `pin_after_promote` (3 rounds, a waiter resumed by the wake's own thread, an `apply` at the pin)" |
| A2 | `docs/status/cold-tier-step2.md`: "WT `chore_pin_commits` (an uncommitted delete, committed on `owed`, no wake, nothing queued)" |
| A3 | `docs/status/cold-tier-step2.md`: "WT `repost_through_timer` (13 reposts, the delays 1, 2, 4, 8, 16, 32, 64 ms" |
| A4 | `docs/status/cold-tier-step2.md`: "WT `bound_and_backoff` (persistent `EMFILE`, 8 failures, each backoff probed at its edge)" |
| A5 | `docs/status/cold-tier-step2.md`: "the 7th does not, the 8th does; no commit chore; `EMFILE` from the reopen" |

## Result

Landed at strata:4795b7e on 2026-11-03. Release, TSan and ASan+UBSan wake, schedule and stress tests pass; 31 of 31 mutants die. Review: `docs/reviews/cold-tier-step2-item4.md` (REJECT on I4-B1; fix round recorded).

- Merge: Merge branch 'phase/cold-tier-step2-item4' (the wake, chore pins and the bound; I4-B1, timer required).
- Landed as strata:38858ab, strata:be6236b, strata:1acee84, strata:f5b0ccf and strata:c0fbfec.
- Schedules: 300 seeds x 160 steps; 10,502 pins granted and 4,951 failed.
- Stress: 6,000 ops; 83 wakes, 4 reposts, 62 failures, 6 bound closes.
- Item 3 fix round merged; merged tests report zero sanitizer findings.
- Preflight was not rerun on the merged tree; CI is pushed and watched.
- Docs: `docs/mechanisms/catalog.md`, `docs/design/limits.md`.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
