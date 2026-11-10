# x7-coverage: Coverage harness and restore corpora for T13 (x7-gate A13)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-coverage`.

## Outcome

T13 can run: a coverage harness reports line coverage against TESTING.md's targets, and the restore corpora T13 names exist (dumps written by the previous release and by current master, replayed by RESTORE and compared document by document).

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 `harness/coverage.sh` builds with coverage and prints line coverage per area beside TESTING.md's targets: a job
- A2 Restore corpora are committed under tests/corpora/ with the build that wrote each, and a harness replays each through RESTORE and compares every document: a job
- A3 TESTING.md names both harnesses: [artifact] TESTING.md

## Contracts

None.

## Decisions

- Lead, 11-10: found by x7-gate's inventory (A13): no restore corpora and no coverage harness exist. Corpora stay small (under 5 MB each).

## May land early

Everything (harnesses and test data).
