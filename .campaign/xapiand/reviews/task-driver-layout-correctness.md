# task-driver-layout review: correctness

Commit: 63b43bd
Verdict: APPROVE
Date: 2026-11-19

APPROVE: lands as is, at this commit's code, with no blocking finding.

## Evidence I relied on

- Job 361954 (mutant at the revert): the layout test fails, release 216 vs debug 224. This shows the two TUs genuinely see different layouts when `driver_` is conditional, so the test is not vacuous.
- Jobs b00b7b (head passes), 48d7b2 (two-drivers abort test passes), 077ad8 (suite passes): A2 and A3's suite half.

## What I checked myself

Read-only inspection of `git diff b7e2449 63b43bd` and the headers at the head; no probe jobs were needed.

1. Two TUs with different NDEBUG: `test/layout_release.cc` defines NDEBUG when not already defined, and `test/layout_debug.cc` does `#undef NDEBUG` before its first include, so it wins over a `-DNDEBUG` on the command line. Each exports non-inline size/align functions, so the linker cannot merge them into one. The mutant result (216 vs 224) confirms the TUs really differ.
2. NDEBUG hits in `task.hh`: line 444 is the `driving` guard and assert inside the body of `run_until` (code only, no member). Lines 1722, 1727 and 1750 are the inline variable `orphan_count`, `orphaned_waiters()` and `event::~event` (code and a namespace-scope variable, no class member). `lifecycle.hh` has none. `inline_scheduler` has no base class; its members are `deque`, `uint64_t`, `unique_ptr<timer_wheel>` (the wheel's layout is behind the pointer), `mutex`, `thread::id`, `atomic<thread::id>`, `vector`, `atomic<bool>`, and `detail::queued_call` is two plain pointers. None is NDEBUG-conditional at the head. `task_asio.hh` has `cancellable_asio_awaiter::on_loop_` conditional (see F1), which is not a member of `inline_scheduler` or anything it embeds.
3. Release-build cost: `driver_` of `inline_scheduler` is touched only inside the `#ifndef NDEBUG` block of `run_until` (task.hh:444-455). In release it is an 8-byte member zero-initialized at construction and never read or written. The other `driver_` (task.hh:567, `blocking_scheduler`) is a separate member that was already unconditional. No hot-path cost; the only price is 8 bytes per scheduler.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | `task_asio.hh` has the same ODR hazard in `cancellable_asio_awaiter`: `bool on_loop_` exists only without NDEBUG (task_asio.hh:504-506, used at 418 and 453). A Debug sanitizer TU linking a Release one sees two layouts of that class. It is outside this quest (inline_scheduler) and Detent's F7 scope, but the same mixed-build adoption hits it. | `grep -n NDEBUG task_asio.hh` | Make `on_loop_` unconditional and guard only the assert/store, in a follow-up quest. |
| F2 | no | nit | `detail::orphaned_waiters()` and `event::~event` are inline with NDEBUG-dependent bodies, and `orphan_count` exists only in Debug: mixed-NDEBUG TUs violate the ODR on function bodies (not layout), and the linker may pick either body. | task.hh:1722-1750 | Document as Debug-only diagnostics, or make the counter unconditional. Not for this quest. |
| F3 | no | nit | The test compares size and alignment only, not member offsets, so a reorder or same-size swap of conditional members would pass. It is enough for the actual defect (a missing member changes size). | test/layout_test.cc | Optionally add `offsetof` of `driver_` via a friend probe; not needed. |

## Not run

No rebuild, no sanitizer link of a Debug TU against a Release library, no benchmark of the 8-byte growth (judged by reading: the member is never accessed in release). CI status (A3's CI half) is not mine to confirm.
