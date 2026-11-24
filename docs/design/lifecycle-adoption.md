> Status: DRAFT, fix round 1 (2026-11-19), ready for review (lens `design`); folds the design
> review's F1-F18. Line numbers are at Xapiand:dc340b302 (src unchanged at 0669d9415),
> task:b7e2449, reactor:01ea267, http:58d4188 and Detent:3611216f. Nothing here is built.

# Xapiand adopts task::lifecycle

The quest list that moves Xapiand's start, stop and housekeeping onto `task::lifecycle` and the
server composition (cpp-libraries:04a75c0 `docs/server-composition.md`, "the note"), retires
`src/manager.cc`, and drops the scheduler and debouncer pin.

## Summary

Eleven landable quests: four in the libraries the note puts first (task, reactor, http), seven in
Xapiand. Eight are high risk: the library primitives shutdown leans on, the timers' bounded join,
the signal path and the driver (durability), the fsyncher, and the options and process steps
(user-visible). Five owner rulings (Q1-Q5), each with a recommended default.

Terms: a *component* has `start(stop)`, `stop()`, `drain(bound)` and `flush()`; `lifecycle` starts
components in order and, at a stop, stops and drains them in reverse, then flushes them
(`lifecycle.hh:72-83,556-560`). A *stop source* requests a stop: the signals (`stop_signals`), a
test, the admin QUIT, a tool's end (Xapiand D132). *Core files*: `src/manager.{cc,h}`,
`src/main.cc`.

## What the code does today

| Piece | Where | Fact that shapes the plan |
| :---- | :---- | :---- |
| Signals | `main.cc:232-262`; `manager.cc:459-498,579-697` | A self-pipe the loop reads; `join()` cancels the reader (`:1057`), so later signals only bump `shutdown_signals`. The dump runs on the loop (`:595-600`), never in `join()` |
| First signal | `manager.cc:686-758` | Listeners stay open while the stop waits for `http_clients == 0`, retried every 5 s, forced after 6 tries |
| Stop | `manager.cc:1033-1219` | `join()`: one deadline (`:1045`), a 50 ms poll, `drain_control` bounded by the deadline (`:1007-1015`), Stops 1-5, `_Exit(EX_TEMPFAIL)` on abandon (`:1191-1198`) |
| WAL, fsync | `manager.cc:1155-1187`; `wal.cc:871-916` | Stops 4-5 loop on `join(500ms)`, unbounded while `atom_sig >= 0`; `finish()` returns at once to a second caller; `join()` has no guard against two |
| HTTP stop | `reactor.h:792-814` | `stop()` cancels in-flight requests at once; no listeners-only phase, no wait for in-flight zero |
| Limits | `manager.cc:942-958`; `opts.cc:50,184-185,197,201`; `main.cc:338-490` | Plain HTTP has no connection bound, header or inactivity timeout (TLS only, `:945-947`); `--max-clients` (1000) feeds only the descriptor budget, which runs after `demote`; TLS is already `strict` (`tls.cc:62`) |
| Global state | `manager.h:235-303` | 37 `XapiandManager::` lines in 8 non-core files; 59 `DatabaseHandler` construction sites |
| Daemon | `main.cc:623-649,1015-1024` | `detach()` forks before the handlers exist and exits the parent at once; `writepid` takes no lock |
| Scheduler pin | `CMakeLists.txt:546-555,946-973` | Users: the fsyncher (`xapiand_fsyncher.h:72-76`, 1000 ms throttle) and `debounce_or_run.h` (`wal.cc:108,117`, passing `full_fsync`) |

## Decisions

| # | Decision | Lost, and why |
| :---- | :---- | :---- |
| D1 | The library quests the note's adoption order names (http `Limits`, reactor `server_context` and `raise_open_files`, task's process step) are filed here (Detent D81) | Waiting for Detent, whose adoption follows Xapiand's (D122) |
| D2 | reactor exposes the stop as phases: `stop_accepting()`, `wait_requests(cancel_token) -> bool` (woken by the gate's last leave once stopping, no poll), `cancel_requests()`, `force_connections()`. The legacy `stop()` stays `stop_accepting`, `cancel_requests`, grace, `force_connections`, grace, join, with no wait, for Xapiand before 9, Prism and Detent | Today's `stop()`: cancels at the first signal, against D71. A legacy `stop()` that waits: every unadopted app changes |
| D3 | Signals land before the driver: `stop_signals` feeds today's `join()`. Stop is today's first-signal path; hurry and the third signal act per the table below, as `lifecycle` does in 9 | One quest: 2,000 lines, two durability races at once. Hurry as `insisted()`: Stop 3 abandons indexes uncommitted, against D71 |
| D4 | State by reference (C6) before the driver: a `server::State` of references (registry, schemas, resolver, WAL writer, autocommit), passed to `SearchService`, `DatabaseHandler` and the tools | A thread-local: implicit. Manager-built state in components: churn |
| D5 | The fsyncher becomes `task::debounce<int>` keyed by fd, with today's timing. task gains `touch(key, flags)`, the flags OR-ed into the pending entry and passed to `fire(key, flags)` (`full_fsync`); `timing::throttle`, which keeps a fired key's entry until `throttle` passes (bounded by the keys fired in the last `throttle`); and `fire_pending()`, which takes the pending keys and fires them on the calling thread | Dropping the throttle: fsync rates change unmeasured. Key `(fd, full)`: one fd fsyncs twice. A pending map in Xapiand: a second map beside debounce's |
| D6 | The core files are serialized: each quest editing them `needs` the previous one; every other glob is disjoint | One 3,000-line quest, or one moving 1,400 lines the later ones rewrite |
| D7 | Registry and engine timers stay on `AsioHost::arm` (service-shape Timers rows 3-5); the loop stays live until `lifecycle` returns | Moving them: allocation work (service-shape D1) nothing here needs |
| D8 | One latched flush per owner: `DatabaseWALWriter::flush()` (finish, join every writer) and the fsyncher's. Each holds a mutex for its whole run and sets `done` last, so a later caller blocks until the first finished, and only the holder joins threads. Stops 4-5 and the third signal's `run_flushes()` call them, the fsyncher registered first so the WAL runs first (`lifecycle.hh:270-281`). The registrations die before the resets (`manager.cc:1203-1214`), waiting out a run in progress (`lifecycle.hh:293-296`). 9's Registry `flush()` and 10's debounce sit behind the same latches | An `exchange` like `finish()`'s (`wal.cc:911`): the second caller returns early, and the third signal raises over unwritten WAL. `std::call_once`: the same wait, read as init |
| D9 | In 9 Http's teardown runs beside the live control loop, whose work reaches HTTP schedulers: a turn (`pool.h:215`) handed to a waiting request (`task.hh:1928-1949`, via `resume_on`, `:1718,1855`), a chore's resume (`pool.cc:124-133`), a cross-thread cancel (`task.hh:1564-1607`). So 9 closes hunt row 105 F2: Http's drain thread runs phases 1-3 (join the loops, `close()`, `destroy_roots()`); a post after `close()` is dropped, one that passed the check first lands in an io_context never run again and re-checks `closed()` (`task_asio.hh:76-97`), and remote timer filings are posted too (`:160-174,206-216`), so the wheel release (`:110`) touches nothing a producer holds. Phase 4 runs in Http's destructor, after the control loop joined | Showing no producer reaches an HTTP scheduler: false. Deferring row 105 F2: the hazard starts in 9 |
| D10 | The dump reads through a `dump_target`, a mutex and the pointers it reads, which `join()` clears under the mutex after Stop 5, before the resets: a dump during any Stop prints the registry and schemas, one in progress ends first, a later one prints only threads. Its reads are safe beside requests and teardown: atomics (`manager.cc:1401-1406`), a catalog snapshot (`pool.cc:965-983`), two mutexes (`schemas_lru.cc:969-971`). In 9 `main` clears it when `lifecycle` returns | Clearing at the first stop: no dump of a wedged stop. No guard: a use-after-free |

### The second and third signal (D3)

| A second signal during | Today | From 6 (and in 9) |
| :---- | :---- | :---- |
| The first-signal wait | Breaks the loop, if 200 ms-1 s after the first (`manager.cc:662-671`) | Breaks the loop, whenever it comes |
| Stop 1, HTTP | Abandons the service (`:1085-1092`); exit 75 | Caps the wait at the hurry plus the 5 s grace (`lifecycle.hh:425-433`), then abandons what is left; exit 75 |
| Stop 2, autocommits | Not consulted | Not consulted |
| Stop 3, indexes | Abandons busy indexes uncommitted (`:1140-1148`), for WAL replay; exit 75 | Not consulted: indexes commit (D71), in 6 within the bound, in 9 unbounded (service-shape step 3) |
| Stops 4-5 | Not consulted, unbounded | Not consulted, unbounded (D8) |

A third signal today: while the loop runs, `-EX_SOFTWARE` (`:647-660`) makes `run()` throw
(`:833-836`) and the Stops exit 70 without waiting out the index or WAL joins
(`:1139-1142,1163-1167,1182-1186`); in `join()` it is one more `insisted()`. From 6: the WAL
writers flush, then the fsyncher (D8), then the signal is re-raised (130 or 143).

## Quests

Sizes are diff estimates. *test*: fails without the change (`campaign job submit
--fails-without`, at a mutant or the parent). *hook*: a test-build hook holding a thread at a
named point. *TSan*, *ASan*: sanitizer jobs. *strict*: a `quiet strict` timing job.

### 1. task-lifecycle-followups (task), high, about 450 lines

Outcome: task's open lifecycle follow-ups and D5's debounce additions; high because `fire_pending`
is the fsyncher's third-signal path. Files: `task.hh`, `lifecycle.hh`, `task_timers.hh`,
`test/{lifecycle,timers}_test.cc`, `docs/mechanisms/lifecycle.md`. After task-driver-layout.

- A1 F6: a `post_call` landing as `run()` exits runs on the next `run()`, counted by `queued()`: test.
- A2 F3 (Q3): the inline-flush fallback is reported in `stopped`: test, thread start failed by a hook.
- A3 R3 (Q1): the `fail()` that caused the stop is `stopped.failure` though a later drain throws: test.
- A4 N4 (Q2): `stopped.abandoned` is set when a drain reached `abandon`: test.
- A5 D5's flags, throttle and `fire_pending()`: tests with mutants; hooks pin `fire_pending()` against a key mid-fire on the loop (fired once) and a touch after `stop` (returns false). Suite, TSan and ASan.

### 2. reactor-server-composition (reactor), high, about 950 lines

Outcome: the note's steps 4-5 in reactor, plus D2; high because `wait_requests` carries D71's first
signal. Files: `reactor.h`, `reactor_tls.h`, `test/**`, `docs/**`, `CHANGELOG.md`.

- A1 `tls::server_context` takes Xapiand's checks (`tls.cc:51-76`: key mode, dates, pair) and sets `strict`; `tls::seconds_to_expiry` feeds the 14-day warning: a test each.
- A2 `raise_open_files(wanted)` raises the soft limit toward the hard one, returning the limit in force: test.
- A3 After `stop_accepting()` a new connection is refused while an in-flight request completes; `wait_requests` returns true at zero, false at its token, and true when a hook puts the last leave between the flag's store and the wait: tests, TSan.
- A4 Legacy `stop()` cancels an in-flight request at once: test, mutant inserting `wait_requests`.
- A5 The gate's leave stays within A/A noise on the request bench: strict A/B. Suite and CI.

### 3. http-limits (http), normal, about 1,200 lines

Outcome: the note's C1, Settled 1 and 6. Files: `http_asio.h`, `http_asio_tls.h`, `test/**`,
`docs/**`, `examples/**`, `CHANGELOG.md`. Needs 2.

- A1 `HttpAsioService(handler, reactors, const Limits&, Tls)` replaces the setters, `tls_connection_factory` and `AsioResponseOptions`' limits: a test per field at its bound, plain and TLS.
- A2 Idle, header and inactivity timeouts, plain and TLS; `close_idle()` closes idle connections at once and busy ones after their response: tests.
- A3 After `stop_accepting()`, a request on an open keep-alive connection is refused and an in-flight response carries `Connection: close`: test.
- A4 Settled 1: a held poll with complete headers and no body read trips neither timeout: test. Suite, TSan and CI.

### 4. task-process-step (task), normal, about 400 lines

Outcome: the note's C7: Detent's `run_daemon`, `ProcessLock` and `locked_pid`
(`detent_cli/daemon.hh:10-30`) move to task. Files: `process.hh`, `test/process_test.cc`,
`docs/mechanisms/process.md`, `CMakeLists.txt`. After task-driver-layout.

- A1 A detached child's `ready()` releases the parent with 0; a throw before it exits the parent with its status and message; the startup timeout exits nonzero: tests (fork).
- A2 A second lock on a held path fails; `locked_pid` names no process for a stale file: tests. Suite and CI.

### 5. lifecycle-timers (Xapiand), high, about 850 lines

Outcome: the control loop's tasks, housekeeping and autocommit run on `task::group`, `periodic` and
`debounce`; `control_plane.h` goes. The stop's drain stays bounded: `group::join(token)`, the token
fired at `join()`'s deadline (`task_timers.hh:64`), replaces `drain_control`. Closes hunt row 117
and sends detent-lead its reply. Files: `src/control_plane.h`, `tests/test_control_plane.cc`, core
files, `CMakeLists.txt`, `docs/mechanisms/control-plane.md`. Needs task-driver-layout; after
restore-exit-status, light-share and stopword-lists.

- A1 Suite at the new pin, with the housekeeping test (`test_node.cc:683-687`) and `graceful_shutdown_check.sh` scenario 4 (a debounced write survives SIGTERM and restart): jobs.
- A2 `git grep -n 'control::' src` is empty: check job.
- A3 A control task ignoring its stop (hook) lets the node exit within `shutdown_write_bound`, with today's warning and status: test, mutant with an unbounded join. TSan on both tests.

### 6. stop-signals (Xapiand), high, about 750 lines

Outcome: `task::stop_signals` owns SIGINT and SIGTERM per D3, and dumps SIGUSR1 and SIGINFO on its
thread through D10; D8's latches. Built after `detach()`'s fork (its thread would not survive it),
it lives until `server()` returns; a `cancel_callback` on its stop points stdin at `/dev/null`
(`main.cc:237-243`). The self-pipe, `signal_loop` and the windows go. Files: core files,
`src/database/wal.{h,cc}`, `src/xapiand_fsyncher.h`, `harness/{graceful_shutdown,signal}_check.sh`,
`tests/test_{stop_signals,coverage_main}*`, `harness/coverage_e2e_main*`,
`docs/mechanisms/stop-signals.md`, changelog. Needs 5; after coverage-core-main.

User-visible (D71): a second signal counts whenever it comes, gives requests one 5 s grace, and
indexes still commit; a third flushes the WAL writers and the fsyncher, then dies by its signal
(130 or 143), where today it can exit 70 first; a SIGTERM during start-up exits 0, not 70
(`main.cc:258-261`).

- A1 Three SIGTERMs 300 ms apart (POSIX merges pending ones) with acknowledged writes queued in the WAL: a restart reads them all: test, mutant without the flush registration. The fsync half is 10's.
- A2 F1: a hook holds Stop 3; a second SIGTERM; the hook releases: exit 0, and the write is in the index's commit, not only the WAL: test, mutant keeping `insisted()` in Stop 3.
- A3 F2: the third SIGTERM while a hook holds Stop 4 between two writers' joins: one finish and join per writer, the raise after the last join, status 143, every write read back: test under TSan, mutant with an `exchange` latch.
- A4 F4: SIGUSR1 with the control loop wedged by a hook; with requests in flight, under TSan; during a hook-held Stop 3, then exit 0 with writes intact: tests.
- A5 F5: a SIGTERM before the manager subscribes stops the node, status 0; a restore reading a held-open stdin ends at SIGTERM: tests.
- A6 `graceful_shutdown_check.sh` scenario 2: two SIGINTs cancel the request in flight, exit 0, writes survive: job. Suite and CI.

### 7. state-by-reference (Xapiand), normal, about 1,000 lines

Outcome: D4. Requests, tools and tests reach state through `server::State&`; the resolver
forwarding (`manager.cc:330-335,1259-1283`) goes. Files: `src/server/state.h`,
`src/database/{handler.*,shard.cc,wal.cc,schemas_lru.*,wide_search.cc,lock.h,schema.cc}`,
`src/index_resolver_lru.*`, `src/lua/processor.cc`, `src/server/search_views.cc`, core files,
`tests/test_{node,loop_yield,pool,reactor_requests}.cc`, `harness/async_allowlist.tsv`. Needs 6;
after cheap-reopen and stopword-lists.

- A1 `git grep -nw 'XapiandManager'` outside the core files lists only the QUIT and FLUSH views: check job.
- A2 One process builds two `State`s whose pools share no index: test. Suite, TSan and CI.

### 8. server-limits (Xapiand), high, about 1,000 lines

Outcome: the note's steps 4-5 in Xapiand. Options map onto `http::Limits`; the TLS context comes
from `reactor::tls::server_context`, and the global `tls_context` goes; the descriptor step runs
before `demote`. `http_shutdown_grace(left)` and its reserve go (`manager.cc:145-168,1078-1079`):
the grace is `Limits`' fixed 5 s (D77). Files: `src/opts.*`, `src/tls.*`, core files,
`CMakeLists.txt`, `tests/test_node.cc`, `harness/{tls,limits}_check.sh`,
`harness/coverage_e2e_main*`, `tests/test_coverage_main*`, `site/src/content/docs/options.md`,
changelog, `docs/mechanisms/server-limits.md`. Needs 2, 3, 7.

User-visible: `--max-clients` bounds plain and TLS connections, idle ones counted, default 1024
(D72); `--tls-max-connections` goes; `--idle-timeout-ms` (75000) and the 30 s header and 60 s
inactivity timeouts cover plain HTTP (Settled 1); `--shutdown-timeout-ms` (35000) sets the bound
(D77), and under about 15 s a stalled peer makes the stop abandon at it (exit 75) rather than
shrink the grace; `--reactors` defaults from `--processors`; an explicit `--max-clients` that does
not fit exits `EX_CONFIG`, a default shrinks with a warning (D133).

- A1 `--max-clients 10` with ten idle connections held: the 11th is refused, plain and TLS: `limits_check.sh`, fails without on plain.
- A2 Under `ulimit -n 256`, an explicit misfit exits `EX_CONFIG` naming both numbers; the default shrinks, warns and serves: `limits_check.sh`, mutant.
- A3 A plain keep-alive connection closes at `--idle-timeout-ms 1000`; a plain request stalled mid-headers or mid-body closes at its timeout: tests.
- A4 The grace tests (`test_node.cc:862,1061`) pin the fixed 5 s: test.
- A5 `--tls-max-connections` exits `EX_USAGE`; `tls_check.sh` passes; suite and CI on Linux, macOS, FreeBSD and arm64: jobs.

### 9. xapiand-lifecycle (Xapiand), high, about 1,500 lines

Outcome: `main` runs `lifecycle(loop, signals, {bound}, ready, registry, schema_cache, autocommit,
http)`. Http's `stop()` is `stop_accepting()`; its drain waits for requests on `b.cancel`, cancels,
forces at `b.abandon`, then runs `IndexChores::cancel_all` and phases 1-3 on its own thread (D9).
The Registry drains on the live loop with no token; its `flush()` takes D8's latches. `join()`,
`shutdown*`, the retry, `drain_control`, `shutdown_wait.h` and `atexit(cleanup_manager)` go; dump
and restore run the Registry under `lifecycle`, the tool's end their stop source. Files:
`src/server/{components.*,search_views.cc}`, core files, `src/shutdown_wait.h`,
`src/database/pool.*`, `tests/test_{node,loop_yield,control_plane,blocking_guard}.cc`,
`tests/test_lifecycle*`, `harness/{graceful_shutdown,signal}_check.sh`,
`harness/coverage_e2e_main*`, `docs/mechanisms/control-plane.md`,
`site/src/content/docs/reference-guide/api.md`, changelog. Needs 8 and 1; after light-share and
restore-exit-status.

User-visible: the first signal closes the listeners and refuses new keep-alive requests while
in-flight ones finish within the bound (D71); QUIT takes SIGTERM's path; FLUSH?clients closes idle
connections, bare FLUSH also flushes databases (D76); a commit stalled past the bound no longer
exits 75 by itself, the third signal ends it.

- A1 First SIGTERM with a slow request in flight: a new connection, and a new request on an open keep-alive connection, are refused at once; the slow one completes 200, exit 0: test.
- A2 QUIT stops the same way, exit 0; FLUSH?clients closes an idle connection; bare FLUSH also commits: tests.
- A3 A request suspended on a shard turn at stop: exit 0 in order (service-shape step 4): test.
- A4 A drain stalled past bound and grace exits `EX_TEMPFAIL` listing what it left: test.
- A5 A second signal during a hook-held Registry drain: the commit happens, exit 0, queued WAL writes read back: test, mutant without the Registry's `flush()`.
- A6 D9: with Http's drain thread held by a hook after the loops join, then between `close()` and `destroy_roots()`, a control-loop task hands a turn to a request suspended there and a chore completes for another: each frame is destroyed once, no resume runs, exit 0: test under ASan and TSan.
- A7 6 A3 at a held end-of-path flush, and 6 A4's dump during a held Registry drain, on this path under TSan.
- A8 `shutdown_wait`, `drain_control`, `run_control` and `shutdown_retry` are gone: check job. restore-exit-status's test, TSan and ASan on test_node and the harness, suite and CI pass.

### 10. fsyncher-on-task (Xapiand), high, about 600 lines

Outcome: D5. The fsyncher is a `task::debounce<int>` on the Registry's group, firing into a
`WorkPool` of `num_fsynchers` threads, its `flush()` behind D8's latch; the scheduler pin, the
stash header loop, their trace headers and `debounce_or_run.h` go (hunt row 105 F5). Files:
`src/xapiand_{fsyncher,scheduler_trace,stash_trace}.h`, `src/debounce_or_run.h`,
`src/database/wal.cc`, `src/logger_fwd.h`, `src/server/components.cc`, `src/manager.cc`,
`CMakeLists.txt`, `tests/test_{debounce_or_run.cc,fsyncher*}`, `docs/mechanisms/fsyncher.md`.
Needs 9 and 1.

- A1 Every fd touched before a stop is fsynced, with its `full_fsync`, before `flush()` returns, on the normal path and at the third signal: test with an fsync-counting hook, mutant without `fire_pending()`.
- A2 A WAL write load keeps its fsyncs per fd within 5%: strict A/B.
- A3 The crash harness's WAL rows pass: job.
- A4 `CMakeLists.txt` names no scheduler or stash; suite, TSan and CI pass.

### 11. process-and-node (Xapiand), high, about 1,100 lines

Outcome: the note's step 2 and the rest of `manager.cc`. `run_daemon` replaces `detach` and
`writepid`; the pid file is locked for the process's life; the Registry holds an exclusive `flock`
on the data directory (tools per Q5); node identity (`manager.cc:337-456,762-824`) is a module
`main` calls before `lifecycle`; metrics (`:1286-1380`) are a function over the components'
counters; `manager.*` go. Files: core files, `src/node_identity.*`, `src/server/server_metrics.*`,
`src/server/{components.h,search_views.cc}`, `harness/{daemon_check.sh,coverage_e2e_main*}`,
`tests/test_{coverage_main*,node.cc}`, `docs/mechanisms/process-step.md`,
`site/src/content/docs/options.md`, changelog. Needs 10 and 4.

User-visible (C7): `--detach` returns once the node is ready, or with its startup error and status;
a second instance exits per Q4; tools on a live node's directory act per Q5.

- A1 `--detach` with a taken port exits nonzero with the error; a good start exits 0 once the port answers: `daemon_check.sh`, fails without.
- A2 A second instance on the same pid file, and on the same data directory, exits per Q4 while the first serves; a dump on the live directory acts per Q5: `daemon_check.sh`.
- A3 `src/manager.*` are gone; metric names are unchanged (`/:metrics` diff): check job. Suite and CI.

## Order

```text
task-driver-layout -> lifecycle-timers -> stop-signals -> state-by-reference --+
reactor-server-composition -> http-limits ------------------------------------+-> server-limits
server-limits + task-lifecycle-followups -> xapiand-lifecycle -> fsyncher-on-task
fsyncher-on-task + task-process-step -> process-and-node
```

The library quests run beside the first three Xapiand quests. The review's optional reorders do
not shorten the chain: 10 after 1 and 5 still edits Stop 5 in `manager.cc` (D6) and collides with
7 on `wal.cc`; splitting 11 adds a quest.

## Collisions with active quests

| Active quest (live set) | Collides with | Recommendation |
| :---- | :---- | :---- |
| restore-exit-status: `src/main.cc`, `src/manager.cc`, changelog | 5-11 | Before 5 (its `after`); 9 keeps its test passing |
| light-share: `src/manager.cc` | 5-11 | Before 5; its wiring moves into Http (9) |
| stopword-lists: `CMakeLists.txt`, `handler.cc`, `schema.cc`, `tests/test_control_plane.cc`, docs | 5-11 | Before 5 |
| cheap-reopen: `src/database/pool.*`, `handler.cc`, `tests/test_{node,pool}.cc` | 7, 8, 9, 11 | Before 7 |
| coverage-core-main: `tests/test_coverage_main*`, `harness/coverage_e2e_main*` | 6, 8, 9, 11 | Before 6, which moves its `sig_handler` pins |
| serialize-validation: `src/database/schema.cc` | 7 | Before 7 |
| task-driver-layout: task `task.hh`, `test/**`, `CMakeLists.txt` | 1, 4 | Both after it |

coverage-server, crash-d-rows, keymaker-keys and coverage-core-small share no glob.

## Left out

1. Registry and engine timers onto the wheel (D7): a quest after 9. Row 105's F2 closes in 9, its F5 in 10.
2. Start-up and shutdown fault hooks (hunt row 198): a coverage quest after 11.
3. Config-file keys (the note's Deviations) and Detent D125's secrets file (`--tls-key` already refuses loose modes).
4. `--upstream-ca` and mutual TLS: Xapiand dials nothing (C9).
5. Renaming `http_clients`, which counts requests: metric names are user-visible.
6. Detent's and Prism's adoptions (Detent D122, Xapiand D138).
7. `FDS_PER_CLIENT` (2, `main.cc:86`) versus the note's one per connection: measured in 2.

## Open questions for the owner

- Q1 (task R3) A later drain error can hide the `fail()` that caused the stop. (a) The causing `fail()` wins; (b) keep last. Recommend (a).
- Q2 (task N4) `main` exits `EX_TEMPFAIL` only when a drain returned lines. (a) Add `stopped.abandoned`; (b) rely on lines. Recommend (a).
- Q3 (task F3) Report the inline-flush fallback? Recommend yes, a `stopped` field `main` logs.
- Q4 A second instance (Detent exits 0 "already running", `detent_authority/main.cc:156`). (a) Held pid file exits 0, held data directory `EX_TEMPFAIL` naming the holder's pid; (b) both nonzero; (c) both 0. Recommend (a): a directory another process holds is not "already running".
- Q5 (F12) The data-directory `flock` binds dump and restore too, which run the Registry from 9, so on a live node's directory they are refused, narrower than today. (a) Refuse naming the holder's pid, in 11's user-visible text and changelog; (b) a shared mode for the tools; (c) exempt them. Recommend (a): a second writer on a live directory is unsafe, and the HTTP API still dumps.

## Limits

- Seven quests edit the core files in sequence (D6): a slow one holds the chain.
- Between 6 and 9 the first signal still waits on `http_clients` with the listeners open.
- D8 leaves a WAL writer stuck on its disk unkillable by SIGTERM (the third signal waits on its latch); SIGKILL remains. `join()` behaves so today (`manager.cc:1163-1167`).
- A dump blocked on a lock holds later signals on the signal thread, and D10's clear waits for it.
- Sizes are estimates; nothing is built or measured. A request leaving without the gate (none known) would hold `wait_requests` to `b.cancel`.

## First steps

1. File quests 1-7 with the sets above; start 2 now, 5 once restore-exit-status, light-share and stopword-lists land, 6 once coverage-core-main lands.
2. Ask the owner Q1-Q5; quest 1 builds A1 and A5 meanwhile.
3. When 2 and 1 land, fold D2 into the note's reactor contract and D5 into service-shape (F14).
