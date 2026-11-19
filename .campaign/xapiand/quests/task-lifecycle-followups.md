# task-lifecycle-followups: task: lifecycle follow-ups and debounce touch/throttle/fire_pending

Risk: high. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-lifecycle-followups`.

## Outcome

task's open lifecycle follow-ups and D5's debounce additions; high because `fire_pending` is the fsyncher's third-signal path. Plan: docs/design/lifecycle-adoption.md quest 1 (about 450 lines). After task-driver-layout.

## Acceptance criteria

- A1 F6: a `post_call` landing as `run()` exits runs on the next `run()`, counted by `queued()`: test
- A2 F3 (Q3): the inline-flush fallback is reported in `stopped`: test, thread start failed by a hook
- A3 R3 (Q1): the `fail()` that caused the stop is `stopped.failure` though a later drain throws: test
- A4 N4 (Q2): `stopped.abandoned` is set when a drain reached `abandon`: test
- A5 D5's flags, throttle and `fire_pending()`: tests with mutants; hooks pin `fire_pending()` against a key mid-fire on the loop (fired once) and a touch after `stop` (returns false). Suite, TSan and ASan

## Contracts

Provides: `task::debounce` `touch(key, flags)` (flags OR-ed into the pending entry and passed to `fire(key, flags)`), `timing::throttle` (keeps a fired key's entry until `throttle` passes, bounded by the keys fired in the last `throttle`), `fire_pending()` (takes the pending keys and fires them on the calling thread); a `stopped` field for the inline-flush fallback, `stopped.failure` as the causing `fail()`, `stopped.abandoned`. Consumed by fsyncher-on-task and xapiand-lifecycle.

## Decisions

- D5 (debounce additions with today's fsyncher timing), D8 (latched flush; `fire_pending` serves the third signal).
- Q1 (R3): recommended (a), the causing `fail()` wins. Q2 (N4): recommended (a), add `stopped.abandoned`. Q3 (F3): recommended yes, report the fallback in `stopped`. Open with the owner; the criteria assume the recommendations.

## May land early

None.
