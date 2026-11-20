# loop-yield-tsan: Fix the TSan race in test_loop_yield's count_delete

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status loop-yield-tsan`.

## Outcome

test_loop_yield runs clean under TSan: the data race between `count_delete` (tests/test_loop_yield.cc:78) and the access at :1239, seen in CI run 44056420626 at code-identical 37eda4ed0 (hunt F6), is removed at its cause, so it no longer turns an unrelated landing's CI red.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The race's cause is named in the journal with file:line, and fixed in the test's synchronization without loosening or removing any assertion: the suite job
- A2 test_loop_yield under TSan passes 20 repeats in one job (`ctest -R loop_yield --repeat until-fail:20` in a `-fsanitize=thread` build), and a job at the pre-fix test reproduces the report or the journal says why it cannot be made to
- A3 GitHub CI is green on Linux, FreeBSD and arm64 at the landed commit: the land gate

## Contracts

None: a test-only change.

## Decisions

- If the race is in product code (src/), not the test, stop and report: the lead files a quest for it.

## May land early

None: it lands whole.
