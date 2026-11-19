# task-asio-layout review: correctness

Commit: fef73c3
Verdict: APPROVE
Date: 2026-11-19

(task:fef73c3, the quest branch quest/xapiand/task-asio-layout.) No blocking finding. Two non-blocking
notes: the `#define private public` scope (F1) and a small release-build cost the change adds (F2).

## Evidence I relied on

- 261119-132648-c40f2a: the layout test passes at the head (A1). 261119-132647-2e1fa2: the mutant with
  `on_loop_` Debug-only fails it (fails-without). Both given as proven by the lead; not rerun.
- 261119-132649-28a38d: the suite (A3). CI at fef73c3 was still queued/running at review time (the
  Linux libstdc++ build is the one that exercises F1).

## What I checked myself

Scripts in `.scratch/xapiand/review-task-asio-layout-correctness/`.

1. `#define private public` (task:test/layout_debug.cc:6, layout_release.cc:8). Only those two TUs
   define it (`git grep 'define private'`), and they link only into `task_layout_test`
   (CMakeLists.txt:52), so it is confined to the test. It is applied after `task.hh` (all its std
   headers) but not after all standard and asio headers: `task_asio.hh` opens its asio headers under
   the define (job 261119-133017-d29a7e): 173 asio headers, libc++ `<any>` and `<csignal>`, and the
   POSIX socket headers. Undefined by the letter ([macro.names]); benign here (access does not move
   layout, and the TUs are test-only), but the stated invariant does not hold. See F1.
2. Sweep. `git grep -n NDEBUG` at fef73c3 gives, outside docs and tests, exactly the journal's rows:
   task.hh:444 (a local struct in `run_until`), 1722 (namespace-scope inline `orphan_count`), 1727
   (`orphaned_waiters()` body), 1750 (`event::~event` body), task_asio.hh:451 (an assert in the
   handler) and 502 (the comment on the now-unconditional `on_loop_`). `lifecycle.hh`,
   `task_timers.hh` and `bench/` have no hit. Test-file hits (`asio_test.cc`, `lanes_test.cc`,
   `sync_test.cc`, `deadline_test.cc`) only gate fork tests or `#undef` in their own program. I also
   listed every other `#if` in the headers: `TASK_COUNT_FRAMES` now defaults to 0 (task.hh:697), not
   to `!NDEBUG`; `TASK_FRAME_POOL`, `TASK_TEST_HOOKS`, `TASK_ASAN` and `TASK_SYMMETRIC_TRANSFER` select
   function bodies, macros or namespace-scope variables, except `TASK_SYMMETRIC_TRANSFER`, which
   selects the members of `trampoline` and `fresh_trampoline` by compiler (clang vs other), not by
   NDEBUG. No data member, base or class-scope state exists only in Debug or only for asserts. The
   journal's table is complete. A2 holds.
3. Release hot path, job 261119-132932-c30297: `-O2 -DNDEBUG` assembly of a token-bound `await_asio`
   TU, base 63b43bd vs head. 4061 vs 4064 instructions, one extra `callq *%rax`: the `on_loop_ =
   sched_.runs_here()` call at task_asio.hh:418, now compiled in release. It goes through the
   scheduler_ref vtable to `asio_scheduler::runs_here()`, which asks `io_->running_in_this_thread()`
   (Asio's per-thread call stack). The only other change is a byte store, at an offset that stays in
   padding, so size is unchanged. The cost is on the token-bound path only (the empty-token path
   returns before it). I did not time it. See F2.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | `#define private public` is applied before `task_asio.hh`, so asio, libc++ `<any>`/`<csignal>` and the socket headers are first opened with `private` redefined. The "after all standard and asio headers" condition is not met. It is test-only and benign today; CI's Linux libstdc++ build is the real check. | 261119-133017-d29a7e; layout_debug.cc:6, layout_release.cc:8 | Include the eight asio headers `task_asio.hh` lists (task_asio.hh:30-37) before the define; then only `cassert` (meant to be re-read) and `task_asio.hh` itself are opened under it (job 261119-133106-572130 compiles that way). Or reach `on_loop_` by the explicit-instantiation member-pointer idiom, with no macro. |
| F2 | no | nit | Release now pays one indirect call plus Asio's call-stack lookup per token-bound `await_suspend`, to fill a field only the Debug assert reads. The layout goal does not need the store. | 261119-132932-c30297; task_asio.hh:418, 452 | Options: (a) keep as is (simplest, one shape, cost is small next to the cancel-state setup and the Asio operation); (b) guard only the assignment, `#ifndef NDEBUG on_loop_ = sched_.runs_here(); #endif`, keeping the member unconditional: same layout, zero release cost. The layout test is unchanged by (b). |
| F3 | no | nit | The quest asks the `orphaned_waiters()` / `event::~event` difference be kept NDEBUG-independent or documented as harmless; the reasoning is only in the journal (F2 ruling). task.hh:1725 says "Debug builds; 0 otherwise", which is the whole in-code note. Mixed-NDEBUG programs can see a 0 count from a release body. Diagnostics only, no layout effect. | task.hh:1722-1755 | Optionally add "ODR-different by NDEBUG; diagnostic only" to that comment. |
| F4 | no | nit | `layout_test.cc` line 1 is a parenthetical placed above the file's header comment; reads as an orphan. | test/layout_test.cc:1 | Move it into the header comment. |

## Not run

- Timing of F2 (no benchmark; the claim is the assembly diff only).
- The Linux/libstdc++ build and CI (running at review time).
- The suite and layout test (taken from the lead's jobs).
