# loop-yield-race: Fix test_loop_yield's TSan race (F6) and the strata search timing bound

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status loop-yield-race`.

## Outcome

Two flaky tests stop turning unrelated landings red:
1. TSan reports a race in `test_loop_yield`'s `count_delete` (tests/test_loop_yield.cc:78 against :1239). It failed CI runs 44056420626 and 44344005112, the latter on a harness-only Python change.
2. `test_engine_search_strata.cc:1406` checks `elapsed < 800 ms`; job 261118-080957-d0cf4c failed it on a loaded host.

Find the cause, not a wider bound. If the race is in src/ rather than in the test, stop, write it under Blocked on with the TSan report, and the lead re-files it at high risk with its files.

## Acceptance criteria

- A1 The TSan race is gone: a job runs `test_loop_yield` under TSan 20 times with no report, and the same job at master reproduces it at least once (or the journal shows the race's interleaving forced by a test that fails without the fix).
- A2 `test_engine_search_strata` no longer depends on wall time under load: the 800 ms check measures what it claims (work counted, or a bound derived from an unloaded control in the same run), shown by a job that passes with 4 parallel CPU burners beside it.
- A3 The build-test check passes, and CI is green on every platform.

## Decisions

A timing bound may stay only if the journal states the mechanism it guards and why load can't break it.

## May land early

None.
