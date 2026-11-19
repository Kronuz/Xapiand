# task-driver-layout: task: inline_scheduler layout identical in Debug and release (F7)

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-driver-layout`.

## Outcome

inline_scheduler has one layout in every build: the one-driver member (`driver_`) exists whatever NDEBUG is, and only the check is Debug-only (task-inbox-first review F7). A Debug sanitizer build linking a Release library otherwise sees two layouts of one class (an ODR break); Detent's adoption waits on this.

## Acceptance criteria

- A1 A test (static_assert or a two-TU build, one with NDEBUG and one without) shows sizeof/offsets of inline_scheduler are identical with and without NDEBUG, and fails at task:b7e2449
- A2 The two-drivers assert test still aborts in Debug
- A3 The suite passes and CI is green

## Contracts

No API change.

## Decisions

- Detent's lead treats F7 as blocking their adoption (2026-11-19).

## May land early

None.
