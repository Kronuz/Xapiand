> Status: CURRENT (2026-11-03) the ledger for the task wave's second half: Xapiand on reactor R and http H1.

# phase7c-http-wave

- Design: http `docs/design/task-handlers.md` at http:2bede1b ("What Xapiand changes"), http `docs/status/task-h1.md` (H1's consumer break list), reactor `docs/status/task-sessions.md` at reactor:2fd30c8 (R's break list), task `docs/design/cancellation.md` at task:6558c2e (D7, D8), the lead's notes `http-wt/task-handlers/docs/reviews/task-handlers-xapiand.md`; the first half's ledger `Xapiand-wt/task-wave/docs/status/phase7c-task-wave.md` (Handoff, F1, F2).
- Branch / worktree: `phase7c/http-wave` at `~/code/Kronuz/Xapiand-wt/http-wave`, from local master Xapiand:b6b8f7534. Local only: never pushed.
- Pins: reactor:2fd30c8 (step R), http:2bede1b (step H1), http-log:765672a (branch `phase/task-h1` in `~/code/Kronuz/http-log-wt/task-h1`, not pushed: see Blocked on). task stays at task:6558c2e (C3).
- State: building.

## Done criteria (the lead's owed items; tick only with evidence)

- [ ] 1 `AsioHost` wraps `reactor(i).scheduler()` and the control reactor's `scheduler()`, with no scheduler of its own
- [ ] 6 `take_turn`'s deadline linked to the request's token (F2)
- [ ] 8 `Run::await` deleted; F1's `Run::Resume` wrapper gone; the debug counters have a new home
- [ ] 9 reactor Session migration: the tests build `AsioHost` from the reactor's scheduler; `~Reactor` stops its loop
- [ ] H1: `SearchService::handle` returns `task::task<void>`, views awaited directly, `Request::cancel_token()` the only cancellation, `tls_connection_factory`, `join()` through `HttpAsioService::stop/close/destroy_roots`
- [ ] 7 again through `HttpAsioService`'s phases: `live_frames()` back to baseline
- [ ] Floor: Linux TSan and ASan+UBSan, macOS ASan; graceful_shutdown_check, docs E2E (`npx newman@6.2.3`), stall_load reduced, lua_bounds, live_frames; `async_check --strict --owed` 0 owed; g++-13 and clang builds (g++-13 excludes `test_pool` and `test_blocking_guard`: the known task hang, fixed elsewhere)

## Decisions as built

- CMake: Asio is fetched before task, and `TASK_ASIO` is on, so task's `task::asio` (which reactor links) reuses `ASIO_INCLUDE_DIR`; `XAPIAND_OBJ` links `task::asio`.
- `src/engine/host_asio.h`: `AsioHost(task::asio_scheduler&)` forwards to the reactor's scheduler; its timers arm on the scheduler's executor. `Run` is gone. `engine::counted(host, work)` returns `work` in Release; a Debug build runs it on a `Counted` scheduler (forwards `post_call`, `runs_here`, `roots`; queues its resumes and runs them from one pump coroutine it owns, posted to the loop's normal or demoted queue), then moves back to the loop before it returns. Nothing is allocated per resume, so a resume the closed loop drops leaks nothing (F1).
- `SearchService::handle` returns `task::task<void>`; it takes `hreq.cancel_token()` for the budget, and a `task::cancel_callback` (`Request::fail_body`) fails a streamed body when it is requested. `Request::cancel` is gone. The views are awaited through `engine::counted`.
- Item 6: `Shard::cancel` (from `lock_shard`, from `DatabaseHandler::turn_token()`, the budget's token) reaches `ShardEndpoint::take_turn` and `fresh`; the turn's deadline is linked to it, and a requested token throws `task::cancelled` instead of `busy`. Writes keep their token-less budget (D3), so an admitted write's turn wait is unchanged; a read's refresh turn and a RESTORE's writes end with the request.
- `manager.cc` `join()`: Stop 1 is `HttpAsioService::stop()`, then `IndexChores::cancel_all`, `close()`, `destroy_roots()`; the control loop's phases run on `reactor.scheduler()`.

## Evidence

(pending)

## Findings

- W1 A task holds no Asio work while it waits for a chore, so an `io_context` without a work guard returns from `run()` while a request waits for a pool thread. `asio::co_spawn` and the old `Run::await` held work for the awaited operation; tasks do not. The reactors' threads and the control loop hold a guard; the test fixture `Loop::run` now does too (it lost the bulk test's last chunks and destroyed an `IndexChores` with a chore running, at 94,291 of 100,000 lines).
- W2 http's D9 affinity rule (`AsioResponseWriter::affine()`, http `http_asio.h:280`) asserts `task::current()` is the reactor's scheduler. A Debug build that runs a request on a per-request scheduler forwarding to the loop's (the design's own "switches with `task::schedule_on`", and `engine::Counted` here) fails it at the first write. Xapiand writes at one place (`emit_via_writer`), which names the loop's scheduler (`task::running_on`) around the writes in Debug builds, on the loop's thread. The alternative is in http: `affine()` accepting a current scheduler whose `runs_here()` is true. For the lead.
- W3 R's `stop()` is bounded by twice the grace (10 s for Xapiand), and an admitted write that waits for its turn longer than that is left suspended and destroyed by `destroy_roots()`: its client gets no answer, and the write does not apply. Before R, `stop()` waited for every session, and the shutdown's bound was `default_shutdown_write_bound()` (DB_TIMEOUT + 5 s = 35 s, C14). Xapiand's grace stays 5 s; raising it to cover DB_TIMEOUT would keep stalled peers that long too. For the lead.

## Next

## Blocked on

- Pushing http-log's `phase/task-h1` (http-log:765672a) was refused by the permission classifier. Xapiand pins it; the build reads it locally (`FETCHCONTENT_SOURCE_DIR_HTTP_LOG`). The lead pushes it and lands it on http-log main.

## Log

- 2026-11-03: worktree from Xapiand:b6b8f7534. Xapiand:f29690a3a reactor pin; Xapiand:bb6fa6e34 http and http-log pins. http-log:765672a ports `AccessLog::handle` to task (3/3 ctest, macOS Debug).
