# strata-races: Two strata races found as test flakes are fixed

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status strata-races`.

## Outcome

Two races found as flakes are fixed (prototype: strata explore/flakes, `docs/perf/flakes.md`):
1. A close's end lowers the catalog's `closing_` before `live_`, and `closing_over()` reads `live_`
   with acquire, so a close pass never stops one close short (`catalog.hh` `note()`, `closing_over()`).
2. `resource_budget`'s `tick()` never leaves the external cap raised past a concurrent failure at
   its count (`resource_budget.hh` `note_failure`, `tick`).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 `burst_closes` under load (20 waves x 16 processes, `docs/perf/flakes/repeat_load.sh`) fails
  0 of 320 at the head; the same run at main failed 11 of 320 (261111-052507-6df290), tagged
  `--fails-without` for the main run.
- A2 A deterministic test (the clock hook) fails without the `tick()` fix and passes with it; the
  TSan load run fails 0 of 320 at the head (main: 3 of 320, 261111-053734-7d63a2).
- A3 [artifact] The memory orderings are argued in a comment at each changed load and store (one
  line each): `include/strata/core/catalog.hh`.
- A5 `closing_over()`'s descriptor and vnode relief reads no stale order: `retire` unbooks the index
  (`finish_close`) before it drops it, or the relief reads in the order that keeps a pass from
  stopping short (docs/perf/flakes.md "Open"); a test or load run that fails without the change.
- A4 The strata suite and its TSan build pass at the head, with CI green.

## Contracts

None changed.

## Decisions

Lead: both fixes in one quest and one review (one reviewer, concurrency questions).

## May land early

The deterministic tests.
