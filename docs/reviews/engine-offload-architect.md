# Xapiand review: the engine seam's offload bridge (engine-core CR1, CR2; pool CR5)

- Date: 2026-10-26. Architect ruling on an escalation; a fresh context.
- Reviewed: local master `2466dac5c` (engine core merged at `566943e9e`, pool concurrency at
  `2466dac5c`): `src/server/search_service.cc`, `src/server/search_views.cc`, `src/database/{shard,pool,handler}.cc`,
  `src/database/cleanup.cc`, `src/manager.cc`, `src/server/discovery.cc`, `src/main.cc`,
  `src/engine/xapian/shard.{h,cc}`, `src/server/{remote,replication}_protocol_service.h`; Kronuz/task at the pin
  `aa447e7` (`task.hh`, `task_asio.hh`); Kronuz/reactor at the pin `e19afd9` (`reactor.h`); Kronuz/http at the pin
  `dc3be77` (`http_asio.h`).
- Checked against: D13 (`strata/docs/design/xapiand-on-strata.md:224-235`), `docs/reviews/engine-core.md` CR1, CR2,
  CR7, CR8, `docs/reviews/pool-concurrency-code.md` CR5 and its re-check, `docs/status/engine-core.md` ("Owed after
  review"), the memory rules "one implementation per mechanism" and "migrate callers, don't forward".
- Checked by: `bridge_probe.cc` (standalone, ASan+UBSan, Apple clang, the pinned task and asio 1.36.0), seven variants
  of the bridge, output `bridge_probe.out`; reading with file:line for every reactor-thread site.
  - Not run: a Xapiand build (none exists on this host for master; the engine-core reviewer's lldb run of the FLUSH
    and control-plane paths is the runtime evidence for those two sites), the server, E2E, `cluster_check.sh`, TSan,
    Linux and FreeBSD, any timing. Nothing under `src/xapian/`, `src/sortable_serialise.{cc,h}` or
    `src/server/remote_protocol_views.cc` was opened; the `.scratch/engine-core-review/` copies of Xapian sources were
    not opened either.

## Verdict

**APPROVE WITH CONDITIONS: master as landed (pool off) stands; the engine offload pool may not default on in any
form until C1 to C6 hold, and what "on" means after them is C7, not the current `Offload`.**

What holds, and reproduces:

- With the pool off, `Hop::await_ready()` is true (`src/engine/xapian/shard.cc:46-49`), so every engine call runs on
  the thread that calls `sync_wait`, exactly where master called Xapian. No new blocking was introduced.
- HTTP, remote and replication requests already run their database work on the serving reactor's offload pool
  (`http_asio.h:779-788` at `dc3be77`; `remote_protocol_service.h:179-186`; `replication_protocol_service.h:179-182`,
  `:237`), and every one of those pools has at least one worker (`src/manager.cc:986-988`, `:1029-1031`,
  `:1044-1046`). That pool is "Xapiand's existing offload pool" in D13's sense.
- The landed pool-on shape does not deadlock today (probe V1: a reactor worker waits on a separate pool; completes),
  as the engine-core review found (CR7).

What blocks default-on: EO2 (database work still runs on reactor loops at four kinds of site), EO4 (the second
pool's hazards and cost); C1 to C6 close them.

## Findings

### EO1. D13 reads two ways, and the landed code took the costlier one

D13 says the Xapian implementation "runs the blocking call on Xapiand's existing offload pool" and, separately,
that until 7c a caller "bridges with a blocking `sync_wait` on the offload thread"
(`xapiand-on-strata.md:228-231`). The second sentence already puts the caller on the offload thread. The landed code
read the first as a second hop: `Offload` (`src/engine/xapian/shard.cc:62-125`) receives the call while the caller,
already an offload worker, blocks in `sync_wait`. Ruling: the existing offload pool is the serving reactor's
`Reactor::pool()`, reached by offloading the caller; the engine call then runs inline on that worker. A hop exists
only for a coroutine caller that runs on a reactor (7c), and it targets that reactor's pool (C7). The lead amends D13
with C1's text.

### EO2. Database work on reactor loop threads: four kinds of site, two found by runtime, two by reading

1. HTTP `FLUSH` runs inline: `SearchService::should_offload` returns false for it (`src/server/search_service.cc:38`),
   and `flush_view` (`src/server/search_views.cc:1506-1530`) calls `DatabasePool::cleanup(true, false)` (`:1521`),
   which closes and commits every open writable (`pool.cc:1103` to `ShardEndpoint::clear`, `pool.cc:481` to
   `Shard::do_close` to `task::sync_wait(wdb->commit(...))`, `src/database/shard.cc:908`). Runtime evidence: the
   engine-core review's ASan stack and lldb trace (CR1 item 1). The comments that justify the inline path
   (`search_service.cc:30-37`, `search_views.cc:1502-1505`) say FLUSH does no database work; that is wrong.
2. The control-plane loop (`Xapiand:MAIN`, one inline reactor with 0 workers, `src/manager.cc:134-153`, run at
   `:447`) runs `setup_node_async_cb` (`:767-`, shard opens, metadata reads, a node document write, and a
   `nanosleep(100 ms)` retry loop), `load_nodes` (`:1568-`, writable open and `replace_document`), `new_leader`
   (`:1613-1631`, calls `load_nodes`) from `dispatch_command_async_cb` (`:1510-1520`), and through the same dispatcher
   `Discovery::_ASYNC_elect_primary` (`src/server/discovery.cc:655-`), which opens a writable shard with
   `lock_shard ... lock(0)` (`:696-697`) and resolves index settings. Runtime evidence for the first two: engine-core
   CR1 item 2; the election path is by reading (new).
3. Request teardown: `http::Request` is a local of the reactor's connection coroutine (`http_asio.h:769` at `dc3be77`;
   the streamed form at `:675`), so `Request::~Request` (`src/server/search_views.cc:2782-2793`) runs on the loop. It
   calls `DocIndexer::finish()`, and when the request holds the last reference, `~DocIndexer` commits
   (`src/database/handler.cc:2111-2121`) on the loop. `restore_database_view` stores the indexer in the request
   (`search_views.cc:1671`, `:1690`) and never clears it. By reading; this settles engine-core CR1 item 3 ("thread
   not determined") as the loop whenever the DI threads have dropped their references first.
4. Everything those three reach without `sync_wait`: `storage_commit()` (`shard.cc:904`), `DatabasePool` checkout
   waits on `writable_cond`, and `cleanup_mtx` (EO3). A guard only at `sync_wait` misses these, hence C2's second
   check point.

### EO3. `cleanup_mtx` makes an inline FLUSH wait for the periodic cleanup's commits

`DatabasePool::cleanup` holds `cleanup_mtx` for its whole walk (`src/database/pool.cc:1091`), across
`ShardEndpoint::clear()` and its commits (`pool.cc:481`, `:502`). The periodic cleanup runs it on its own thread
every 60 s (`src/database/cleanup.cc:64-80`), so a FLUSH that arrives meanwhile waits on the HTTP loop for all of
that walk's commits, and every connection on that reactor waits with it. The lock is right (it closes the CR8
use-after-free); the caller's thread is wrong. C3 moves the wait onto a worker. Residual, accepted: with the default
sizing (`opts.cc:59-61`, `manager.cc:986-988`) a reactor has one or two workers, so a FLUSH behind a long cleanup can
hold one of them and later requests on that reactor queue in its `OffloadGate` (503 past the window); the loop itself
keeps serving.

### EO4. The second pool: no benefit before 7c, two deadlock shapes, one more parked thread per call

`bridge_probe.out`:

| Variant | Shape | Result |
|---|---|---|
| V1 | reactor worker `sync_wait`s, call hops to a separate pool (landed pool-on) | OK, two threads parked per call |
| V2 | a thread of the engine pool `sync_wait`s on work that hops onto the same pool (size 1) | DEADLOCK |
| V3 | reactor worker `sync_wait`s, call hops onto that same reactor's pool (1 worker) | DEADLOCK |
| V4 | reactor worker `sync_wait`s, call runs inline (the pre-7c bridge) | OK |
| V5 | reactor loop `sync_wait`s on a call whose completion needs that loop (strata I/O at S1/7c) | DEADLOCK |
| V6 | reactor coroutine `co_await task::as_awaitable(pool, call)` | OK, resumes on the reactor |
| V7 | `running_in_this_thread()`: in `run()`, on a pool thread, same thread after `run()` | 1, 0, 0 |

V1 against V4: before 7c the second pool changes only which thread runs Xapian while another waits for it. V2 is
the hazard the `Offload` comment states (`shard.h:42-44`) and nothing enforces; today only `finalise_write`
(`shard.cc:104`) runs on a pool thread, and it calls no shard operation (engine-core CR7). V3 is what "reuse the
existing pool" becomes if done as a hop from a waiting worker: it deadlocks once every worker of that reactor waits,
which at one or two workers per reactor is soon. V5 is why the reactor rule is a correctness rule at S1, not only a
latency rule: once strata's executor binds to Xapiand's reactors, a loop that waits for strata waits for itself.
Smaller, unchanged from engine-core CR2: `set_offload` is never reset on the unclean shutdown path
(`src/main.cc:810-817`) and the variable has no bound; C6 removes both by removing the pool.

### EO5. The landed choice of pool in the ledger is superseded

`docs/status/engine-core.md` records "Lead's ruling against D13: replace it with Kronuz/threadpool behind a
task::scheduler". With EO1's reading there is nothing for that pool to do before 7c, and at 7c a threadpool-backed
scheduler would be a fifth set of threads (DP/DI/WL `ThreadPool`s, the reactors' `asio::thread_pool`s, the committer,
strata's merge executor) with the V2 hazard, outside the reactor's admission gate and shutdown abort registry
(`reactor.h:87-100`, `:118-140` at `e19afd9`). The reactor's own pool through `task::asio_scheduler` (`task_asio.hh`)
is the one mechanism. If the lead wants a separately sized pool for Xapian I/O (a concurrency cap), that is a
measured decision for 7c (Q1), not a default.

## Blocking items (for default-on; none for master as landed)

1. EO2: database work leaves every reactor loop (C2 to C5).
2. EO4: `Offload` is retired; the only hop is C7's.

## Conditions

C1. The rule, written into D13 by the lead (`xapiand-on-strata.md:224-235`): no reactor loop thread blocks on
database work: no `sync_wait`, no `DatabasePool` checkout or cleanup, no shard open, commit, or close. A blocking
caller runs on an offload worker (the serving reactor's `pool()`, or an existing named thread: DP, DI, the committer,
WL, the cleanup thread) and the engine call runs inline on it. A coroutine caller on a reactor `co_await`s the task
and never `sync_wait`s (from 7c). No thread waits, by `sync_wait` or otherwise, on a pool it is a thread of.

C2. One bridge and one guard. `task::sync_wait` appears in `src/` only inside one engine wrapper (CI check:
`git grep -n 'task::sync_wait' src` lists only that file). The wrapper and the entry points of
`DatabasePool::checkout` and `DatabasePool::cleanup` check two things before blocking: the thread is not inside a
reactor's `run()`, and `task::current()` is empty (not a scheduler's own thread; `sync_wait` clears it for the
awaited task, `task.hh:509`, so the check reads the caller's state). The reactor test is dynamic, not a thread tag
(V7: `XapiandManager::run` reuses the loop thread for `join()` after `run_loop()`, `manager.cc:716-724`): add a
thread-local marker to the reactor library, set around every `io().run()` the library drives and by a RAII scope
the host puts around its own `run()` (`manager.cc:447`); a generic change, so it lands in Kronuz/reactor with its own
test. A violation asserts under Xapiand's `ASSERTS` (Debug and sanitizer builds); release builds count it and log
once per site. Test: the guard fires inside an `io_context` handler and not on a pool thread or after `run()`
returns (V7's shape), and fails with the check removed.

C3. HTTP `FLUSH` is offloaded: drop it from the inline list (`search_service.cc:38`) and correct both comments.
OPTIONS, OPEN, CLOSE and QUIT stay inline (no database work; `quit_view` only signals, `search_views.cc:1533-1546`).
Test: `hammer.sh` with FLUSH from every client and the docs E2E, both with the guard armed: zero guard hits.

C4. No `DocIndexer` dies on a loop: the restore view drops `request.indexer` on its worker on every exit (a scope
guard around `search_views.cc:1657-1712`), so the last reference falls on the worker or a DI thread; or
`~DocIndexer` hands its commit to the committer instead of committing inline. Test: `restore_stress.sh` and
`dump_restore_loop.sh` (engine-core probes) with the guard armed: zero hits.

C5. The control plane's database work leaves `Xapiand:MAIN` through the reactor library's own offload pool: the
control-plane `Reactor` gets one worker (`manager.cc:153`, now `reactor(0, 0)`), so `setup_node`, `load_nodes`,
`new_leader` and the election's shard lookup stay serialized in arrival order; the loop posts the database part and
resumes the state change (servers, ready state, replication triggers) on the loop. A setup failure keeps its exit
path (the exception is carried back and rethrown on the loop), and shutdown joins that worker after `break_loop()`
and before the pool's `finish`. Test: the solo E2E and `cluster_check.sh 2` and `3` with the guard armed (zero hits),
and a setup failure (primary not available) exits with master's code.

C6. Retire `engine::xapian::Offload`, `set_offload` and `XAPIAND_ENGINE_OFFLOAD_THREADS`
(`src/engine/xapian/shard.{h,cc}`, `src/main.cc:810-817`) once C2 to C5 land; `Hop` takes a caller-chosen
`task::scheduler_ref`, empty meaning inline, and `test_engine_shard`'s pool case moves to that. This closes engine-core
CR2 and its lifetime and bound points by removal. Update `docs/status/engine-core.md`'s "Owed" CR2 line to this
ruling.

C7. What "default on" means, from 7c: a coroutine caller on a reactor hops to that reactor's `pool()` through
`task::asio_scheduler` (turn `TASK_ASIO` on, `CMakeLists.txt:731`) and returns through `task::as_awaitable` (V6). A
worker caller never hops. Only `post` is used on an `asio_scheduler` over a multi-threaded pool (its demoted queue is
unsynchronized, "all calls happen on the executor's thread except post()", `task_asio.hh` at `aa447e7`). Work posted
this way enters through the reactor's `OffloadGate` like a request. Before it ships: a test that a reactor coroutine's
engine call runs on a pool thread and resumes on the loop while the loop serves another handler (V6's shape on the
real reactor), and V2, V3 and V5 as negative tests that the guard reports.

## Design questions (for the lead or owner)

Q1. Should Xapian I/O have a concurrency cap of its own at 7c (the one thing a separate pool would buy)? Recommend no
until a measurement shows the reactor pools' sizing (`opts.cc:55-61`) is the wrong cap.

## Probes and files kept (`.scratch/review/engine-offload/`)

`bridge_probe.cc` (V1 to V7, a copy of the landed `Offload`), `build.sh` (ASan+UBSan build; prints the task commit),
`bridge_probe` (binary), `bridge_probe.out` (the table above).

Not run: Xapiand build, server, E2E, cluster, hammer, TSan, Linux and FreeBSD, timing; the GPL files and the Xapian
copies under `.scratch/engine-core-review/`.
