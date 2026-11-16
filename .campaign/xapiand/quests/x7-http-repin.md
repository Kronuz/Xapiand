# x7-http-repin: Repin http to main with perf-watch-peek (x7-gate A24)

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-http-repin`.

## Outcome

Xapiand pins Kronuz/http at its main head, which includes perf-watch-peek (http:51d153a), so x7-gate's T12 and A24 measure the keep-alive watch without its extra read. Nothing else changes.

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 The `http_lib` GIT_TAG in CMakeLists.txt is http main's head and includes http:51d153a: `harness/pins_check.py` in the `build-test` job, and the diff
- A2 The build, unit tests and docs E2E pass: the `build-test` and `e2e` checks, and CI is green

## Contracts

Consumes http main (perf-watch-peek's readiness wait). Provides nothing new.

## Decisions

- Lead, 11-10: found by x7-gate's inventory (A24).

## May land early

None.
