# x7-scan-slices: prospect's scan yields every 256 ordinals when per-match work runs

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status x7-scan-slices`.

## Outcome

`exhaustive_search::scan` checks its slice budget every 256 ordinals when an aggregation or sort key
runs per match, so one window no longer holds a Xapiand loop for ~2.3 ms (x7-loop-stalls cause 2,
Xapiand docs/perf/x7-loop-stalls.md, job 261111-221539-18ce81). Scans with no per-match work keep
the 4,096-ordinal window.

## Acceptance criteria

- A1 A sliced scan with a per-match collector yields at most every 256 ordinals and returns the same results and aggregation values as an unsliced scan: a test in `test/sliced_test.cc` (the build-test check), which fails with the check reverted (a `--fails-without` job)
- A2 A scan with no per-match work is not slower: one strict job of at most 10 minutes, before and after at named commits, honest magnitude
- A3 [artifact] The before/after: `docs/perf/scan-slices.md`

## Contracts

None beyond prospect's existing scan API; no signature changes.

## Decisions

- Lead, 11-11: split from x7-loop-stalls (Xapiand's quest lands after this one and bumps the pin).

## May land early

Everything.
