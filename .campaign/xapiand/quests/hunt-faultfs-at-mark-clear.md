# hunt-faultfs-at-mark-clear: faultfs clears a removed ordering point's at_mark record

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status hunt-faultfs-at-mark-clear`.

## Outcome

Hunt row (faultfs at_mark, from hunt-faultlab-at-mark's journal, faultlab:7f3910c): an ordering
point that fails before its effect removes itself but leaves its `at_mark` record on the file.
`settle_staged()` clears records only while some ordering point exists. First decide, with a test,
whether a later sync, crash or second ordering point can read the leftover record as staged (wrong
durable bytes after a simulated crash). If it can, fix it where the point is removed; if it cannot,
still clear the record there so no state outlives its point, and say why it was harmless.

## Acceptance criteria

- A1 The suite passes: the `build-test` check, a job tagged `--for A1`
- A2 A new case removes a failed ordering point and then syncs, crashes and adds a second point,
  checking the durable bytes by value; it fails without the fix (a `--fails-without` job)
- A3 [ci] CI is green on every platform

## Contracts

None: no API or behavior change.

## Decisions

- Lead, 2026-11-23: low risk, an exploration batch target; the lead reads the diff, no review.

## May land early

Nothing: one change.

## Tasks

None: one contributor (exploration, economical, medium).
