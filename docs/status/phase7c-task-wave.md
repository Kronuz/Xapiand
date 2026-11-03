> Status: CURRENT (2026-11-02) the ledger for the task wave's first half: Xapiand on task C3 and reactor step R.

# phase7c-task-wave

- Design: task `docs/design/cancellation.md` at task:6558c2e (D5-D8, D11), task `docs/API.md`; reactor `docs/status/task-sessions.md` at reactor:2fd30c8 (the consumer break list); strata `docs/status/road-to-release.md` (Xapiand's owed items); the lead's notes `http-wt/task-handlers/docs/reviews/task-handlers-xapiand.md`, `task-wt/cancellation/docs/reviews/cancellation-xapiand.md`.
- Branch / worktree: `phase7c/task-wave` at `~/code/Kronuz/Xapiand-wt/task-wave`, from local master Xapiand:ebfb8fdfe. Local only: never pushed.
- Pins: task:6558c2e (C3). reactor:2fd30c8 (R) is blocked (below).
- State: C3 half built; tests written; sanitizer builds running.

## Done criteria (the lead's owed items; tick only with evidence)

- [ ] 1 `AsioHost` wraps `reactor(i).scheduler()` (needs reactor R)
- [ ] 2 `Host` forwards `roots`, `runs_here`, `post_call` (C9); `Run` forwards them too
- [ ] 3 Four-phase shutdown; index chores cancelled and drained before any scheduler is destroyed
- [ ] 4 control_plane's published wakes cleared by a scope guard; housekeeping->stop() before destroy_roots
- [ ] 5 `Host::run` never blocks on a spawn after its scheduler closes
- [ ] 6 `take_turn`'s deadline linked to the request's token
- [ ] 7 `live_frames()` shutdown test under ASan, requests suspended on a turn, a chore, a body read
- [ ] 8 `Run::await` deleted if nothing needs it after R (else H1's half)
- [ ] 9 reactor Session migration (needs reactor R)
- [ ] Each fix: a test that fails with it reverted; a mutant that dies; ASan+UBSan and TSan separate builds (Linux via aws-linux.sh, macOS ASan locally); graceful_shutdown_check.sh, docs E2E (`npx newman@6.2.3`), stall_load.py reduced, Lua flood; `async_check --strict --owed --extern strata=<pin>` at 0 owed; g++-13 and clang builds
- [ ] g++-13 ctest on this branch: whether test_pool (released_views) and test_blocking_guard still hang at C3

## Decisions as built

- Host (`src/engine/host.h`): `post_call` pure virtual, `runs_here` (default false) and `roots` (default null) virtual, so `scheduler_ref::of(Host&)` detects all three and every host forwards. `BlockingScheduler` queues `post_call` functions with its resumes and owns no roots, so `run()`'s spawn always runs its task.
- `AsioHost` forwards the three to its scheduler and exposes `close()`, `destroy_roots()`, `closed()`. `Run` forwards the three (its spawned task is a root of the loop's scheduler); its debug `post` skips a closed scheduler. `Run::await`'s completion (`Complete`) does not destroy the awaiting coroutine's handler when the task is destroyed unrun: it queues it on the stopped loop, so the awaiting frame (which holds the `Run`) outlives the task's chain.
- control_plane.h: `Periodic` and `Debounce` clear their published wake in a scope guard (`Unpublish`); `Tasks` counts a task by its frame (`Ended`, a coroutine parameter), so a task destroyed suspended or unrun is counted out. pool.cc's `counted` the same (`Uncount`).
- `IndexChores::cancel_all(limit)` (pool.cc): every live `IndexChores` is on a list; it drops each group's queued chores (their awaiters resume with `task::cancelled` through their scheduler) and waits until none runs.
- `XapiandManager::join`: after the HTTP stop, `cancel_all`, then every HTTP host `close()`, then every `destroy_roots()`, before the indexes close; at the end the control loop stops and its host closes and destroys its roots; hosts are destroyed before `http_service` (their io_contexts).
- CMake option `TASK_COUNT_FRAMES` (OFF) defines it for every target.

## Blocked on

- Reactor R (items 1, 9, and 3's HTTP phases through `HttpAsioService`): http main http:679c0f8 does not compile against reactor:2fd30c8 (`http_asio.h:1028-1029` `reactor::TimedSession`, `:1010-1013` `asio::awaitable` factories; reactor's break list). Any http that builds against R is H1 (`origin/phase/task-h1`, http:5a2dd3a, not on main). So R's pin moves with H1's, in the second half, unless the lead rules an interim pin at the H1 branch.

## Log

- 2026-11-02: worktree from Xapiand:ebfb8fdfe. Xapiand:6a15db3db task pin at C3.
