# x7-cancel-rows: T9 cancellation tests and a cancel counter (x7-gate A9)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-cancel-rows`.

## Outcome

T9 has tests and a counter, and T8's two peaks are exported (D56): a search whose client disconnects frees its reactor within one slice and raises a cancel counter that `GET /:metrics` shows, and a PUT whose client half-closes after sending completes durably.

## Acceptance criteria

One line each: `- A1 <criterion>: <the job, test or check that shows it>`. An untagged criterion
needs a job that passed at the landed code; tag one `[artifact]` (a committed `path`),
`[decision]` (an id in PLAN.md's Decisions table) or `[manual]` (checked by hand) only when a job
cannot show it.

- A1 A test disconnects a client mid-search and checks the reactor is free within one slice and the counter rose by one: a test in `tests/test_node.cc`, in the `build-test` check; it fails with the counter's increment removed (a mutant job)
- A5 A client leaving cancels a search still waiting in its first schema load (the schema read takes the caller's budget token): a cold-schema variant of the A1 test in `tests/test_node.cc`, in the `build-test` check
- A2 A test half-closes a PUT after its body and checks the document is readable after a restart: in the `build-test` check
- A4 `GET /:metrics` exports `xapiand_admission_inflight_peak` and `xapiand_scheduler_demoted_peak` (T8 reports both; `src/metrics.h:106-107` has only pinned-index gauges): a test in the `build-test` check reading both after an overload
- A3 `GET /:metrics` lists the counter, and the metrics docs name it: the `build-test` check, and a quote of docs/mechanisms in the journal

## Contracts

HTTP: a new counter in `/:metrics`; no other change.

## Decisions

- Lead, 11-10: found by x7-gate's inventory (A9); after write-path-fixes, which holds tests/test_node.cc.

## May land early

The tests alone, if they pass without the counter.
