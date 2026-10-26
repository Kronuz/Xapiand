# offload-bridge (engine-core CR1, CR2; pool CR5)

Branch `fix/offload-bridge` from Xapiand master `2726fd225` (local only, never pushed). Ruling:
`docs/reviews/engine-offload-architect.md`, conditions C1 to C6 (C7 is step 7c). Reactor marker:
Kronuz/reactor `feat/on-reactor-marker` `d381e4d` (pushed; the lead lands it). `CMakeLists.txt` pins
reactor at `d381e4d` (on reactor main; `fix/offload-followups` repins to `0c04d0d`).

## State

C1 to C6 done and verified (Debug ASan build, guard armed, Apple clang 17, macOS x86_64). Code
review (`docs/reviews/offload-bridge-code.md`, approve with conditions) folded in below: CR1, CR4,
CR6 done, CR2 recorded as a follow-up, CR5 is the lead's. Two database sites on reactor loops beyond the ruling's list were found by the guard and
fixed under C5 (shutdown readiness, DB_UPDATED on the bus loop). Not run: Linux, FreeBSD, timing.

## D13 as amended (C1)

The rule, for the lead to write into D13 (`strata/docs/design/xapiand-on-strata.md:224-235`; strata
is outside this branch):

> No reactor loop thread blocks on database work: no `sync_wait`, no `DatabasePool` checkout or
> cleanup, no shard open, commit, or close. A blocking caller runs on an offload worker (the serving
> reactor's `pool()`, or an existing named thread: DP, DI, the committer, WL, the cleanup thread) and
> the engine call runs inline on it. A coroutine caller on a reactor `co_await`s the task and never
> `sync_wait`s (from 7c). No thread waits, by `sync_wait` or otherwise, on a pool it is a thread of.
> Xapiand enforces it with `engine::check_may_block` (`src/engine/sync.h`).
>
> Residual: a remote-protocol connection's teardown (`~RemoteProtocolViews`, destroyed on the
> loop when `serve_remote_connection` ends) is not moved to the pool, so it is not covered by the
> rule today. `DatabasePool::checkin` carries the guard, so a teardown that checks a shard in on a
> loop aborts in Debug and is counted and logged in Release.

## Review conditions (code review, `docs/reviews/offload-bridge-code.md`)

- CR1 closed for replication: `ReplicationProtocolViews` is destroyed on the serving reactor's
  pool (`engine::release_on_pool`, `src/engine/release.h`) at the end of
  `serve_replication_connection` and `connect_and_replicate`, on every exit. Both
  `DatabasePool::checkin` overloads call `engine::check_may_block`. Residual: the remote protocol's
  teardown (GPL destructor, not read or touched here); the checkin guard catches it at runtime if
  it ever blocks. The `RemoteProtocolViews` local in `serve_remote_connection`
  (`remote_protocol_service.h`) could take the same hop.
- CR2 follow-up (done in `fix/offload-followups`, `docs/status/offload-followups.md`): `Node::touch_node(remote_node, false)` in `db_updated` now runs when the
  control worker reaches it, so a long `setup_node` or `load_nodes` ahead of it delays `last_seen`
  (compared with `stalled` in `index_resolver_lru.cc`); and `dispatch_command_async_cb` drains
  with `while (try_dequeue)` in one job, so a sustained DB_UPDATED rate can starve a queued
  shutdown readiness job. Refresh liveness on the loop at message time; bound the drain.
- CR4 closed in Kronuz/reactor `test/udp-inside-run` (`udp_test` checks the handler sees
  `reactor::inside_run()`).
- CR5: reactor pinned to main `0c04d0d`, comment removed (`fix/offload-followups`).
- CR6: the build was reconfigured so the binary's embedded `git:` matches the head.

## Done criteria

- [x] C1 The rule worded for D13 (above). The lead applies it in strata (outside this branch).
- [x] C2 One bridge, one guard: `git grep -n 'task::sync_wait' src` lists only `src/engine/sync.h`;
  `engine::sync_wait`, both `DatabasePool::checkout` overloads and `DatabasePool::cleanup` call
  `engine::check_may_block`: not inside a reactor's `run()` (`reactor::inside_run()`), and
  `task::current()` empty. Debug (asserts) prints the site and a backtrace and aborts; release counts
  and logs once per site.
- [x] C3 FLUSH offloaded (`search_service.cc`), both comments corrected; changelog bullet.
- [x] C4 The restore view finishes and drops `request.indexer` on its worker on every exit.
- [x] C5 The control-plane reactor has one worker: setup_node's database part, the dispatched
  commands (new_leader/load_nodes, the election's `lock_shard`, now also DB_UPDATED) and the shutdown
  readiness checks run there in order; state changes resume on the loop; a worker exception is
  rethrown on the loop; `join()` stops and joins the worker before the database pool finishes.
- [x] C6 `Offload`, `set_offload`, `XAPIAND_ENGINE_OFFLOAD_THREADS` removed; `Hop` takes a
  caller-chosen `task::scheduler_ref`; `test_engine_shard`'s pool case uses one.
- [x] The guard fires in `test_blocking_guard` for each site on a reactor loop and on a scheduler
  thread; mutants dropping either check fail 3 and 1 of its 8 checks.
- [x] Docs E2E GREEN/GREEN, 0 ASan reports, 0 guard hits, clean exit.
- [x] Hammer (`stallcount.sh 0`, 5 rounds of 16 clients with FLUSH) 0 timeouts, no 5xx; `uafhunt.sh`
  10 rounds, node alive, 0 ASan, 0 guard hits.
- [x] TSan (Apple clang 17): `test_pool` and `test_blocking_guard`, 3 runs each, 0 warnings.
- [x] Q1: no separate cap for Xapian I/O.

## Done (commits)

Kronuz/reactor `feat/on-reactor-marker` (pushed): `d381e4d` `reactor::inside_run()`, `RunScope`,
`Reactor::run()`; `TcpServer` and `UdpServer` drive their loops through it; `[M]` test (8 checks,
2 fail with the scope removed from `Reactor::run()`), ASan+UBSan clean.

Xapiand `fix/offload-bridge` (local):

- `fd580ed77` C1 ledger and the D13 text.
- `e355c62d8` C2 `engine::sync_wait`, `engine::check_may_block`, the guard test; reactor pinned at
  `d381e4d`, declared before `http_lib` so it wins over the `e19afd9` pins of http and cluster.
- `6e0a7e092` C3 FLUSH offloaded.
- `1388f4175` C4 restore indexer dropped on its worker.
- `288ad8010` C5 control plane's worker.
- `d49a1d0b8` C6 `Offload` retired.
- `e0f6c895f` C2 backtrace on a guard abort.
- `4d98274ec` C5 shutdown readiness checks on the worker.
- `0191530a2` C5 DB_UPDATED dispatched to the worker.
- `27a9dc463` CR1 replication views destroyed on the pool; `DatabasePool::checkin` guarded.

Kronuz/reactor `test/udp-inside-run` (pushed, from main `d381e4d`): `0c04d0d` CR4, `udp_test` checks
the handler sees `reactor::inside_run()`; with `UdpServer` back on `io().run()` that check fails (1
failure), restored it passes (0), ASan+UBSan.

## Log

### 2026-10-26

- Build: `.scratch/build-asan` (Debug, `-DASAN=ON -DBUILD_TESTS=ON`; asserts on). A first build with
  `-DUBSAN=ON` aborts at node start in RapidJSON (`stack.h:117`, null pointer plus offset, reached from
  `SearchService::on_error`; pre-existing, the pinned RapidJSON v1.1.0); `-fno-sanitize-recover=all`
  makes suppressions useless, so the runtime checks use ASan only, as the pool-concurrency evidence did.
- Unit tests (ASan): `blocking_guard` 8, `engine_shard` 52, `pool` 21, `engine_document` 844,
  `endpoint` 16 checks, all OK.
- Guard hits found and fixed (backtraces from the guard):
  1. `shutdown_impl` on the control loop at SIGTERM: `DatabasePool::is_pending` resolves the index
     settings of a shard with a pending revision (`ShardEndpoint::_get_pending_index_settings`,
     `load_settings`, `DatabaseHandler::get_document`, a checkout). Moved to the worker (`4d98274ec`).
  2. `Discovery::db_updated` on the cluster Bus loop (`cluster_check.sh 3`, node3): resolves index
     settings through `SchemasLRU` and `get_metadata`. Dispatched to the worker like PRIMARY_UPDATED
     (`0191530a2`). Pre-existing: the bus loop (Raft and gossip) blocked on that read on master too.
- Docs E2E (`.scratch/e2e.sh`, newman 6, `e2e_assert.py`, `e2e_diff.py` against the X1 capture):
  GREEN/GREEN, 0 ASan, 0 guard hits, "cleanly done" (final binary, `.scratch/e2e-final`).
- `uafhunt.sh` 10 rounds: node alive, 0 ASan, 0 guard hits. `stallcount.sh 0 8972 5`: 0 rounds with a
  timeout; per round 2 FLUSH 200, 253 to 265 writes 204, 55 to 67 deletes 404, nothing else.
- `dump_restore_loop.sh 0 8974 20`: no slow or failed iteration, 0 guard hits. `restore_stress.sh 0
  8973 3`: every RESTORE 200, 0 guard hits; the 18 concurrent PUTs return 400 "Value STR cannot be cast
  to integer" (the restore typed `_id` as integer first) where the pool-fix binary
  (`pool-concurrency/.scratch/fix-bin`) got 12 of 18 PUTs in first (204). Which request types the
  index first is a race in the probe, not a failure; noted for the reviewer.
- `cluster_check.sh 2` GREEN and `3` GREEN three times, 0 guard hits (after `0191530a2`).
- Setup failure (`.xapiand/nodes` chmod 000, solo): this branch and the pool-fix binary both log
  `DatabaseLockError ... Permission denied` and exit 0; the worker's exception reaches `run()`'s catch
  as before.
- Runtime revert checks (`.scratch/revert_checks.sh`, each fix reverted alone, rebuilt):
  C3 FLUSH inline, `uafhunt.sh` 1 round: 4 guard hits in `DatabasePool::cleanup`. C4 no drop:
  `dump_restore_loop.sh`: 1 hit, `~DocIndexer` from `~Request`. C5 work inline on the loop: node
  start aborts in `setup_node_async_cb`.
- TSan: Apple clang 17 builds the pool tests with `-DTSAN=ON` as is (the Homebrew LLVM workarounds in
  the pool-concurrency ledger were not needed): `.scratch/build-tsan`, 3 runs each of `test_pool` and
  `test_blocking_guard`, 0 warnings.

### 2026-10-26, review follow-up (CR1, CR4, CR6)

- Build: same `.scratch/build-asan`, reconfigured (CR6): the binary prints `git:27a9dc463`, the CR1
  commit; the head after it differs only in this ledger.
- Unit tests (ASan): `blocking_guard` 12 checks (was 8), `pool` 21, OK. TSan (`.scratch/build-tsan`),
  3 runs each of `test_pool` and `test_blocking_guard`: 0 warnings.
- Mutants on `test_blocking_guard`: dropping the checkin guard fails 2 checks (both overloads);
  `release_on_pool` destroying inline fails 1.
- Revert check for the move (`.scratch/repl_drop.sh`: node1 holds a 60-doc index, node2 joins and
  replicates, node1 is `kill -9`ed while the replication connection is up): with
  `replication_protocol_service.h` back at the pre-CR1 text, node2 aborts in `DatabasePool::checkin`
  from `lock_shard::unlock` from `~ReplicationProtocolViews` on a loop (6 report lines per run, all
  three rounds); with the move, two rounds dropped the connection: 0 guard hits, 0 ASan.
- Guard armed (checkin included): docs E2E GREEN/GREEN, 0 ASan, 0 guard hits;
  `FAILOVER=1 harness/cluster_check.sh 3`: 0 guard hits, 0 ASan, only the two known failover 503
  checks fail (pre-existing, same as the pool-fix binary per the review).
- `uafhunt.sh` 10 rounds twice and `stallcount.sh 0 _ 5`: node alive, 0 ASan, 0 timeouts, 0 guard
  hits, with `detect_container_overflow=0`. With it on, `uafhunt.sh` aborts at round 8 in prometheus-cpp
  `CKMSQuantiles::insertBatch` (`Summary::Observe`, called from `emit_via_writer`): a libc++
  container-overflow report in the metrics library. The pre-CR1 sources (service header and
  `pool.cc` at `c5c004740`) built in the same tree abort identically twice, so it is not from CR1;
  it did not show in the earlier 10-round runs and its cause is not run down (the pool-fix binary
  runs 10 clean rounds). Worth its own ticket.

## Next

- Lead: apply the "D13 as amended (C1)" text above, remote residual included, to D13 in strata
  (`xapiand-on-strata.md:224-235`; in this repo D13 appears only in reviews and ledgers, so the text
  lives here). CR2 and the reactor repin are in `fix/offload-followups`.
- Follow-ups: the same `release_on_pool` hop for `serve_remote_connection`
  (`remote_protocol_service.h`, not the GPL file) if the remote teardown ever blocks; the prometheus
  ASan report.
- Code review of this branch; the two new sites above are worth the architect's eye (DB_UPDATED now
  queues behind elections on the one worker).
- C7 at 7c.

## Blocked on

Nothing.
