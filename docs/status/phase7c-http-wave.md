> Status: CURRENT (2026-11-03) the ledger for the task wave's second half: Xapiand on reactor R and http H1.

# phase7c-http-wave

- Design: http `docs/design/task-handlers.md` at http:2bede1b ("What Xapiand changes"), http `docs/status/task-h1.md` (H1's consumer break list), reactor `docs/status/task-sessions.md` at reactor:2fd30c8 (R's break list), task `docs/design/cancellation.md` at task:6558c2e (D7, D8), the lead's notes `http-wt/task-handlers/docs/reviews/task-handlers-xapiand.md`; the first half's ledger `Xapiand-wt/task-wave/docs/status/phase7c-task-wave.md` (Handoff, F1, F2).
- Branch / worktree: `phase7c/http-wave` at `~/code/Kronuz/Xapiand-wt/http-wave`, from local master Xapiand:b6b8f7534. Local only: never pushed.
- Pins: reactor:2fd30c8 (step R), http:2bede1b (step H1), http-log:765672a (branch `phase/task-h1` in `~/code/Kronuz/http-log-wt/task-h1`, not pushed: see Blocked on). task stays at task:6558c2e (C3).
- State: building.

## Done criteria (the lead's owed items; tick only with evidence)

- [x] 1 `AsioHost` wraps `reactor(i).scheduler()` and the control reactor's `scheduler()`, with no scheduler of its own. Mutant H1own dies (the admission test reads the reactor's queue; the service test compares the host's scheduler with `Request::scheduler()`)
- [x] 6 `take_turn`'s deadline linked to the request's token (F2), through `Shard::cancel`: `test_a_cancelled_request_leaves_the_turn_queue` (direct and through a Shard), and the service test. Mutants T6link and T6shard die
- [x] 8 `Run` and `Run::await` deleted with F1's `Run::Resume` wrapper; the counters live in `engine::Counted` (Debug). Mutants Ccount, Cback, Cinline die; Linux g++-13 ASan with LSan: 0 reports (F1 was a 96-byte leak there)
- [x] 9 reactor Session migration: Xapiand builds no session of its own; the tests build `AsioHost` from the reactor's scheduler; `~Reactor` stops its loop (the `Loop` fixtures rely on it). The flat 1 ms accept retry is inside reactor: nothing in Xapiand
- [x] H1: `SearchService::handle` returns `task::task<void>`, views awaited (through `engine::counted`), `Request::cancel_token()` the only cancellation (`Request::fail_body`; mutant FailBody dies), `tls_connection_factory`, `join()` through `HttpAsioService::stop/close/destroy_roots`
- [x] 7 again through `HttpAsioService`'s phases: `test_service_phases_end_or_destroy_requests_waiting_for_a_turn`, `live_frames()` back to baseline
- [ ] Floor: Linux TSan and ASan+UBSan, macOS ASan; graceful_shutdown_check, docs E2E (`npx newman@6.2.3`), stall_load reduced, lua_bounds, live_frames; `async_check --strict --owed` 0 owed; g++-13 and clang builds (g++-13 excludes `test_pool` and `test_blocking_guard`: the known task hang, fixed elsewhere)

## Decisions as built

- CMake: Asio is fetched before task, and `TASK_ASIO` is on, so task's `task::asio` (which reactor links) reuses `ASIO_INCLUDE_DIR`; `XAPIAND_OBJ` links `task::asio`.
- `src/engine/host_asio.h`: `AsioHost(task::asio_scheduler&)` forwards to the reactor's scheduler; its timers arm on the scheduler's executor. `Run` is gone. `engine::counted(host, work)` returns `work` in Release; a Debug build runs it on a `Counted` scheduler (forwards `post_call`, `runs_here`, `roots`; queues its resumes and runs them from one pump coroutine it owns, posted to the loop's normal or demoted queue), then moves back to the loop before it returns. Nothing is allocated per resume, so a resume the closed loop drops leaks nothing (F1).
- `SearchService::handle` returns `task::task<void>`; it takes `hreq.cancel_token()` for the budget, and a `task::cancel_callback` (`Request::fail_body`) fails a streamed body when it is requested. `Request::cancel` is gone. The views are awaited through `engine::counted`.
- Item 6: `Shard::cancel` (from `lock_shard`, from `DatabaseHandler::turn_token()`, the budget's token) reaches `ShardEndpoint::take_turn` and `fresh`; the turn's deadline is linked to it, and a requested token throws `task::cancelled` instead of `busy`. Writes keep their token-less budget (D3), so an admitted write's turn wait is unchanged; a read's refresh turn and a RESTORE's writes end with the request.
- `manager.cc` `join()`: Stop 1 is `HttpAsioService::stop()`, then `IndexChores::cancel_all`, `close()`, `destroy_roots()`; the control loop's phases run on `reactor.scheduler()`.

## Evidence

Builds: macOS Debug ASan+UBSan (Homebrew clang 22, `TASK_COUNT_FRAMES=ON`, `.scratch/build-asan.sh`); Linux in strata's local VM (`tools/vm/linux.sh`, Ubuntu, 8 vCPUs; the AWS lease was unreachable once the AWS session expired): g++-13 Debug ASan+UBSan (`TASK_COUNT_FRAMES=ON`), clang 18 RelWithDebInfo TSan, g++-13 Release (`.scratch/vm.sh`), at Xapiand:44a5ff985.

| Run | Result |
|---|---|
| macOS ASan: `test_reactor_requests`, `test_control_plane`, `test_blocking_guard`, `test_engine_host` | 363, 127, 14, 99 checks pass, no reports |
| graceful_shutdown_check.sh, macOS ASan server | every scenario passes |
| docs E2E, `npx newman@6.2.3`, macOS ASan server | 644 of 644 assertions, 374 requests; 0 sanitizer reports |
| `stall_load.py --accounts-docs 50000 --duration 10 --no-assert`, macOS ASan server | completes; unloaded and loaded 0 errors, 0 503s (Debug ASan latencies, not a measurement) |
| `lua_bounds.py`, macOS ASan Debug server | first two cases 400 in 10.5 s and 9.7 s, the third past its 30 s bound (the first half saw 7.7 s and a pass here; Release is the run that counts, below) |
| `async_check --strict --owed --extern strata=.scratch/deps/strata-src` (strata:3f564dc) | 0 owed, 0 unclassified, rc 0 |
| Linux g++-13 ASan ctest, `-E "test_pool|test_blocking_guard"` (the known g++-13 task hang, excluded) | 32/33; 0 ASan, UBSan or LSan reports. `test_control_plane` failed its lag bound (`test_control_plane.cc:282`, p99 under 16 ms) once; alone it failed 1 of 3 runs. Not attributed: the check is master's binary in the same VM |

Mutants (macOS ASan, `.scratch/mutants.py`, `.scratch/mutants.sh`, each restored after):

| Mutant | Result |
|---|---|
| H1own `AsioHost` owns a scheduler over the reactor's executor | the reactor's demoted count stays below 256; the service test's scheduler check fails |
| T6link the turn's deadline not linked to the token | the turn test waits past 5 s twice; the service test's read is not cancelled |
| T6shard `Shard::add_document` drops its token | the turn test's Shard case fails |
| FailBody the callback does not fail the body | the body read is not failed (the watchdog ends it) |
| Ccount `Counted` does not set the counters | three rule tests fail, then an abort |
| Cback `counted()` does not move back to the loop | ASan heap-use-after-free in `running_on::~running_on` (the destroyed Counted) |
| Cinline `Counted::runs_here()` forwards the loop's | survived; the new rule case after an Asio completion kills it (Xapiand:44a5ff985) |
| D9scope `emit_via_writer` writes from Counted | the ASan server aborts on http's affinity assertion at the first PUT; restored: PUT 204, GET 200 (`.scratch/d9.sh`) |

## Findings

- W1 A task holds no Asio work while it waits for a chore, so an `io_context` without a work guard returns from `run()` while a request waits for a pool thread. `asio::co_spawn` and the old `Run::await` held work for the awaited operation; tasks do not. The reactors' threads and the control loop hold a guard; the test fixture `Loop::run` now does too (it lost the bulk test's last chunks and destroyed an `IndexChores` with a chore running, at 94,291 of 100,000 lines).
- W2 http's D9 affinity rule (`AsioResponseWriter::affine()`, http `http_asio.h:280`) asserts `task::current()` is the reactor's scheduler. A Debug build that runs a request on a per-request scheduler forwarding to the loop's (the design's own "switches with `task::schedule_on`", and `engine::Counted` here) fails it at the first write. Xapiand writes at one place (`emit_via_writer`), which names the loop's scheduler (`task::running_on`) around the writes in Debug builds, on the loop's thread. The alternative is in http: `affine()` accepting a current scheduler whose `runs_here()` is true. For the lead.
- W3 R's `stop()` is bounded by twice the grace (10 s for Xapiand), and an admitted write that waits for its turn longer than that is left suspended and destroyed by `destroy_roots()`: its client gets no answer, and the write does not apply. Before R, `stop()` waited for every session, and the shutdown's bound was `default_shutdown_write_bound()` (DB_TIMEOUT + 5 s = 35 s, C14). Xapiand's grace stays 5 s; raising it to cover DB_TIMEOUT would keep stalled peers that long too. For the lead.

## Next

## Blocked on

- Pushing http-log's `phase/task-h1` (http-log:765672a) was refused by the permission classifier. Xapiand pins it; the build reads it locally (`FETCHCONTENT_SOURCE_DIR_HTTP_LOG`). The lead pushes it and lands it on http-log main.

## Log

- 2026-11-03: worktree from Xapiand:b6b8f7534. Xapiand:f29690a3a reactor pin; Xapiand:bb6fa6e34 http and http-log pins. http-log:765672a ports `AccessLog::handle` to task (3/3 ctest, macOS Debug).
