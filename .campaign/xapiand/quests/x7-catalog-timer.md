# x7-catalog-timer: Run the catalog's timer on the control loop (x7-gate T1 census)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-catalog-timer`.

## Outcome

The catalog's timer (`src/database/pool.cc:423`, the `Xapiand:DB-TIMER` thread x-pool added) arms on the control loop (`engine::Host::start_timer`), so the process has only the threads `docs/design/phase7c.md` section 2 lists and x7-gate's T1 census passes. One timer mechanism, no new thread (reuse before new timers).

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 The `Xapiand:DB-TIMER` thread is gone and the catalog's timed work (its expiries and negative-TTL sweeps) runs on the control loop's timers (`engine::Host::start_timer`): x7-gate's `harness/thread_census.sh` row as a job, failing at master
- A2 The catalog's timed behaviour is unchanged: the pool tests (test_pool's TTL and erase cases) in the `build-test` check, plus a mutant job that drops the timer's arming and must fail them
- A3 [artifact] The how-it-works note for the catalog names the control loop's timer: a quote in `docs/mechanisms/`

## Contracts

None; internal.

## Decisions

- Lead, 11-10 (D56, corrected by D58): no debouncer wheel remains (the autocommit debouncer is `control::Debounce` on the control loop; the fsync wheel retires in 7b), so the catalog's timer arms on the control loop with `engine::Host::start_timer`; the pool is built after, and reset before, the control loop.

## May land early

None.
