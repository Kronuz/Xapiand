# queue-drain-throughput: Keep light slots busy around exclusive drains

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status queue-drain-throughput`.

## Outcome

The job queue keeps the host's light slots busy while exclusive jobs wait. Today (2026-11-19 17h) light slots were 29% used with 102 jobs queued (34 exclusive): each exclusive drains the host for its whole wait, and the drain window still admits new heavy builds and priority-1 suites (hunt: 'drain window starves itself', 11-18), so drains ran 38 min of the hour. Queue health also ignores CPU charges (hunt 11-18). When this lands, exclusive jobs are batched back to back inside one drain, a drain admits no new work it would have to wait for, and status reports slot use by charged cores.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 Consecutive queued exclusive jobs run back to back inside one drain instead of a drain each, while light work runs between batches; a drain stops admitting builds and jobs that would outlast it: a test in tests/ that fails on main (a fails-without job at main's commit) and passes at the fix
- A2 Exclusive isolation is unchanged: no light or strict hold runs while an exclusive job runs, and an exclusive job still waits for every hold to end and for the load to settle: tests/ cases, the suite job
- A3 `campaign status` slot use and 'runnable work beside idle slots' count charged slots (a light job's --cpus charge), not job counts: a test, the suite job
- A4 A replay of today's queue shape (34 exclusive, 43 light, 10-45 min each) in the test harness's simulated clock shows light-slot use and total drain minutes before and after, recorded in the journal: a job

## Contracts

`quiet` holds and levels keep their meaning (references/host.md); only scheduling order changes.

## Decisions

- The lead maintains the campaign skill (owner, 2026-11-17); the suite must be green before landing.
- Never weaken exclusive isolation to raise utilization.

## May land early

None.
