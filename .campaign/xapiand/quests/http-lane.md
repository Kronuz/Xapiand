# http-lane: http resumes a waiter inline only for a light reference

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status http-lane`.

## Outcome

Kronuz/http's `resume_waiter` (http_asio.h:578) resumes a waiter inline only for a light `scheduler_ref`; a heavy one is posted through its scheduler, so a heavy request's reads join the heavy lane (loop-scheduling D5). No change for callers that never set a lane.

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 A heavy waiter's read resumes through its scheduler and a light one inline: a test in http's suite that fails without the change (the build-test check, with a `--fails-without` job)
- A2 http's suite passes against the task-lanes pin: the build-test check

## Contracts

Consumes `task:lanes` (after task-lanes). No API change.

## Decisions

- Lead, 11-12: one line plus its test, per loop-scheduling D5.

## May land early

None.
