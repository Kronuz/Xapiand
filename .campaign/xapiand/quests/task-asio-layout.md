# task-asio-layout: task_asio: cancellable_asio_awaiter layout identical in Debug and release

Risk: normal. The lead writes this quest; contributors read it and never edit it. Its machine state
(status, branch, checks, reviews) is in `state.json`: `campaign status task-asio-layout`.

## Outcome

`cancellable_asio_awaiter::on_loop_` (task_asio.hh:504-506) exists in every build, with only its check Debug-only, so the awaiter has one layout whatever NDEBUG is (task-driver-layout review F1, the same ODR hazard as F7). Inline functions whose bodies differ by NDEBUG (`orphaned_waiters()`, `event::~event`, review F2) are reviewed in the same pass: keep them NDEBUG-independent or document why the difference is harmless.

## Acceptance criteria

- A1 The layout test (test/layout_*.cc) also compares cancellable_asio_awaiter's size and alignment across NDEBUG and non-NDEBUG TUs, and fails at a revert commit
- A2 [artifact] `task_asio.hh` has no NDEBUG-conditional data member left
- A3 The suite passes and CI is green

## Contracts

No API change.

## Decisions

- From reviews/task-driver-layout-correctness.md F1, F2. Detent's adoption waits on layout parity.

## May land early

None.
