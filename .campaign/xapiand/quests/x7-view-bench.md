# x7-view-bench: Shard::view() benchmark at 1 and 8 threads (x7-gate A22)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-view-bench`.

## Outcome

`Shard::view()` has a benchmark at 1 and 8 threads on one index, and its numbers are in docs/perf/shard-view.md, so x7-gate's A22 (X3-4) can cite them.

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 A benchmark target measures `Shard::view()` per call at 1 and 8 threads on one index: a `strict` job, the numbers and commit in docs/perf/shard-view.md
- A2 The build and unit tests pass: the `build-test` check

## Contracts

None.

## Decisions

- Lead, 11-10: found by x7-gate's inventory (A22).

## May land early

Everything (a benchmark and a note).
