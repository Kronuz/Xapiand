# task-asio-layout: journal

State: waiting on jobs 261119-132355-c6cff3 261119-132356-5d5f37 261119-132357-b81acd
Quest: .campaign/xapiand/quests/task-asio-layout.md (the lead's; never edit it)

## Doing

Work committed in task:adecffe. Mutant branch mutant-asio-layout (task:ced52ad) reverts on_loop_ to Debug-only.

## Next

1. Confirm the three jobs, then set ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | job 261119-132356-5d5f37 passes at task:adecffe; 261119-132355-c6cff3 (--fails-without) fails at mutant task:ced52ad on the awaiter layout assertion |
| A2 | [artifact] task_asio.hh: the only NDEBUG left (line 451) guards an assert, no data member |
| A3 | suite job 261119-132357-b81acd; CI after push (not pushed) |

## Result

on_loop_ is stored in every build; the layout test (test/layout_test.cc, layout_release.cc, layout_debug.cc, CMake defines TASK_LAYOUT_ASIO and links task::asio) compares inline_scheduler and cancellable_asio_awaiter across NDEBUG TUs.

NDEBUG / assert-state sweep of every header (task.hh, task_asio.hh, task_timers.hh, lifecycle.hh):
| Hit | Verdict |
|---|---|
| task.hh:444 `driving` guard in run_until | code only, no member; driver_ already unconditional |
| task.hh:1722 `orphan_count` | namespace-scope inline variable, not class state; no layout effect |
| task.hh:1727 `orphaned_waiters()` | body differs by NDEBUG (ODR on code); left, see F2 ruling |
| task.hh:1750 `event::~event` | same |
| task_asio.hh:451 assert in handler | code only |
| task_asio.hh:502 `on_loop_` | fixed, unconditional |
| lifecycle.hh, task_timers.hh asserts | assert-only, no state; no NDEBUG hits |
| TASK_COUNT_FRAMES, TASK_FRAME_POOL, TASK_TEST_HOOKS, TASK_ASAN, TASK_SYMMETRIC_TRANSFER | macros affect only inline function bodies / namespace-scope variables, none a class member (checked #if list) |
No other conditional member, base or class-scope state exists.

## Rulings I made

- F2 left: `orphaned_waiters()`/`event::~event` live in task.hh, outside this quest's file set. Both are Debug-only diagnostics with no layout effect; making orphan_count unconditional would add a relaxed RMW to event teardown in Release. Lead may widen the set if wanted.
- The mutant is a branch (mutant-asio-layout), not on the quest branch.
