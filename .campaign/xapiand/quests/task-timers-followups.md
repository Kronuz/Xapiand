# task-timers-followups: fire_pending's hang, starvation and double fire

Risk: high (concurrency). The lead writes this quest; contributors read it and never edit it. Its
machine state is in `state.json`: `campaign status task-timers-followups`.

## Outcome

reviews/task-lifecycle-followups-correctness.md N1/N2/N3/N8 in `task_timers.hh`: `fire_pending()`
called from inside a fire hangs forever (a Debug assert, not just a comment); before a stop,
back-to-back loop fires can starve `fire_pending()` (it waits for `firing_ == 0` across all keys);
without a stop, a touch after the take can fire a key on the loop while `fire_pending()` fires it
on another thread; `debounce_mid_fire` gives the other thread 300 ms, which is spurious on a loaded
host (the test waits on an event, not a time).

## Acceptance criteria

- A1 `fire_pending()` from inside a fire asserts in Debug: a death or flag test
- A2 Back-to-back loop fires cannot starve `fire_pending()`, and no key fires twice concurrently:
  tests with values, each with a mutant (`--fails-without`)
- A3 `debounce_mid_fire` passes 50 runs under load with no timing margin: a job running it 50 times
- A4 The suite and TSan pass: the suite and tsan jobs
- A5 [ci] CI is green on every platform

## Contracts

Provides task:timers (behavior only; no API change).

## May land early

Nothing: one landing.
