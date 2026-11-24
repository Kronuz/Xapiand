# hunt-single-flight-race: test_single_flight waits for the leaving joiner's task, not a snapshot

Risk: low (a test). The lead writes this quest; contributors read it and never edit it.

## Outcome

`test_a_joiner_whose_token_fires_leaves_while_the_load_goes_on` checks `tasks.running() == 2` right
after B's `cancelled` count rises, but B's coroutine raises the count before it returns, so the task
count can still be 3 for a moment. It failed master's CI (run 45242486008) and journal-replaces-wal's
(45289446018). The check waits for the count with `eventually`, which still fails if B never leaves.

## Acceptance criteria

- A1 The test passes 300 runs in a row under a light hold at the fix (a job)
- A2 [ci] CI is green on every platform

## Decisions

- Lead, 2026-11-24: a test race, not a SingleFlight bug: the count is updated inside B's task before the
  task completes.

## May land early

Nothing: one line.
