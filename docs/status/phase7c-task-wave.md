> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-11-03) the ledger for the task wave's first half: Xapiand on task C3. Reactor R and the http H1 move go to the next agent (Handoff, below).

# phase7c-task-wave

- Design: task `docs/design/cancellation.md` at task:6558c2e (D5-D8, D11), task `docs/API.md`; reactor `docs/status/task-sessions.md` at reactor:2fd30c8 (the consumer break list); http `docs/status/task-h1.md` at http:2bede1b (H1's break list); strata `docs/status/road-to-release.md` (Xapiand's owed items); the lead's notes `http-wt/task-handlers/docs/reviews/task-handlers-xapiand.md`, `task-wt/cancellation/docs/reviews/cancellation-xapiand.md`.
- Branch / worktree: `phase7c/task-wave` at `~/code/Kronuz/Xapiand-wt/task-wave`, from local master Xapiand:ebfb8fdfe. Local only: never pushed.
- Pins: task:6558c2e (C3). reactor and http stay at reactor:ade618d and http:679c0f8: http main before H1 does not build against reactor R (`http_asio.h:1028` `reactor::TimedSession`), so both move together in the next half.
- State: the C3 half is built and proved; handed off. Nothing running.

## Done criteria (the lead's owed items; tick only with evidence)

- [ ] 1 `AsioHost` wraps `reactor(i).scheduler()`: next half (needs reactor R, so http H1)
- [x] 2 `Host` forwards `roots`, `runs_here`, `post_call` (C9); `Run` forwards them too. Mutants M6, M7 die
- [x] 3 Four-phase teardown on C3: chores cancelled and drained (`IndexChores::cancel_all`), every HTTP host closed, then their roots destroyed, before the indexes close; the control host last; hosts destroyed before the io_contexts. Mutant M4 dies. Through `HttpAsioService::stop/close/destroy_roots`: next half
- [x] 4 control_plane's published wakes cleared by a scope guard (`Unpublish`); `housekeeping->stop()` stays first in `join()`, before any `destroy_roots`. Mutants M1, M2 die
- [x] 5 `BlockingScheduler::run` (Host::run) owns no roots, so its spawn always runs (`test_a_blocking_scheduler_owns_no_roots`); `Tasks` and the registry's detached tasks are counted by frame, so a task destroyed unrun on a closed host is counted out (`test_a_task_spawned_on_a_closed_host_is_counted_out`). Mutant M3 dies
- [ ] 6 `take_turn`'s deadline linked to the request's token: next half (F2)
- [x] 7 Shutdown test under ASan with `live_frames()`: `test_teardown_destroys_requests_suspended_on_a_turn_a_chore_and_a_body_read` (a turn, a running chore, a body read, an event while holding a turn): frames back at baseline, both turns free after. Again through `HttpAsioService`'s phases: next half
- [ ] 8 `Run::await` deleted: next half (H1 removes its caller)
- [ ] 9 reactor Session migration: next half
- [x] Floor for this half: separate ASan+UBSan and TSan builds on Linux, macOS ASan; graceful_shutdown_check, docs E2E, Lua flood, stall_load dry run; async_check 0 owed (Evidence)
- [x] g++-13 ctest: the hang stays at C3 (Evidence)

## Decisions as built

- `src/engine/host.h`: `Host::post_call` pure virtual, `runs_here` (false) and `roots` (null) virtual, so `scheduler_ref::of(Host&)` finds all three and every host forwards. `BlockingScheduler` queues `post_call` functions with its resumes (`Ready`) and owns no roots.
- `src/engine/host_asio.h`: `AsioHost` forwards the three and exposes `close()`, `destroy_roots()`, `closed()`; `Run` forwards the three, so a request's bridged task is a root of its loop.
- `src/control_plane.h`: `Periodic` and `Debounce` clear their published wake in `Unpublish`; `Tasks::counted` takes `Ended` as a coroutine parameter, so the count follows the frame; `waiting()` on both, for the tests. `src/database/pool.cc`: `counted` takes `Uncount` the same way.
- `src/database/pool.cc`: every live `IndexChores` on one list; `cancel_all(limit)` drops each group's queued chores (`ChoreAwaiter::cancel`: the awaiter resumes with `task::cancelled` through its scheduler), then polls until no chore runs.
- `src/manager.cc` `join()`: after Stop 1, `cancel_all`, every HTTP host `close()`, then every `destroy_roots()`; at the end the control loop stops and its host closes and destroys its roots; `http_hosts` cleared before `http_service`.
- CMake `TASK_COUNT_FRAMES` (OFF) defines task's frame counting for every target. `docs/mechanisms/control-plane.md` describes the phases. `harness/async_allowlist.tsv` classifies the new sites.

## Evidence

Builds: macOS Debug ASan+UBSan (Homebrew clang 22, `TASK_COUNT_FRAMES=ON`); Linux lease (`aws-linux.sh`, Ubuntu 24.04): g++-13.3 Release, g++-13.3 Debug ASan+UBSan (`TASK_COUNT_FRAMES=ON`), clang 18 RelWithDebInfo TSan, full ctest each at Xapiand:58a734d0b, the two changed tests again at Xapiand:0be2c2597.

| Run | Result |
|---|---|
| macOS ASan at Xapiand:7b4317502: `test_control_plane`, `test_reactor_requests`, `test_blocking_guard`, `test_engine_host` | 127, 347, 14, 99 checks pass, no reports |
| Linux g++-13 ASan, both changed tests at Xapiand:0be2c2597 | 347 and 127 pass, no reports |
| Linux ctest g++-13 Release | 32/35: `test_pool` (released views: hung) and `test_blocking_guard` (300 s timeout) are the known gcc-13 hang; `test_node` failed under `-j4` (port) and passes alone |
| Linux ctest g++-13 Debug ASan | 32/35: the same two hangs; `test_reactor_requests` failed the frame check at 58a734d0b (fixed in Xapiand:8bdb3badb) and LSan's 96-byte leak (F1) |
| Linux ctest clang 18 TSan | 34/35: `test_pool` reports a race in `test_a_failed_commit_reaches_only_the_joined_waiters` (`test_pool.cc:181` reads a shared exception while another thread's `__cxa_end_catch` frees it; libc++abi is uninstrumented). Not attributed: the check is that test at master under clang 18 TSan |
| graceful_shutdown_check.sh, macOS ASan server | four scenarios pass |
| docs E2E, `npx newman@6.2.3`, macOS ASan server | 644 of 644 assertions; 0 sanitizer reports in the node log |
| `lua_bounds.py`, Linux g++-13 Release | rc 0, every case 400 in under 2 s. On the macOS ASan Debug server the third case passed its 30 s bound (the first two took 7.7 s against 0.35 s in Release: ASan slowdown) |
| `stall_load.py --dry-run`, macOS ASan server | completes; 0 errors, 0 503s (latencies are a Debug ASan build's, not a measurement) |
| `async_check --strict --owed --extern strata=build-asan/_deps/strata-src` | 0 owed, 0 unclassified, rc 0 |

Mutants (macOS ASan, `.scratch/mutants.sh`, each restored after):

| Mutant | Result |
|---|---|
| M1 Periodic without `Unpublish` | `waiting()` stays true; `stop()` then sets the dead event: ASan use-after-poison |
| M2 Debounce without `Unpublish` | the same in `test_teardown_unpublishes_the_autocommit_wakes` |
| M3 `Ended` never uncounts | `running()` never reaches 0 (four tests) |
| M4 `cancel_all` returns at once | the running chore outlives its frame: `chores.idle()` fails and the test aborts |
| M6 AsioHost `roots()` null | the periodic task is not destroyed: both checks fail |
| M7 Run `roots()` null | the request frames are not destroyed: ASan stack-use-after-scope |
| M5 `Run::await` destroying the handler with a destroyed task | survived on clang 22 and g++-13 (both destroy the task's chain first), so the deferral was dropped (Xapiand:7b4317502) |

The g++-13 hang at task C3: `test_pool` (`released views: hung`) and `test_blocking_guard` (timeout) hang the same in g++-13 Release and Debug ASan and pass under clang 18 TSan. C3 does not make it go away.

## Findings for the next half

- F1 The debug `Run::Resume` wrapper leaks at teardown (LSan, 96 bytes, `test_reactor_requests`, g++-13 Debug ASan): a chore completing after its loop stopped posts a wrapper the loop never runs (task D8.6 names it). Owning the wrappers in `Run` made it worse: the docs E2E under ASan showed `~Run` (`SearchService::handle` ending) destroying a queued wrapper that the loop then ran, so a request's `Run` can end while a resume posted through it is still queued on a live loop. Reverted (Xapiand:80e158db2). Deleting `Run::await` and the wrapper removes both; the debug per-request counters need another home.
- F2 Item 6 needs the request's token at `take_turn` (`pool.cc:349`): its callers are `Shard` methods with no token (`shard.cc:637`, `:683`, `:1509-1658`); the token lives in `Request::cancel` (`search_views.h:195`) and reaches the engine only inside `task::budget` (`handler.h:89`). With H1, `Request::cancel_token()` is the source.

## Handoff: what the next agent does

1. Pins, one commit each, dependency first: reactor:2fd30c8, then http:2bede1b (both pin task:6558c2e). Xapiand includes reactor with `include_directories("${reactor_SOURCE_DIR}")`; reactor's header now includes `task.hh` and `task_asio.hh`, and reactor links `task::asio` while Xapiand sets `TASK_ASIO OFF`: settle that.
2. H1 sites (http `docs/status/task-h1.md`, "Consumer break list"): `src/server/search_service.h:54` and `search_views.cc` `SearchService::handle` return `task::task<void>`; the `hreq.on_cancel(... http::CancelReason)` site (`search_views.cc:3085`) becomes a `task::cancel_callback` on `hreq.cancel_token()`, requesting `request.cancel` and failing the body stream; `manager.cc` `tls_connection_factory_timed` becomes `tls_connection_factory` (same arguments plus the failure hook); `read_chunk`'s `BodyStream::async_read` (`search_views.cc:2950`) per H1; `Request::scheduler()` and `task::deadline` where the handler needs its loop.
3. Delete `Run::await` and `Run` (item 8): the handler is the connection's child task. F1's debug counters move.
4. `AsioHost` wraps `http_service->reactor(i).scheduler()` (`manager.cc` make_servers) and the control reactor's `scheduler()` (`manager.cc` `Reactors`), with no scheduler of its own (item 1). `join()` then runs `http_service->stop()`, `IndexChores::cancel_all`, `http_service->close()`, `http_service->destroy_roots()`, and the control reactor's phases; drop `AsioHost::close/destroy_roots` if unused. `~Reactor` stops its io_context and asserts no claimed loop.
5. Item 9: the tests that build `reactor::Reactor` (`test_blocking_guard.cc:79,179`, `test_control_plane.cc:90`, `test_reactor_requests.cc:90`) and their Loop fixtures build `AsioHost` from the reactor's scheduler.
6. Item 6 (F2).
7. Item 7 through `HttpAsioService`'s phases, then the floor: Linux ASan/TSan, macOS ASan, graceful_shutdown_check, docs E2E, stall_load reduced, Lua flood (Release: ASan Debug passes its 30 s bound), async_check, g++-13 and clang.

## Blocked on

- Nothing for this half.

## Log

- 2026-11-02: worktree from Xapiand:ebfb8fdfe. Xapiand:6a15db3db task pin at C3; Xapiand:a7bae3742 Host and Run forwarding; Xapiand:286c97539 the teardown phases; Xapiand:58a734d0b tests and ledger.
- 2026-11-03: Xapiand:8bdb3badb frame check after the unlock; Xapiand:0be2c2597 wrapper ownership and the control-plane note; Xapiand:6d85bcbab async_check rows; Xapiand:80e158db2 wrapper ownership reverted (F1); Xapiand:7b4317502 Run::await's deferral dropped (M5). The lead moved items 1, 6, 8, 9 and the HTTP phases to the next half once H1 landed (http:2bede1b). Lease runs in `~/taskwave` on `strata-ci-linux`; local build dir `build-asan`.
