# scheduler-lazy-rearm: journal

State: landed
Quest: .campaign/xapiand/quests/scheduler-lazy-rearm.md (the lead's; never edit it)

## Doing

Nothing: landed at scheduler:2cbea95 on 2026-10-20 under the earlier campaign skill.

## Next

1. Nothing: landed.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `docs/status/perf-lazy-rearm.md`: "`bench/bench_churn.cc` (b4f4895, 9751d22, 93d6f9e; made opt-in in the CI fix), the re-arm in" |
| A2 | `docs/status/perf-lazy-rearm.md`: "on 1,000 hot keys 507,000 tasks alive and 99-108 MB of heap become 1,002 and 1.5 MB" |
| A3 | `docs/status/perf-lazy-rearm.md`: "TSan and ASan clean locally; CI green on all 8 jobs (run 39382526110)." |
| A4 | `docs/status/perf-lazy-rearm.md`: "are `noexcept` and return `schedule_result { scheduled, past_horizon, stopping, no_memory }`" |
| A5 | `docs/status/perf-lazy-rearm.md`: "Downstream: Xapiand's 17 TUs that use the scheduler, a debouncer or the logger build against" |
| A6 | `docs/status/perf-lazy-rearm.md`: "C1 fixed (a touch on a pending key after `finish()` returns `stopping` and releases the key;" |

## Result

Landed at scheduler:2cbea95 on 2026-10-20.

- Review: `review-wt/stash/docs/reviews/window-refusals-code.md` -- APPROVE WITH CONDITIONS (prepared by this audit as stash `docs/reviews/window-refusals-code.md`).
- Docs: `docs/perf/lazy-rearm.md`.
- Left open: none.

## Rulings I made

None beyond the quest file's Decisions (the ledger is the record).
