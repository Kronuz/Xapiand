# task-inbox-first: task: cross-thread post_call before demoted handles; flush inline when off_loop cannot start

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-inbox-first`.

## Outcome

The task-lifecycle re-check's open defects are closed before any daemon adopts lifecycle: a cross-thread `post_call` is not starved by demoted handles (R1, task.hh:447; probe 261119-091929-77b81e ran a call posted at step 10 at step 10,000), the end-of-lifecycle flush runs inline when `off_loop` cannot start its thread (R2, lifecycle.hh:448-458,522-526), the owner-thread check in post_call is not a stale relaxed read (R4, task.hh:402), and service-shape.md drops `event::wait` (N8).

## Acceptance criteria

- A1 The inbox probe (reviewer's run.sh inbox) runs a cross-thread call within a bounded number of steps while the loop keeps re-posting demoted work; a test asserts the bound and fails with the old order
- A2 A test forces off_loop's thread start to fail and checks that every flush still runs, inline; it fails without the fix
- A3 The owner-thread check is correct when a second thread drives the scheduler after the first; a TSan test covers it
- A4 [artifact] docs/design/service-shape.md no longer lists `event::wait` in the step list
- A5 The suite and TSan pass and CI is green

## Contracts

No API change; task:lifecycle's behavior as reviewed, with these defects closed.

## Decisions

- From reviews/task-lifecycle-correctness.md Re-check (R1, R2, R4, N8). R3 and N4 wait on the owner.

## May land early

None.
