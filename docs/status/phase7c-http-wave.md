> Status: CURRENT (2026-11-03) the ledger for the task wave's second half: Xapiand on reactor R and http H1.

# phase7c-http-wave

- Design: http `docs/design/task-handlers.md` at http:2bede1b ("What Xapiand changes"), http `docs/status/task-h1.md` (H1's consumer break list), reactor `docs/status/task-sessions.md` at reactor:2fd30c8 (R's break list), task `docs/design/cancellation.md` at task:6558c2e (D7, D8), the lead's notes `http-wt/task-handlers/docs/reviews/task-handlers-xapiand.md`; the first half's ledger `Xapiand-wt/task-wave/docs/status/phase7c-task-wave.md` (Handoff, F1, F2).
- Branch / worktree: `phase7c/http-wave` at `~/code/Kronuz/Xapiand-wt/http-wave`, from local master Xapiand:b6b8f7534. Local only: never pushed.
- Pins: reactor:2fd30c8 (step R), http:73cfa02 (main: H2 `BodyStream::read`, affinity against the loop thread), http-log:616c80a (branch `phase/task-h1`; repin when the Detent lead fast-forwards http-log main), task:d080786 (the nested-driver fix, merged from master).
- State: wave 2 built and proved (H2, W3, W2 closed, the timing checks attributed); ready to merge. Nothing running.

## Done criteria (the lead's owed items; tick only with evidence)

- [x] 1 `AsioHost` wraps `reactor(i).scheduler()` and the control reactor's `scheduler()`, with no scheduler of its own. Mutant H1own dies (the admission test reads the reactor's queue; the service test compares the host's scheduler with `Request::scheduler()`)
- [x] 6 `take_turn`'s deadline linked to the request's token (F2), through `Shard::cancel`: `test_a_cancelled_request_leaves_the_turn_queue` (direct and through a Shard), and the service test. Mutants T6link and T6shard die
- [x] 8 `Run` and `Run::await` deleted with F1's `Run::Resume` wrapper; the counters live in `engine::Counted` (Debug). Mutants Ccount, Cback, Cinline die; Linux g++-13 ASan with LSan: 0 reports (F1 was a 96-byte leak there)
- [x] 9 reactor Session migration: Xapiand builds no session of its own; the tests build `AsioHost` from the reactor's scheduler; `~Reactor` stops its loop (the `Loop` fixtures rely on it). The flat 1 ms accept retry is inside reactor: nothing in Xapiand
- [x] H1: `SearchService::handle` returns `task::task<void>`, views awaited (through `engine::counted`), `Request::cancel_token()` the only cancellation (`Request::fail_body`; mutant FailBody dies), `tls_connection_factory`, `join()` through `HttpAsioService::stop/close/destroy_roots`
- [x] 7 again through `HttpAsioService`'s phases: `test_service_phases_end_or_destroy_requests_waiting_for_a_turn`, `live_frames()` back to baseline
- [x] Floor: Linux TSan and ASan+UBSan, macOS ASan; graceful_shutdown_check, docs E2E (`npx newman@6.2.3`), stall_load reduced, lua_bounds, live_frames; `async_check --strict --owed` 0 owed; g++-13 and clang builds (g++-13 excluded `test_pool` and `test_blocking_guard` then: the task hang fixed at task:d080786). Evidence below; two timing checks failed under `ctest -j4` in the VM and pass alone (not attributed)

## Decisions as built

- CMake: Asio is fetched before task, and `TASK_ASIO` is on, so task's `task::asio` (which reactor links) reuses `ASIO_INCLUDE_DIR`; `XAPIAND_OBJ` links `task::asio`.
- `src/engine/host_asio.h`: `AsioHost(task::asio_scheduler&)` forwards to the reactor's scheduler; its timers arm on the scheduler's executor. `Run` is gone. `engine::counted(host, work)` returns `work` in Release; a Debug build runs it on a `Counted` scheduler (forwards `post_call`, `runs_here`, `roots`; queues its resumes and runs them from one pump coroutine it owns, posted to the loop's normal or demoted queue), then moves back to the loop before it returns. Nothing is allocated per resume, so a resume the closed loop drops leaks nothing (F1).
- `SearchService::handle` returns `task::task<void>`; it takes `hreq.cancel_token()` for the budget, and a `task::cancel_callback` (`Request::fail_body`) fails a streamed body when it is requested. `Request::cancel` is gone. The views are awaited through `engine::counted`.
- Item 6: `Shard::cancel` (from `lock_shard`, from `DatabaseHandler::turn_token()`, the budget's token) reaches `ShardEndpoint::take_turn` and `fresh`; the turn's deadline is linked to it, and a requested token throws `task::cancelled` instead of `busy`. Writes keep their token-less budget (D3), so an admitted write's turn wait is unchanged; a read's refresh turn and a RESTORE's writes end with the request.
- `manager.cc` `join()`: Stop 1 is `HttpAsioService::stop()`, then `IndexChores::cancel_all`, `close()`, `destroy_roots()`; the control loop's phases run on `reactor.scheduler()`.

## Evidence

Wave 2 (Xapiand:376eebf23 for the Linux binaries, the rest of the commits touch docs and tests only): macOS ASan `test_reactor_requests` 363, `test_control_plane` 128, `test_blocking_guard` 14, `test_engine_host` 99, `test_node` 104 checks, no reports; graceful_shutdown_check.sh passes; docs E2E (`npx newman@6.2.3`) 644 of 644 assertions, 374 requests; `stall_load.py --accounts-docs 10000 --duration 10 --no-assert` completes, 0 errors and 0 503s in both phases (10,000 documents, not the first run's 50,000: the Debug ASan RESTORE of 20,000 documents takes more than 300 s on a loaded host, and old and new binaries restore 3,000 documents in 23 s and 19 s); `async_check --strict --owed --extern strata=<strata:3f564dc>` 0 owed, 0 unclassified. Linux (strata's local VM, g++-13 and clang 18; AWS was expired): Release ctest 35 of 35 with `test_pool` and `test_blocking_guard` included (no g++ exclusions), ASan+UBSan (Debug, `TASK_COUNT_FRAMES`) 35 of 35 (with `--timeout 900`: `test_engine_search_strata` needs more than 300 s under `-j4` ASan), TSan (clang 18, RelWithDebInfo) 35 of 35 (the first half's `test_pool` report is gone); `lua_bounds.py` on Release PASS (the flood's light GETs p99 93 ms against its 100 ms limit; 36 ms in the first half, 216 ms while the TSan build ran beside it). macOS `lua_bounds.py` Debug ASan: first two cases 400 in 10.8 s and 10.6 s, the third past its bound (Debug ASan, as before).

First half:

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
| Linux clang 18 TSan ctest, all 35 | 34/35: `test_pool`'s one report is the first half's (`test_pool.cc:181` reads a shared exception while libc++abi's `__cxa_end_catch`, uninstrumented, frees it); `test_blocking_guard`, `test_control_plane`, `test_reactor_requests` pass with none |
| macOS Debug TSan (clang 22), `test_reactor_requests` (RelWithDebInfo compiles `Counted` out, so this is the one TSan run of it) | 363 checks pass, 0 reports |
| Linux g++-13 Release ctest, `-E "test_pool|test_blocking_guard"` | 32/33: `test_node` failed `test_node.cc:691` (a shutdown's stop time within its bound + 800 ms) under `-j4`; alone it passes. The first half saw `test_node` fail under `-j4` the same way |
| `lua_bounds.py`, Linux g++-13 Release | PASS (rc 0); the flood's light GETs p99 36 ms; the graceful stop exits |

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

## Wave 2 (2026-11-03): H2, W2, W3, the timing checks

- [x] H2: `Request::read_chunk` awaits `BodyStream::read(chunk, http_req->cancel_token())`; no `await_asio` is left on the body path. A requested token raises `task::cancelled`; a failed body is a `ClientError`. `Request::FailBody` and `fail_body` are deleted (the token reaches the read itself). Mutants R2token (token not passed: `test_a_cancelled_request_ends_its_body_read` fails) and R2cancel (ClientError instead of cancelled: aborts) die.
- [x] W2 closed: http:73cfa02 checks affinity against the loop thread, so the Debug workarounds in `emit_via_writer` and `read_chunk` are deleted; `test_reactor_requests` (363 checks, body reads and writes under `Counted`) passes on macOS ASan.
- [x] W3: `XapiandManager::http_shutdown_grace(left) = max(250 ms, (left - 5 s) / 2)`, set in `join()` before `stop()` (D16 and `docs/mechanisms/control-plane.md` carry the arithmetic: 35 s budget, 15 s grace, stop at most 30 s, 5 s left for the registry). Tests (`test_node`): `test_the_http_grace_follows_the_time_left_in_the_budget` (arithmetic) and `test_a_write_waiting_for_its_turn_is_let_finish_while_the_budget_has_time` (a 25 s budget, the held turn released 14 s in, past the first grace of 10 s: exit 0, nothing abandoned, the write is there after a restart). Mutants W3grace (a quarter of the budget) dies in the arithmetic test; W3wire (`join()` leaves the grace at 5 s) dies in the late-write test (the write is destroyed at 10 s: restart does not find it) and in the abandon test. `test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound` now expects the registry's abandon only (its 2 s bound gives the shortest grace, so the write is destroyed, not abandoned); the second-signal test still covers an abandoned HTTP stop. Trade: a stalled client holds the stop for the first grace (15 s, where the fixed 5 s cut it); a client whose write finishes after the first grace sees its connection cut while the write applies.
- [x] The timing checks. `test_control_plane` `test_pending_keys_cost_the_loop_nothing` (p99 of the lag probe under 16 ms): attributed to CPU contention, not to the code. The test started and fired 5,000 keys in one burst, so the loop's lag was the host's time to run them; the failure printout (added) showed 5 to 187 wakes, 3 to 8 over 16 ms, max 264 to 720 ms. Idle Linux ASan VM: 6 of 6 pass; six busy loops beside it: 4 of 6 fail; the unchanged test code is master's. The test now paces the keys (50 every 20 ms, all 5,000 pending when the first fires); the same contention: 6 of 6 pass. Mutant CPsleep (the loop sleeps 25 ms at every 50th fire) fails the paced test. `test_node` stop-time check (`stopped_in < bound + 800`): did not reproduce (12 runs, Linux Release, 8 busy loops; 0 failures); the earlier `-j4` failure is not attributed and the failure now prints the stop's log timeline. One new flake seen under contention, 1 of 12: `test_concurrent_first_uses_of_a_schema_load_it_once` (`schemas->loads() - before == 2`); the failure now prints the count; not attributed.

## Findings

- W1 A task holds no Asio work while it waits for a chore, so an `io_context` without a work guard returns from `run()` while a request waits for a pool thread. `asio::co_spawn` and the old `Run::await` held work for the awaited operation; tasks do not. The reactors' threads and the control loop hold a guard; the test fixture `Loop::run` now does too (it lost the bulk test's last chunks and destroyed an `IndexChores` with a chore running, at 94,291 of 100,000 lines).
- W2 (closed in wave 2: http:73cfa02) http's D9 affinity rule (`AsioResponseWriter::affine()`, http `http_asio.h:280`) asserts `task::current()` is the reactor's scheduler. A Debug build that runs a request on a per-request scheduler forwarding to the loop's (the design's own "switches with `task::schedule_on`", and `engine::Counted` here) fails it at the first write. Xapiand writes at one place (`emit_via_writer`), which names the loop's scheduler (`task::running_on`) around the writes in Debug builds, on the loop's thread. The alternative is in http: `affine()` accepting a current scheduler whose `runs_here()` is true. For the lead.
- W3 (closed in wave 2: the grace follows D16's budget) R's `stop()` is bounded by twice the grace (10 s for Xapiand), and an admitted write that waits for its turn longer than that is left suspended and destroyed by `destroy_roots()`: its client gets no answer, and the write does not apply. Before R, `stop()` waited for every session, and the shutdown's bound was `default_shutdown_write_bound()` (DB_TIMEOUT + 5 s = 35 s, C14). Xapiand's grace stays 5 s; raising it to cover DB_TIMEOUT would keep stalled peers that long too. For the lead.

## Next

1. The lead: merge into master; repin http-log to its main commit once the Detent lead fast-forwards it (it is a branch commit now, http-log:616c80a).
2. Open: the schema load-once flake under contention (1 of 12), the `test_node` stop-time check if it recurs (its timeline now prints), `lua_bounds` flood p99 93 ms against 100 on the shared VM.

## Blocked on

- Nothing. (http-log's `phase/task-h1`, http-log:616c80a, is pushed; Xapiand pins it until the Detent lead moves it to http-log main.)

## Log

- 2026-11-03: worktree from Xapiand:b6b8f7534. Xapiand:f29690a3a reactor pin; Xapiand:bb6fa6e34 http and http-log pins. http-log:765672a ports `AccessLog::handle` to task (3/3 ctest, macOS Debug).
- 2026-11-03: Xapiand:42d5d4f9b item 6; Xapiand:d2e6901c3 the H1 move, AsioHost on the reactors' schedulers, `Run` deleted, `Counted`, tests; Xapiand:44a5ff985 the rule test after an Asio completion (Cinline); Xapiand:6408e2200 mechanism notes; Xapiand:1e9b90042 changelog. The AWS lease became unreachable (AWS session expired) mid-run; the Linux floor ran in strata's local VM (`~/httpwave`, removed after). Kept: `build-asan` (macOS ASan, reusable), `.scratch/` scripts (`build-asan.sh`, `run-tests.sh`, `harness*.sh`, `mutants.*`, `d9.sh`, `vm.sh`).
- 2026-11-03 (wave 2): merged master (task:d080786); Xapiand:c3af6d28a http:93dc7bb, http-log:616c80a and `read_chunk` on `BodyStream::read`, with the grace sized from D16's budget; Xapiand:927acf570 http:73cfa02 and the Debug workarounds deleted; Xapiand:22ed28a80 the late-write node test; Xapiand:7b1dcb563 and Xapiand:095402c97 the lag test paced and its failure printouts; Xapiand:376eebf23 the schema-load printout; the mechanism notes. macOS `build-asan` reconfigured without the `lz4` source override (strata's `COMPRESSORS_LZ4_PINNED` makes compressors fetch lz4 1.10 on a reconfigure). The VM's disk filled (my ASan build is 20 GB): removed after its ctest.
