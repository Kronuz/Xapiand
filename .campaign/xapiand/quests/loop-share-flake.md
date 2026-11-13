# loop-share-flake: test_loop_yield's RESTORE loop share holds on any core count

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status loop-share-flake`.

## Outcome

test_loop_yield's RESTORE check (tests/test_loop_yield.cc:383, `loop_ms < process_ms * 0.45`) passes on any runner, so master CI is green again. It fails on GitHub's runner (master run at 53ce159e1 and xapiand-strata-bump run 43278343734: loop 331.7 of 723.6 ms, 45.8%) because the loop's share depends on how many worker threads the machine gives the prepares. The check compares against the same RESTORE on the serial path in the same process (`DocIndexer::seams.serial`), so it measures that prepares moved off the loop, not the core count.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The loop-share check is relative to the serial path's share measured in the same test, and passes in GitHub CI's Build + unit + smoke job: CI
- A2 The check still fails when prepares run on the loop: a `--fails-without` job (prepares forced onto the loop)

## Contracts

None.

## Decisions

- Lead, 11-12: master is red on this ratio; fix forward. Overlaps x7-loop-fix-c's tests/test_loop_yield.cc, which merges master after this lands.

## May land early

None.
