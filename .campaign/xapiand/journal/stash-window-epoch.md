# stash-window-epoch: journal

State: landed
Quest: .campaign/xapiand/quests/stash-window-epoch.md (the lead's; never edit it)

## Doing

Nothing: landed at stash:351ef2e on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/perf-window-epoch.md`: "Owner's decisions (d234d24): fixed window width (adaptive shrink removed)" |
| A2 | `docs/status/perf-window-epoch.md`: "Model on the fixed-width variant: no violation in 6 configurations" |
| A3 | `docs/status/perf-window-epoch.md`: "TSan and ASan+UBSan on `test.cc` and `concurrent.cc` (lead 0 and 60, 5 runs each): clean." |
| A4 | `docs/status/perf-window-epoch.md`: "Starved stress, 40 interleaved runs at lead 0 and 60: 0 failed, 0 lost on main and fixed." |
| A5 | `docs/status/perf-window-epoch.md`: "A/B against main, 5 interleaved rounds under a strict hold: no difference beyond the spread" |
| A6 | `docs/status/perf-window-epoch.md`: "DW11, fail-fast: value box before the fetch-add claim, terminate after it" |
| A7 | `docs/status/perf-window-epoch.md`: "C3 fixed: every trace in `add` now runs before the leaf claim" |
| A8 | open: Next 2 in the ledger in `docs/status/perf-window-epoch.md` |

## Result

Landed at stash:351ef2e on 2026-10-20.

- Review: `review-wt/stash/docs/reviews/window-refusals-code.md` -- APPROVE WITH CONDITIONS (four branches: stash, scheduler, logger, Xapiand). That file exists only uncommitted on the laptop; this audit prepares it as stash `docs/reviews/window-refusals-code.md`.
- Results: `docs/perf/window-epoch.md`.
- Docs: `docs/perf/window-epoch.md`.
- Left open: A8.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
