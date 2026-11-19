# task-fire-pending-guard: debounce: fire_pending() from inside a fire is caught, not a hang

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-fire-pending-guard`.

## Outcome

debounce's `fire_pending()` called from inside a fire is caught by a Debug assert instead of hanging forever; the other review notes (N2 starvation before a stop, N3 concurrent fire of one key without a stop, N8 the 300 ms test window) are fixed or documented (reviews/task-lifecycle-followups-correctness.md).

## Acceptance criteria

- A1 A Debug test calls fire_pending() from inside a fire and observes the assert (death test or hook), not a hang; fails without the guard (mutant job)
- A2 N2 and N3 are either fixed with a test each or stated in docs/mechanisms/lifecycle.md as the contract
- A3 debounce_mid_fire no longer depends on a 300 ms window (a handshake instead); suite passes, TSan clean, CI green

## Contracts

No API change.

## Decisions

- From hunt.md; filed by the lead 2026-11-19.

## May land early

None.
