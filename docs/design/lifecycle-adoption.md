> Status: DRAFT (2026-11-19), ready for review (lens `design`). Line numbers are at
> Xapiand:dc340b302, task:b7e2449, reactor:01ea267, http:58d4188 and Detent:3611216f. Nothing here
> is built.

# Xapiand adopts task::lifecycle

The quest list that moves Xapiand's start, stop and housekeeping onto `task::lifecycle` and the
server composition (cpp-libraries:04a75c0 `docs/server-composition.md`, "the note"), retires
`src/manager.cc`, and drops the scheduler and debouncer pin.

## Summary

Eleven landable quests: four in the libraries the note's adoption order puts first (task, reactor,
http) and seven in Xapiand. Three Xapiand quests can start now (timers, signals, state by
reference); the rest wait on the library quests. Four of the eleven are high risk: the signal path
and the lifecycle driver (shutdown and WAL durability), the fsyncher (durability), and the options
and process step (user-visible contracts). The plan needs four owner rulings (Q1-Q4); each has a
recommended default.

Terms: a *component* has `start(stop)`, `stop()`, `drain(bound)` and `flush()`; `lifecycle` starts
components in order and, at a stop, stops and drains them in reverse, then flushes them
(`lifecycle.hh:78-86,490-570`). A *stop source* is anything that requests a stop: the signals
(`task::stop_signals`), a test, the admin QUIT, a tool's end (Xapiand D132). *Core files* are
`src/manager.cc`, `src/manager.h` and `src/main.cc`.

## What the code does today

| Piece | Where | Fact that shapes the plan |
| :---- | :---- | :---- |
| Signals | `main.cc:232-262,288-336`; `manager.cc:459-498,579-697` | A handler writes a self-pipe the control loop reads; "you insisted" uses 200 ms-1 s windows; the SIGUSR1/SIGINFO dump runs on the loop (`manager.cc:595-600`) |
| First signal | `manager.cc:686-758` | Listeners stay open; the stop waits for `http_clients == 0`, retried every 5 s (`:706-709`), forced after 6 tries (`:690-693`) |
| Stop | `manager.cc:1033-1219` | `join()`: one deadline (`:1045`), a 50 ms poll in `shutdown_wait` (`:1081`), a 10 ms `drain_control` (`:1007-1015`), Stops 1-5, `_Exit(EX_TEMPFAIL)` on abandon (`:1191-1198`) |
| HTTP stop | reactor `reactor.h:792-814` | `stop()` cancels in-flight requests at once; there is no "close listeners only" phase and no wait for in-flight zero |
| Limits | `manager.cc:942-958`; `opts.cc:50,184-185,197,201` | Plain HTTP has no connection bound (`:950`); `--max-clients` (1000) feeds only the descriptor budget |
| Budget, TLS | `main.cc:338-490,801,1110,1118`; `tls.cc:51-76` | The budget runs after `demote` and shrinks an explicit `--max-clients` silently |
| Global state | `manager.h:235-303` | 37 lines with `XapiandManager::` in 8 non-core source files (10 include `manager.h`), and 59 `DatabaseHandler` construction sites |
| Admin | `search_views.cc:1537-1541,1557` | FLUSH?clients calls `shutdown(0, 0)`, a no-op (`manager.cc:705`); QUIT calls `try_shutdown` |
| Daemon | `main.cc:623-649,1015-1021` | `detach()` exits the parent at once; `writepid` takes no lock; `atexit(cleanup_manager)` joins |
| Scheduler pin | `CMakeLists.txt:546-555,946-973` | Users: the fsyncher (`xapiand_fsyncher.h:72-76`, a threaded debouncer with a 1000 ms throttle, `opts.cc:70`) and `debounce_or_run.h` (`wal.cc:108,117`). logger:455cad0 no longer includes it |
| Control plane | `control_plane.h` | `control::Tasks`, `Periodic`, `Debounce`: users only `manager.cc:181-184,300,707,826` and `tests/test_control_plane.cc` |

## Decisions

| # | Decision | Lost, and why |
| :---- | :---- | :---- |
| D1 | The library quests the note's adoption order names (http `Limits`, reactor `server_context` and `raise_open_files`, task's process step) are filed in this campaign, since Detent D81 has Xapiand take the http quest and no campaign has filed them | Waiting for Detent: its adoption is ordered after Xapiand's (Detent D122), so nobody would start them |
| D2 | reactor exposes the stop as phases: `stop_accepting()`, `wait_requests(cancel_token) -> bool` (blocking, woken by the gate's last leave once stopping, no poll), `cancel_requests()`, `force_connections()`; `stop()` keeps running them in order for the apps that have not adopted | Xapiand's Http drain built on today's `stop()`: it cancels at the first signal, against D71 |
| D3 | Signals land before the driver: `stop_signals` first feeds today's `join()` (stop runs today's first-signal path, hurry is today's "insisted", the third signal runs the WAL and fsync flushes) | One quest for signals and driver: about 2,000 lines and two durability races reviewed at once |
| D4 | State by reference (C6) before the driver: a `server::State` of references (registry, schemas, resolver, WAL writer, autocommit) built by its owner and passed to `SearchService`, `DatabaseHandler` and the tools | A loop-bound thread-local: still implicit; components holding state the manager builds: churn the driver quest then undoes |
| D5 | The fsyncher moves to `task::debounce` with today's timing, throttle included, and its `flush()` fsyncs every pending fd off the loop; task gains `timing::throttle` and `debounce::fire_pending()` (any thread: takes pending keys and fires them on the caller) | Dropping the throttle: changes fsync rates without a measurement. Xapiand keeping its own pending set: a second map beside debounce's |
| D6 | The core files are serialized: each quest that edits them `needs` the previous one. Every other glob is disjoint across the eleven quests | Disjoint sets for the core files: either one quest of about 3,000 lines, or a first quest that only moves 1,400 lines out of `manager.cc` into files the later quests then rewrite anyway, and that holds the core files against restore-exit-status and light-share for longest |
| D7 | Registry and engine timers stay on `AsioHost::arm` (service-shape's Timers rows 3-5): the loop stays live until `lifecycle` returns, so they fire during drains | Moving them here: allocation work (service-shape D1) with no lifecycle need |

## Quests

Sizes are estimates of the diff, deletions included. Evidence words: *test* means a test that fails
without the change, run with `campaign job submit --fails-without` at a mutant or the parent;
*TSan* and *ASan* are the sanitizer jobs; *strict* is a `quiet strict` timing job.

### 1. task-lifecycle-followups (repo task), risk normal, about 400 lines

Outcome: task's open lifecycle follow-ups and the two debounce additions Xapiand needs.
Files: `lifecycle.hh`, `task_timers.hh`, `test/lifecycle_test.cc`, `test/timers_test.cc`,
`docs/mechanisms/lifecycle.md`. After task-driver-layout (task's `test/**`).

- A1 F6: a `post_call` landing as `run()` exits runs on the next `run()` and `queued()` counts it: test.
- A2 F3, per Q3: the inline-flush fallback is reported in `stopped`: test with the thread start failed by a hook.
- A3 R3, per Q1: a `fail()` that caused the stop is `stopped.failure` even when a later drain throws: test.
- A4 N4, per Q2: `stopped.abandoned` is true when a drain reached `abandon`, with or without lines: test.
- A5 `debounce::timing::throttle` (no fire of a key within `throttle` of its last) and `fire_pending()` (D5): tests, each with a mutant.
- A6 Suite, TSan and ASan in CI.

### 2. reactor-server-composition (repo reactor), risk normal, about 900 lines

Outcome: the note's steps 4-5 in reactor, plus D2's stop phases.
Files: `reactor.h`, `reactor_tls.h`, `test/**`, `docs/**`, `CHANGELOG.md`. Needs nothing.

- A1 `tls::server_context(options)` refuses a group- or other-readable key, a not-yet-valid or expired certificate and a mismatched pair, and sets `strict`; `tls::seconds_to_expiry(context)` feeds the 14-day warning: Xapiand's `tls.cc:51-76` checks moved with a test each (mutant: drop the mode check).
- A2 `raise_open_files(wanted)` raises the soft limit toward the hard one and returns the limit in force: test in a child with a soft 1024 under a higher hard limit.
- A3 D2's phases: after `stop_accepting()` a new connection is refused while an in-flight request completes; `wait_requests` returns true at zero and false at its token: tests, TSan.
- A4 The gate's leave stays within A/A noise on reactor's request bench: strict A/B.
- A5 Suite and CI.

### 3. http-limits (repo http), risk normal, about 1,200 lines

Outcome: the note's C1 and Settled 1 and 6. Files: `http_asio.h`, `http_asio_tls.h`, `test/**`,
`docs/**`, `examples/**`, `CHANGELOG.md`. Needs reactor-server-composition.

- A1 `HttpAsioService(handler, reactors, const Limits&, Tls)`; the setters, `tls_connection_factory` and `AsioResponseOptions`' limit fields are gone: a test per field reaching its bound, plain and TLS (11th connection refused at `max_clients` 10).
- A2 Idle close at `idle_timeout` and header/inactivity timeouts on plain and TLS: tests.
- A3 `close_idle()` closes idle keep-alive connections at once and busy ones after their response: test.
- A4 After `stop_accepting()`, a request on an open keep-alive connection is refused and an in-flight response carries `Connection: close`: test.
- A5 Settled 1: a held poll with complete headers and no body read trips neither timeout: test.
- A6 Suite, TSan and CI.

### 4. task-process-step (repo task), risk normal, about 400 lines

Outcome: the note's C7: Detent's `run_daemon`, `ProcessLock` and `locked_pid`
(`detent_cli/daemon.hh:10-30`, 144 lines of `daemon.cc`) move to task. Files: `process.hh`,
`test/process_test.cc`, `docs/mechanisms/process.md`, `CMakeLists.txt`. After task-driver-layout.

- A1 A detached child's `ready()` releases the parent with 0; a run that throws before `ready()` makes the parent exit with its status and message; the startup timeout exits nonzero: tests (fork).
- A2 A second lock on a held path fails; `locked_pid` names no process for a stale file: tests.
- A3 Suite and CI. Detent's migration stays Detent's (server-composition-adoption).

### 5. lifecycle-timers (Xapiand), risk normal, about 850 lines

Outcome: the control loop's tasks, housekeeping and autocommit run on `task::group`, `periodic`
and `debounce` at a task pin with F7 fixed; `control_plane.h` is deleted; behavior unchanged.
Files: `src/control_plane.h`, `tests/test_control_plane.cc`, core files, `CMakeLists.txt`,
`docs/mechanisms/control-plane.md`. Needs task-driver-layout.

- A1 Suite at the new pin, with the housekeeping test (`test_node.cc:683-687`) and `graceful_shutdown_check.sh` scenario 4 (an uncommitted debounced write survives SIGTERM and restart): jobs.
- A2 `git grep -n 'control::' src` is empty; SingleFlight's tests stay: check job.
- A3 TSan on test_node and test_control_plane.

### 6. stop-signals (Xapiand), risk high, about 650 lines

Outcome: `task::stop_signals` owns SIGINT and SIGTERM (D3), and SIGUSR1 and SIGINFO as dump
signals run on its thread; the self-pipe, `sig_handler`'s manager path, `signal_loop` and the
200 ms-1 s windows are deleted. Files: core files, `harness/graceful_shutdown_check.sh`,
`harness/signal_check.sh`, `tests/test_stop_signals*`, `tests/test_coverage_main*`,
`harness/coverage_e2e_main*`, `docs/mechanisms/stop-signals.md`,
`docs/src/content/docs/changelog.md`. Needs lifecycle-timers; after coverage-core-main.

User-visible (D71): a second signal cancels requests and still commits and flushes, instead of
exiting at once; a third kills, with status 130 or 143, only after the WAL writers and fsyncher
flush.

- A1 Three SIGTERMs with acknowledged writes queued in the WAL: a restart reads them all back: test, mutant without the flush registration.
- A2 SIGUSR1 with the control loop wedged by a test hook still prints the dump: test.
- A3 `graceful_shutdown_check.sh` scenario 2 becomes: two SIGINTs cancel the request in flight, the node exits 0, its writes survive a restart: job.
- A4 TSan with the third signal arriving during Stop 4 (`flush()` runs beside `join()`).
- A5 Suite and CI.

### 7. state-by-reference (Xapiand), risk normal, about 1,000 lines

Outcome: D4. Requests, tools and tests reach state through `server::State&`; the resolver
forwarding (`manager.cc:330-335,1259-1283`) is deleted. Files: `src/server/state.h`,
`src/database/handler.*`, `src/database/shard.cc`, `src/database/wal.cc`,
`src/database/schemas_lru.*`, `src/database/wide_search.cc`, `src/database/lock.h`,
`src/database/schema.cc`, `src/index_resolver_lru.*`, `src/lua/processor.cc`,
`src/server/search_views.cc`, core files, `tests/test_node.cc`, `tests/test_loop_yield.cc`,
`tests/test_pool.cc`, `tests/test_reactor_requests.cc`, `harness/async_allowlist.tsv`. Needs
stop-signals; after cheap-reopen and stopword-lists.

- A1 `git grep -nw 'XapiandManager'` outside the core files lists only the QUIT and FLUSH views: check job.
- A2 One test process builds two `State`s whose pools do not share an index: test.
- A3 Suite, TSan and CI.

### 8. server-limits (Xapiand), risk high, about 950 lines

Outcome: the note's steps 4-5 in Xapiand. Options map onto `http::Limits`; the TLS context comes
from `reactor::tls::server_context` and the global `tls_context` goes; the descriptor step runs
before `demote`. Files: `src/opts.*`, `src/tls.*`, core files, `CMakeLists.txt`,
`harness/tls_check.sh`, `harness/limits_check.sh`, `harness/coverage_e2e_main*`,
`tests/test_coverage_main*`, `docs/src/content/docs/options.md`,
`docs/src/content/docs/changelog.md`, `docs/mechanisms/server-limits.md`. Needs
reactor-server-composition, http-limits, state-by-reference.

User-visible: `--max-clients` bounds plain and TLS connections, idle ones counted, default 1024
(D72, Settled 5); `--tls-max-connections` is gone (D72); `--idle-timeout-ms` (75000) on plain HTTP
too (D72); `--shutdown-timeout-ms` (35000) sets today's bound (D77); `--reactors` defaults from
`--processors`; an explicit `--max-clients` that does not fit exits `EX_CONFIG`, a default shrinks
with a warning (D133).

- A1 With `--max-clients 10` and ten idle connections held, the 11th is refused, plain and TLS: `limits_check.sh`, fails without on plain.
- A2 Under `ulimit -n 256`, an explicit misfit exits `EX_CONFIG` naming both numbers; the default shrinks, warns and serves: `limits_check.sh`, mutant.
- A3 An idle plain keep-alive connection closes at `--idle-timeout-ms 1000`: test.
- A4 `--tls-max-connections` exits `EX_USAGE`; `tls_check.sh`'s six scenarios pass: jobs.
- A5 Suite and CI on Linux, macOS, FreeBSD and arm64.

### 9. xapiand-lifecycle (Xapiand), risk high, about 1,450 lines

Outcome: `main` runs `lifecycle(loop, signals, {bound}, ready, registry, schema_cache, autocommit,
http)`. Http's `stop()` is `stop_accepting()`; its drain waits for requests on `b.cancel`, then
cancels, forces at `b.abandon`, and runs `IndexChores::cancel_all`, `close` and `destroy_roots`
on a thread of its own. The Registry drains on the live loop and flushes the WAL and fsyncher.
`join()`, `shutdown*`, the 5 s retry, `drain_control`, `shutdown_wait.h` and
`atexit(cleanup_manager)` are deleted; dump and restore run the Registry under `lifecycle` with the
tool's end as the stop source. Files: `src/server/components.*`, core files, `src/shutdown_wait.h`,
`src/server/search_views.cc`, `src/database/pool.*`, `tests/test_node.cc`,
`tests/test_loop_yield.cc`, `tests/test_control_plane.cc`, `tests/test_blocking_guard.cc`,
`tests/test_lifecycle*`, `harness/graceful_shutdown_check.sh`, `harness/signal_check.sh`,
`harness/coverage_e2e_main*`, `docs/mechanisms/control-plane.md`,
`docs/src/content/docs/reference-guide/api.md`, `docs/src/content/docs/changelog.md`. Needs
server-limits and task-lifecycle-followups; after light-share and restore-exit-status.

User-visible: the first signal closes the listeners at once and refuses new keep-alive requests
while in-flight requests finish within `--shutdown-timeout-ms` (D71); QUIT takes the SIGTERM path
through `request_stop()` and FLUSH?clients closes idle connections (D76).

- A1 First SIGTERM with a slow request in flight: a new connection is refused at once, the request completes 200, exit 0: test.
- A2 QUIT under `--admin-commands` stops through the same path and exits 0; FLUSH?clients closes an idle keep-alive connection: tests.
- A3 A request suspended on a shard turn at stop: the stop exits 0 in order (service-shape step 4): test.
- A4 A drain stalled past bound and grace exits `EX_TEMPFAIL` listing what it left: test.
- A5 A second signal with queued WAL writes: a restart reads them back: test, mutant without the Registry's `flush()`.
- A6 `shutdown_wait`, `drain_control`, `run_control` and `shutdown_retry` are gone: check job. restore-exit-status's test still passes.
- A7 TSan and ASan on test_node and the harness; suite and CI.

### 10. fsyncher-on-task (Xapiand), risk high, about 600 lines

Outcome: D5. The fsyncher is a `task::debounce<int>` on the Registry's group, firing into a
`WorkPool` of `num_fsynchers` threads; the scheduler pin, the stash header loop and their trace
headers go; `debounce_or_run.h` goes (hunt row F5). Files: `src/xapiand_fsyncher.h`,
`src/debounce_or_run.h`, `src/database/wal.cc`, `src/xapiand_scheduler_trace.h`,
`src/xapiand_stash_trace.h`, `src/logger_fwd.h`, `src/server/components.cc`, `src/manager.cc`,
`CMakeLists.txt`, `tests/test_debounce_or_run.cc`, `tests/test_fsyncher*`,
`docs/mechanisms/fsyncher.md`. Needs xapiand-lifecycle and task-lifecycle-followups.

- A1 Every fd touched before a stop is fsynced before `flush()` returns, on the normal path and at the third signal: test with an fsync-counting hook, mutant without `fire_pending()`.
- A2 A WAL write load sees the same fsyncs per fd within 5% before and after: strict A/B.
- A3 The crash harness's WAL rows pass: job.
- A4 `CMakeLists.txt` names no scheduler or stash; suite, TSan and CI.

### 11. process-and-node (Xapiand), risk high, about 1,100 lines

Outcome: the note's step 2 and the rest of `manager.cc`. `run_daemon` replaces `detach` and
`writepid`; the pid file is locked for the process's life; the Registry holds an exclusive `flock`
on the data directory; node identity (`manager.cc:337-456,762-824`) is a module `main` calls before
`lifecycle`; metrics (`:1286-1380`) are a function over the components' counters; `manager.cc` and
`manager.h` are deleted. Files: core files, `src/node_identity.*`, `src/server/server_metrics.*`,
`src/server/components.h`, `src/server/search_views.cc`, `harness/daemon_check.sh`,
`harness/coverage_e2e_main*`, `tests/test_coverage_main*`, `tests/test_node.cc`,
`docs/mechanisms/process-step.md`, `docs/src/content/docs/options.md`,
`docs/src/content/docs/changelog.md`. Needs fsyncher-on-task and task-process-step.

User-visible (C7): `xapiand --detach` returns after the node is ready, or with its startup error
and status; a second instance on a held pid file or data directory exits per Q4.

- A1 `--detach` with a taken port exits nonzero with the error; a good start exits 0 once the port answers: `daemon_check.sh`, fails without.
- A2 A second instance on the same pid file, and on the same data directory, exits per Q4 and leaves the first serving: `daemon_check.sh`.
- A3 `src/manager.*` are gone; metrics names are unchanged: check job and `/:metrics` diff.
- A4 Suite and CI.

## Order

```text
task-driver-layout -> lifecycle-timers -> stop-signals -> state-by-reference --+
reactor-server-composition -> http-limits ------------------------------------+-> server-limits
server-limits + task-lifecycle-followups -> xapiand-lifecycle -> fsyncher-on-task
fsyncher-on-task + task-process-step -> process-and-node
```

The four library quests run beside the first three Xapiand quests.

## Collisions with active quests

| Active quest (live set) | Collides with | Recommendation |
| :---- | :---- | :---- |
| restore-exit-status: `src/main.cc`, `src/manager.cc`, changelog | 5-11 | Land it first; xapiand-lifecycle keeps its test passing |
| light-share: `src/manager.cc` | 5-11 | Land it first; its "light in flight" wiring moves into the Http component in 9 |
| stopword-lists: `CMakeLists.txt`, `handler.cc`, `schema.cc`, `tests/test_control_plane.cc`, options docs, changelog | 5-11 | Land it before 5 |
| cheap-reopen: `src/database/pool.*`, `handler.cc`, `tests/test_node.cc`, `tests/test_pool.cc` | 7, 9, 11 | Land it before 7 |
| coverage-core-main: `tests/test_coverage_main*`, `harness/coverage_e2e_main*` | 6, 8, 9, 11 | Land it first; its pins on `sig_handler` and "Received SIGTERM scheduling shutdown" then move with 6 |
| serialize-validation: `src/database/schema.cc` | 7 | After it |
| task-driver-layout: task `task.hh`, `test/**`, `CMakeLists.txt` | 1, 4 | Both after it |

coverage-server, crash-d-rows, keymaker-keys and coverage-core-small share no glob.

## Left out

1. Registry and engine timers onto the wheel, and hunt row 105's F2 (D7): a quest after 9.
2. Fault hooks for start-up and shutdown coverage (hunt row 198): a coverage quest after 11.
3. Config-file keys: Xapiand has flags only (the note's Deviations). Detent D125's secrets file: Xapiand's one secret, `--tls-key`, already refuses group or other access.
4. `--upstream-ca` and mutual TLS: Xapiand dials nothing (C9).
5. Renaming the `http_clients` metric, which counts requests: metric names are user-visible.
6. Detent's and Prism's adoptions (Detent D122, Xapiand D138).
7. `--max-files` keeps its meaning as the explicit `wanted` of `raise_open_files`; `FDS_PER_CLIENT` (2, `main.cc:86`) against the note's one descriptor per connection is measured in 2, not assumed.

## Open questions for the owner

- Q1 (task R3) A `fail()` ranks last in `stopped.failure`, so a control task's failure that caused the stop can be hidden by a later drain error and lose its exit status. Options: (a) the `fail()` that caused the stop wins; (b) keep last. Recommend (a).
- Q2 (task N4) `stopped` has no abandoned flag; `main` exits `EX_TEMPFAIL` only when a drain returned lines. Options: (a) add `abandoned`; (b) rely on lines. Recommend (a).
- Q3 (task F3) Report the inline-flush fallback? Recommend yes, as a field of `stopped` that `main` logs.
- Q4 A second instance: Detent exits 0 "already running" on a held pid file (`detent_authority/main.cc:156`). Options: (a) a held pid file exits 0 as Detent does, a held data directory exits `EX_TEMPFAIL` naming the holder's pid; (b) both nonzero; (c) both 0. Recommend (a): one shape with Detent for the pid file, and a data directory held by another process is not "already running".

## Limits

- The core files are edited by seven quests in sequence (D6): a slow quest holds the chain.
- Silent: between 6 and 9, the first signal still waits on `http_clients` with the listeners open; D71's first-signal behavior arrives only with 9.
- Silent: `flush()` runs beside `join()`'s Stop 4 in 6 and 9; only TSan and the A1 restart test see a double finish of the WAL writers.
- `wait_requests` relies on the gate's leave noticing the stopping flag; a request that leaves without the gate (none known) would hold the drain to `b.cancel`.
- Size estimates are estimates; none of this is built or measured.

## First steps

1. File quests 1-4 and 5-7 with the sets above; start 2 and 5 now, 6 once coverage-core-main lands.
2. Ask the owner Q1-Q4; quest 1 builds A1 and A5 while they are open.
