# control-plane-wallclock: Replace test_control_plane's wall-clock p99 bound with a deterministic check

Risk: low. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status control-plane-wallclock`.

## Outcome

tests/test_control_plane.cc's `test_pending_keys_cost_the_loop_nothing` (:289) proves that pending keys cost the loop nothing without a wall-clock bound: the p99 <= 16 ms assertion failed on the FreeBSD CI VM (p99 28.7 ms, max 90 ms), so the test asserts a deterministic property instead, such as the loop doing no work per pending key (turns, wakeups or handler counts).

## Acceptance criteria

Three to five, at outcome level: `- A1 <criterion>: <the test that shows it>`. An untagged one
needs a job tagged `--for A1` passing at the landed code, often the suite; tag one `[artifact]`
(a committed `path`), `[decision]` (a PLAN.md Decisions id) or `[manual]` (checked by hand) when no
job can show it.

- A1 The test asserts a count-based property that fails if pending keys make the loop do work per key: the suite job, and a fails-without job on a mutant that adds per-key loop work
- A2 [manual] No wall-clock bound remains in that test; any timing kept is reported, not asserted: the lead reads the diff
- A3 GitHub CI is green on Linux, FreeBSD and arm64 at the landed commit: the land gate

## Contracts

None: a test-only change.

## Decisions

- Only the test changes. If the property can only be observed through a product hook that does not exist, stop and report.

## May land early

None: it lands whole.
