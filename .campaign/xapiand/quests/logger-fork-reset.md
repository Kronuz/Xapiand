# logger-fork-reset: logger: a fork child that logs never blocks on the LOG thread's mutex

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status logger-fork-reset`.

## Outcome

A process that forks while the LOG thread holds its mutex can log in the child without blocking: a `pthread_atfork` handler (or equivalent) leaves the child's logger state consistent, so `wake_for` never waits on a mutex held by a thread that no longer exists (logger-exit-lifetime review R4, pre-existing on main).

## Acceptance criteria

- A1 A test forks while the LOG thread holds the mutex (forced with a test hook) and the child logs and exits within a bound; it hangs or times out without the fix (mutant job)
- A2 The parent's logging is unaffected across the fork; TSan clean
- A3 The suite and TSan pass and CI is green on all platforms

## Contracts

No API change.

## Decisions

- From hunt.md; filed by the lead 2026-11-19.

## May land early

None.
