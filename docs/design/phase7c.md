> Status: DRAFT (2026-10-29) Revision 1, designing (ready for preliminary review). Ledger:
> `docs/status/phase7c.md`.

# Phase 7c: Xapiand that never blocks a reactor

Reads Xapiand master `3b0dc433f`; strata `d93bf7d` and its `design/write-modes` branch (`ae2cf70`,
`docs/design/write-buffer.md`); prospect `b3ad3e3` (Xapiand pins `14dbbbb`); task `aa447e7`; reactor
`0c04d0d`; http `6a07f99` (Xapiand pins `dc3be77`); completion `03cef1d`; journal `9c87929`; threadpool
`main` `394aaea` and `feature/scheduler` `5caedce`; Detent `main`. Clean room: nothing under
`src/xapian/`, no `sortable_serialise` file and not `src/server/remote_protocol_views.cc` was opened;
the replication rows below come from `replication_protocol_{service,views}.{h,cc}` (MIT).

Phase 7's single-node gate is 7a (passed, `docs/gates/phase7a.md`) plus 7c (strata `docs/PLAN.md:651-674`,
owner, 2026-10-29). 7c's scope: Xapiand's blocking primitives become asynchronous (pool checkout,
storage reads, fsyncs, schema locks, Lua) and the offload pools are removed. This design covers the
strata path only; the Xapian path goes at 7c's start or in Phase 8 (Q1) and the offload bridges are replaced, so neither is
preserved or tuned.

## Summary

1. Every request runs as a `task::task` on the reactor that accepted it, sliced by a cycle-counter
   budget and scheduled least-attained-service by Kronuz/task's `asio_scheduler` (D2, D3). Nothing on a
   reactor waits: no condition variable, no lock held across I/O, no sleep, no file syscall (D1).
2. The one place work leaves a reactor is an **index chore** on one host `WorkPool` (Kronuz/threadpool
   `feature/scheduler`): open, flush, commit, close, merge, reclaim, check, and document parse-ahead,
   per index in its own `WorkGroup`, awaited by the request and resumed on its reactor (D4). Fsyncs run
   only there (D7).
3. `DatabasePool` checkout becomes an index registry that never waits plus an awaitable per-index
   write lease; reads use strata's `reader_handle`, which never blocks (D5, D6).
4. The reactors' offload pools, the control-plane worker, and the DP, DI, AC and cleanup threads are
   deleted with their options, no switch kept (D4, D13, D15). Lua runs in a yielding Lua coroutine
   (D10); schema and settings caches hold no lock across I/O (D11).
5. Step 1 builds the gate tooling (a reactor lag probe, the mixed-load driver, a thread census, a
   static check, a light-request profile) so the lead measures the pre-7c offload baseline before
   anything changes. It needs no owner answer.

## What this depends on and changes

| Document | Relation |
|---|---|
| strata `docs/PLAN.md` Phase 6 (`:640-649`), Phase 7 (`:651-674`) | Gates 7c with Phase 6's reactor rows (T4, T5, T6); the light-query baseline Phase 0 owed (strata `docs/gates/phase-1.md:60`, "Open") is measured by step 1 |
| strata `docs/ARCHITECTURE.md:163-185` ("Concurrency and async") | Implements its scheduler, budget and guarantee on Xapiand's reactors |
| strata `write-buffer.md` (`design/write-modes` `ae2cf70`) D2 (`:197`), D8 (`:621`), D11 (`:765`), D12 (`:797`) | Depends: checkout never waits, flush on a pool thread, coalesced freeze for search, writes as tasks. 7c runs before steps 2 to 4 land with host-side stand-ins that those steps delete (D5, D6) |
| strata `xapiand-on-strata.md` D7 (`:210`), D11 (`:233`), D13 (`:242-264`) | D7 kept (read-your-writes through an awaited, coalesced refresh). D13's bridge (`sync_wait` on an offload thread) retires; its blocking rule becomes D1 |
| `docs/reviews/engine-offload-architect.md` C7 | Changed: no coroutine hops to the reactor's `pool()`; the only hop is D4's chore into the index's `WorkGroup`. Q1 (a separate Xapian I/O cap) is moot with Xapian gone |
| `docs/status/offload-bridge.md` | The guard (`engine::check_may_block`) and its tests are kept and widened (D14); C7 lands here |
| threadpool `docs/research/scheduler.md` R1 to R3 (`:673-728`), API sketch (`:730-790`) | Depends: `WorkPool`, `WorkGroup`, classes and reserve. Changes its "an offload is a `WorkItem`" (`:786-789`): no request offload remains |
| `docs/gates/phase7a.md` T6, T8, T11 | Baselines for T2, T3, T12 |
| Detent `src/detentd/detentd.cc:125`, `src/detentd/main.cc:278-284`, `src/detent_relay/host.cc:243` | Detent uses http's offload; Q3 decides whether the libraries drop it now |

## 1. Inventory: blocking primitives on reactor-reachable paths

After 7c every HTTP view runs on its reactor, so everything a view reaches is reactor-reachable:
`search_views.cc`, `DatabaseHandler`, `Shard`, the strata adapter, schemas, the index resolver, Lua,
and the control loop. Counts are greps on `3b0dc433f` (rerun: `harness/async_check.sh`, step 1, K5).

**A. Engine bridges, 33 sites.** `engine::sync_wait` (`src/engine/sync.h:44-48`) at `handler.cc:271, 772,
814, 1178, 1425, 1727, 1759` (7) and `database/shard.cc:438, 446, 459, 552, 568, 600, 716, 973, 1034,
1052, 1070, 1105, 1171, 1339, 1449, 1542, 1553, 1555, 1559, 1628, 1691, 1745, 1778, 1821, 1852, 1894`
(26; `:438`, `:552`, `:568` are Xapian-only). CLI-only callers in `main.cc:836`, `:872` reach the same
handler.

**B. Pool checkout, 36 sites.** Seven condition-variable waits in `pool.cc` (`:191`, `:198` writable;
`:295`, `:302` readable; `:816`, `:820` lockable; `:1088` checkin-clears). `DatabasePool::cleanup` holds
`cleanup_mtx` across every commit it runs (`pool.cc:1103`, review EO3). Callers on strata paths: 25
`lock_shard` in `handler.cc` (`:766` to `:2524`), the multi-endpoint checkout and checkins
(`handler.cc:1160`, `:1208`, `:1211`), `schema.cc:2778`, `:2844`, `discovery.cc:710`; 29 in all. Six
replication sites are Xapian-only (`replication_protocol_service.cc:143`, `:160`, `:173`,
`replication_protocol_views.cc:516`, `:682`, `:986`).

**C. The strata adapter and strata itself, 41 adapter sites.** `engine/strata/shard.cc`: 27
`core.mtx` lock sites; `registry_mutex` (`:274-280`); six refresh-before-read sites that may write a
segment (`:395`, `:987`, `:1057`, `:1142`, `:1196`, `:1234`, through `:225-228`); four commits that
fsync (`:191` on close, `:832`, `:854`, `:876`, through `:232-239`); three opens that read files
(`:215`, `:348`, `:353`). A search pins every shard it reads, holding each shard's mutex for the
whole search (`pin.h:33-55`, `shard.cc:1189-1205`, `search.cc:632-650`). Inside strata, on the caller's
thread: `apply` flushes the buffer inline when it is full (`index.hh:691`); with no host executor,
`maybe_merge` runs merges inline (`index.hh:845-858`; Xapiand passes none, `shard.cc:162-169`); a lease
taker waits on a check-in's chores (`owner.hh:120-123`).

**D. Storage and fsync, 16 sites.** Blob reads from `DataStorage` volumes (`shard.cc:1208-1227`,
reached from `handler.cc:717` and `:1793-1803`); blob writes (`shard.cc:1317`, `:1426`, `:1515` into
`:1230-1280`); the volume commit fsync (`shard.cc:969`, `:1283-1290`, `STORAGE_FULL_SYNC` at `:174`).
Nine WAL appends (`shard.cc:993, 1115, 1181, 1354, 1464, 1575, 1638, 1701, 1904`) queue to the WL
threads (`wal.cc:942-951`), or write and sync inline when the WAL is synchronous (`wal.cc:932-938`,
replica only); the WAL's fsync goes to the FS debouncer (`wal.cc:116`, `:131`).

**E. Locks and waits in request code, 42 sites.** `schemas_lru.cc`: 19 lock sites and four nested
cross-index database calls (`:110`, `:163`, `:277`, `:338`). `index_resolver_lru.cc`: two locks
(`:579`, `:868`) and four database calls (`:252`, `:299`, `:399`, `:436`). Lua (`lua/processor.cc`): the
compile cache lock (`:190`), the per-script lock held for the whole script run (`:304` across `:318`),
and a foreign-script document read (`:247-251`). `DocIndexer::wait` blocks the request thread on three
condition-variable waits (`handler.cc:2199`, `:2210`, `:2214`); DI threads wait on their queues
(`:2059`). The streamed restore body: five `objects_mtx` sites (`search_views.cc:2870-2961`) and the
blocking `body_reader->read` (`:2957`).

**F. Sleeps, 1 site.** `manager.cc:834`, a 100 ms retry in node setup. (`main.cc:807` runs before any
loop.)

**G. Unbounded CPU on one request, 5 kinds.** prospect's search loops yield nowhere
(`prospect/shards.hh:237`, `:279`); the host callbacks inside them (sort keys, spies, collapse,
`engine/strata/search.cc:689-711`); a Lua script, with no hook or limit (`processor.cc:318`); scans for
dumps (`handler.cc:772`, `:814`); decoding a buffered request body, which has no size cap (only a
reserve cap, http `http_request_parser.h:170`, `:240`).

**H. Threads and hops that run blocking work for someone else, 11 kinds.** The HTTP reactors' `asio::thread_pool`s
(`manager.cc:1059`, `:1066`; `ceil(1.5 x CPUs)` threads, `opts.cc:59-61`); the control-plane worker
(`manager.cc:156`, `control_offload` `:467-488`, used at `:643`, `:825`, `:1576`); the remote and
replication pools (`remote_protocol_service.h:179-186`, `replication_protocol_service.h:180-182`,
`:245`), Xapian-only; `engine::release_on_pool` (`release.h:40-55`); DP and DI (`manager.cc:240-241`);
AC committers (`handler.h:335`); FS fsynchers (`xapiand_fsyncher.h:52`); WL writers (`manager.cc:236`,
`wal.h:305`); `DatabaseCleanup` (`cleanup.h:41`, `cleanup.cc:80`). Cluster-only and 7b's: TR
(`manager.h:334`) and DU, SU, IU, PU (`discovery.h:194-218`). In the libraries: http's offload and
streamed-body worker (`http_asio.h:633-719`, `:780-790`), `HttpHandler::handle` being synchronous
(`http_handler.h:190`), and `Reactor::pool_` with `OffloadGate` (`reactor.h:99-129`, `:167-168`).

**I. Destructors that reach database work, 3.** `~DocIndexer` commits (`handler.cc:1920`); `~Request`
drops the last indexer (`search_views.cc:2719`); a strata core commits when its last writer drops
(`engine/strata/shard.cc:186-196`).

Totals: 169 call sites (A 33, B 36, C 41, D 16, E 42, F 1), 5 kinds of unbounded CPU, 11 kinds of
blocking-work mechanisms (8 on the single-node path), 3 destructors.

## 2. The end state

```
 HTTP reactor r (one per core)                    WorkPool (one per host; R1-R3)
  ├ asio_scheduler + budget: request tasks  ──chore──▶ WorkGroup per open index
  ├ index registry lookup (never waits)     ◀─resume─┤  open, flush, commit, close,
  ├ lease turns: apply, get (inline)                 │  merge, reclaim, check, parse-ahead
  └ lag probe                                        └ strata's executor binds here
 control loop: setup, timers (autocommit, cleanup), shutdown, all coroutines
```

Threads left on the single-node path: the HTTP loops, the control loop, the `WorkPool` workers, the
logger thread, the debouncer wheel thread, completion's bounded fallback worker for operations a
kernel cannot run asynchronously (completion `ARCHITECTURE.md:3`), and WL and FS while Q2's (a)
holds.

## 3. Decisions

**D1. The rule.** No reactor thread waits. A reactor never calls `sync_wait`, waits on a condition
variable, sleeps, issues a file syscall, or takes a lock whose critical section does I/O, waits, or
runs unbounded work. A lock taken on a reactor guards O(1) to O(log n) work on in-memory maps, is listed
in `harness/async_check.sh`'s allowlist with its file:line, and its hold time falls under T4. Work that
must wait suspends its coroutine. The rule replaces D13's bridge rule
(`xapiand-on-strata.md:255-264`) on landing.

**D2. One scheduler per reactor; requests are tasks with a budget.** Each HTTP reactor and the control
loop own a `task::asio_scheduler` on their `io_context` (`task_asio.hh:43-83`; `TASK_ASIO` turns on,
`CMakeLists.txt:773`). A request is a `task::task<void>` with a `task::budget` (slice 200 µs, an estimate
T4 tunes; strata ARCHITECTURE's 100-250 µs) and the connection's `cancel_token`, so a client that goes
away ends its search at the next checkpoint (T9). A spent slice demotes the task (least-attained
service: light requests run ahead of demoted heavy ones). Each reactor caps its demoted tasks; at the
cap a new request gets 503 instead of a queue (T8). Kronuz/task gains the demoted count the cap reads
(`asio_scheduler::demoted()`); nothing else in it changes.

**D3. HTTP handlers are coroutines.** Kronuz/http's `HttpHandler::handle` (`http_handler.h:190`)
becomes `asio::awaitable<void> handle(const Request&, ResponseWriter&)`, awaited on the connection's
own executor where `invoke_handler` is awaited today (`http_asio.h:611-613`, `:780-790`); the request and
writer stay valid until it completes. A streamed body is read with `co_await body.read(chunk)` on the
same loop instead of a worker pulling from `body_reader` (`http_asio.h:633-719`). `SearchService`
(`search_service.h`) wraps each view's task in `task::as_awaitable(scheduler, ...)`, the one bridge
from Asio to task. Xapiand constructs the service with zero workers and deletes `should_offload`
(`search_service.cc:28-37`); prism's demo handler migrates (`prism/src/main.cc:182`). Whether the
libraries drop the offload path itself is Q3.

**D4. Index chores: the only work that leaves a reactor.** A chore is index work that may wait on a
device or run long: open, refresh (flush), commit, close, merge publication, reclaim, check, and
parse-ahead (decoding and preparing a large document or a RESTORE batch, a function of its inputs that
does no I/O and takes no lock). Chores run on one host `WorkPool` (threadpool `feature/scheduler`:
futex-parked workers, R1; per-group deficit round robin, R2; classes with a reserve, R3), each open
index owning a `WorkGroup` (128 bytes, research `:688-708`). `co_await chore(index, cls, fn)` posts a
`WorkItem` into the index's group and resumes the awaiter on its own reactor (Kronuz/task's affinity
rule). Classes: `flush` for open, refresh, commit, close, check and parse-ahead; `merge` for merges and
reclaim. strata's executor binds to the same pool (`include/strata/core/executor.hh`'s `group` over a
`WorkGroup`, the research's `work_executor.hh`), so merges and reclaim deletes leave the commit path.
A chore that waits for its own index's background work (a close waits for posted deletes,
`executor.hh:27-33`) waits in `WorkGroup::wait`, which runs queued items when called on a worker
(research `:762`, `:772`), so no worker waits on work only it could run (review EO4's V2 shape).
Request logic (routing, schema mapping of small documents, response encoding) never runs in a chore.
Deleted: the HTTP reactors' pools, `engine::release_on_pool`, DP, DI, the AC pool and the cleanup
thread; there is no thread setting (PLAN, "Parallelism adapts").

**D5. Checkout never waits.** `DatabasePool` becomes an index registry: a map from path to an open
index entry (strata reader handle, lease, `WorkGroup`, revision, idle time). A miss single-flights one
open chore; concurrent requests for the same path await one `task::event` (new in Kronuz/task: a
one-shot completion that resumes each waiter on its own scheduler, the waiter's node in its frame).
Readers take no checkout: they use the entry's `reader_handle`, which never blocks
(strata `index.hh:1245-1295`). Lease operations (apply, refresh, commit, close, merge publication) are
turns of a per-index `task::async_mutex` (new in Kronuz/task: FIFO, no thread waits, the waiter's node in
its frame): cheap turns run inline on the caller's reactor, flush and commit turns run as chores holding
the lease. This is strata's write-buffer D2 with one writer per turn; when write-buffer step 2 lands
(write groups, `:988`), writers link into strata's group and the host mutex is deleted. Idle indexes close
through a control-loop timer that posts close chores (replaces `cleanup.cc:64-80`); `FLUSH` awaits the
close chores of every open index. Deleted: the seven condition-variable waits, readable pooling and
`max_database_readers`, `lockable_cond` (replication only), `cleanup_mtx`.

**D6. The strata adapter holds no lock.** `core.mtx`, `registry_mutex` (moved to D5's registry),
`Pinned` (`pin.h`), the shard ordering in `search.cc:632-650`, and `Hop` with every `scheduler_ref on`
parameter (`engine/strata/shard.cc:62-68`, `engine/open.h:43`, `:52`) are deleted. A search takes one
view guard per shard (`reader_handle::acquire`, never blocks) after awaiting `publish_through(p)` as a
chore when the shard's view is behind the request's read position, coalesced so the searches queued
behind one refresh reuse it (`index.hh:1312-1318`). That keeps 7a's D7 (reads see every acknowledged
write) without a reactor flushing. Until write-buffer D11 lets prospect read frozen buffers in memory,
that refresh writes a segment; Q6 asks whether the default stays read-your-writes. Writes apply in a
lease turn on the reactor; when strata reports the buffer must flush first (the `index.hh:691`
condition, exposed as `flush_owed(const transaction&)`, a strata change), the turn runs the flush as a
chore before applying. Write-buffer D8's background flush deletes that step when it lands.

**D7. Fsyncs and durability.** Every fsync, `F_FULLFSYNC` and directory barrier runs inside a chore
(strata commits through completion's `PosixIO::sync_files`). `?commit=true`, `COMMIT` and `FLUSH` await
their commit chores. Autocommit keeps its throttle and debounce values (`handler.h:335`), on Kronuz/
scheduler's inline-runner debouncer: the wheel thread fires and only posts a commit chore, so the AC
pool goes. A commit that fails returns the error to every request awaiting it.

**D8. Xapiand's WAL is untouched until 7b** (Q2, recommendation (a)). Its appends already queue to WL
without waiting (`wal.cc:942-951`) and its fsync is the FS debouncer's; neither is an offload pool and no
reactor waits on them. The synchronous mode is the replica's (`replication_protocol_views.cc:986`) and
goes with replication (Q1). 7b replaces both with the journal.

**D9. Blobs live in strata's stored record** (Q4, recommendation (a)). A `stored` locator's bytes are
kept in-place in the document's stored record (strata's cap is 64 MiB a record, `limits.md:23`), so a
blob read is a strata read under D6 and its I/O rules (Q5). `DataStorage` (`shard.cc:191-342`,
`:1206-1291`) leaves the strata path; with Q1's (a) it is deleted.

**D10. Lua yields.** Each reactor keeps its own compiled-script cache (thread-local; the locks at
`processor.cc:155`, `:190`, `:304` go). A call runs in a Lua coroutine (`lua_newthread`) with a count hook
(`lua_sethook`, `LUA_MASKCOUNT`, every 10,000 instructions, an estimate) that calls `lua_yield(L, 0)` when
the request's slice is spent, which Lua 5.4 allows from count hooks (pinned 5.4.7, `CMakeLists.txt:1100`);
the task awaits `budget.checkpoint()` and calls `lua_resume`. An instruction cap (10^8, an estimate) and
an allocation cap through `lua_Alloc` end a runaway script with a 4xx (T10). Foreign scripts load through
an awaited handler read (`processor.cc:247-251`). Scripts run wherever the document is prepared: on the
reactor, or in a parse-ahead chore (D12).

**D11. Schema and index-settings caches hold no lock across I/O.** The nested cross-index reads and
writes (`schemas_lru.cc:110`, `:163`, `:277`, `:338`; `index_resolver_lru.cc:252`, `:299`, `:399`, `:436`)
become awaited handler operations. A cache miss single-flights one load per path (`task::event`), and the
waiters reuse its result. The maps' own critical sections (`schemas_mtx`, `versions_mtx`,
`resolve_index_lru_mtx`) do no I/O and are allowlisted under D1; updates keep their version
compare-and-swap.

**D12. CPU on a reactor is sliced.** prospect's search becomes a task that awaits
`budget.checkpoint()` at every window boundary and every 4,096 ordinals of the exhaustive loop
(`shards.hh:237`), a prospect change; host callbacks run inside those slices. Scans for dumps await a
checkpoint every 64 documents. A buffered body gets a cap (16 MiB by default, an estimate; strata's
record cap is 64 MiB) enforced by http before decoding. Documents larger than 64 KiB (an estimate) and
RESTORE batches are prepared in parse-ahead chores, so one bulk load still uses every idle core, as DP
and DI did; smaller single documents are prepared inline.

**D13. The control plane is coroutines on its loop.** `control_offload` and the control reactor's worker
are deleted (`manager.cc:156`, `:467-488`, `:1227-1230`). Node setup, `load_nodes`, `new_leader`, the
election's shard lookup and the shutdown readiness checks are tasks on the control loop's scheduler that
await handler operations; their order is the loop's arrival order, which the one worker gave before. The
setup retry awaits a timer instead of sleeping (`manager.cc:834`). The bounded command drain stays
(`engine/drain.h`).

**D14. Enforcement.** `engine::check_may_block` stays (`engine/sync.cc:48-71`) and guards every
remaining blocking entry: the CLI's `sync_wait` (the only one left, `main.cc`), `WorkGroup::wait`, and the
registry's close-all at shutdown. Kronuz/reactor gains `reactor::LagProbe`: a periodic timer per loop that
records its own lateness in a fixed log2 histogram, allocation-free after start; Xapiand exports it per
loop in `:metrics`. No destructor does database work: `~DocIndexer`, `~Request` and the strata core's
closing commit become explicit awaited closes, and their destructors assert the close ran.
`harness/async_check.sh --strict` runs in CI.

**D15. Removed options.** `--http-clients`, `--bulk-preparers`, `--bulk-indexers`, `--committers`,
`--fsynchers` (`opts.cc:237-288`) go, with a changelog bullet; `--writers` stays while D8 holds.

Extraction ledger rows owed (`docs/dev/extraction-ledger.md`): `task::event`, `task::async_mutex` and the
demoted count (Kronuz/task), `LagProbe` (Kronuz/reactor), the coroutine handler and awaitable body
(Kronuz/http), `WorkPool` (Kronuz/threadpool); in Xapiand, the deleted pools, `control_offload`,
`release_on_pool`, the cleanup thread and `DataStorage`. How-it-works notes owed on landing: the request
scheduler (D2), index chores (D4), the registry and lease (D5), Lua slicing (D10).

## 4. Gate rows

Timing rows (T4, T5, T6, T11, T12) are the lead's, on `main`, five interleaved trials with the 95%
bootstrap interval (strata PLAN, "Passing"), smoke then confirm under `quiet exclusive`; branches make
functional claims only. Heavy load: concurrent exhaustive searches with aggregations, wildcard and phrase
searches, and a RESTORE stream, at 4x the reactor count in clients. Light load: open-loop GET by id and
single-term searches at a fixed rate. Index: `accounts` replicated to 1,000,000 documents
(`loadtest.py --replicate`). Each run records its phase times.

| Row | Target | Procedure |
|---|---|---|
| **T1** no offload pool | The thread census under T4's load lists only section 2's threads: none of inventory H on the single-node path except WL and FS while Q2's (a) holds | `harness/thread_census.sh` during `stall_load.py`; `harness/async_check.sh --strict` exits 0 |
| **T2** E2E on strata | Zero assertion regressions against gate 7a's rerun (566 of 566, `docs/gates/phase7a.md`); 0 guard hits, 0 ASan reports | 7a's T6 procedure (`e2e_capture.sh`, `e2e_diff.py`) on a Debug ASan build with the guard armed |
| **T3** recovery | `wal_recovery_check.sh` and `engine_crash_check.sh` pass on strata | Run as written, `XAPIAND_ENGINE=strata` |
| **T4** reactor stall | p99 lag ≤ 1 ms on every loop under the mixed heavy and light load (PLAN Phase 6, `:645`); p99.9 and max reported | `harness/stall_load.py --phase loaded`; the lag histograms' deltas from `:metrics` |
| **T5** light-query p99 | Under heavy load ≤ 2x its unloaded p99 + 1 ms (PLAN `:645-647`), and below the pre-7c offload baseline step 1 measures | `stall_load.py`, phases `unloaded` then `loaded`, same binary and index |
| **T6** reactor faults | Major page faults on reactor threads per 1,000 queries ≤ 1, index at 2x RAM (PLAN `:647-648`); gating per Q5 | Linux gate VM, per-thread `getrusage(RUSAGE_THREAD)` sampled by the lag probe's loops, `stall_load.py --index-size 2xram` |
| **T7** concurrency | 0 TSan reports; the final dump equals the model | `engine_mixed_load.py BIN OUT --engine strata --secs 600` on a TSan build, and on an ASan build |
| **T8** overload | Past the demoted cap new requests get 503 within one slice; no request waits unbounded; RSS stays within 10% of the T4 run (an estimate to confirm) | `stall_load.py --overload 4` (four times the saturating light rate) |
| **T9** cancellation | A client that disconnects mid-search frees its reactor within one slice; the cancel counter rises | Functional test: a heavy search, disconnect after 50 ms, lag histogram and counter checked |
| **T10** Lua bounds | An infinite-loop script returns 4xx at the instruction cap; T4 holds with four such scripts running | Unit test plus `stall_load.py --lua-loop 4` |
| **T11** replaced layer wins by a lot | The dispatch cost per light request (pre-7c: the offload hop's post, wake, run and return; 7c: the scheduler's post and resume) is at most half of the offload hop's, in CPU and wall; parity fails the row (owner, 2026-10-27) | `harness/profile_light.sh` on both binaries, frames attributed by name at concurrency 1 |
| **T12** no throughput loss | One RESTORE's docs/s (`profile_bulk.sh`, 20,000 accounts) and saturated query QPS (`loadtest.py`) each at least 1.0x the pre-7c `main` | Interleaved runs of both binaries |
| **T13** every gate | ASan and UBSan, TSan; coverage of changed files at TESTING.md's targets; the restore decoders' corpora replayed; per-open-index cost (registry entry, lease, `WorkGroup`) measured at 10,000 open indexes and stated | strata PLAN, "Every gate"; `small-indexes.md`'s open-index rows |

## 5. Steps

```
 S1 tooling + pre-7c baseline ───────────────────────────────────────────────┐
 L1 task: event, async_mutex, demoted() ─┐                                   │
 L2 threadpool: land WorkPool ───────────┼─> X2 registry, lease, chores ─> X3 HTTP on reactor ─> X6 deletions ─> X7 gate
 L3 http: coroutine handler, body cap ───┘        ▲          ▲     X4 Lua ──┤
 L4 prospect: sliced search ──────────────────────┼──────────┘     X5 schema, resolver, control ┘
 L5 strata: flush_owed, executor binding ─────────┘
 X1 delete the Xapian path (Q1) ─> X2
```

1. **S1, tooling and baseline (step 1, below).** No owner answer needed.
2. **L1 to L5, the library pieces**, each on its own branch in its own repository, parallel with S1:
   L1 `task::event`, `task::async_mutex`, `asio_scheduler::demoted()`; L2 finish `feature/scheduler`
   (its ledger's open items: sanitizers and CI on the rebased branch, K1 to K3 reruns, consumer builds);
   L3 the coroutine `handle`, the awaitable body, the buffered-body cap (D3, D12); L4 prospect's search as
   a task with checkpoints (D12); L5 strata's `flush_owed` and the `WorkPool` executor binding (D4, D6).
3. **X1** (after Q1): delete the Xapian engine, the remote and replication services, and `DataStorage`.
4. **X2**: `DatabaseHandler` and `Shard` operations become tasks; the registry and lease (D5); the
   adapter without locks (D6); chores (D4, D7). `sync_wait` leaves every server path.
5. **X3**: views as tasks on the reactors (D2, D3); `FLUSH`, `RESTORE` with parse-ahead (D12).
6. **X4** Lua (D10); **X5** schema, resolver and control plane (D11, D13).
7. **X6**: the deletions (D4, D13, D15), destructor asserts (D14), `async_check.sh --strict` in CI.
8. **X7**: the lead runs T1 to T13 on `main`.

**Step 1 (S1).** Branch `phase7c/tooling` in `Xapiand-wt/phase7c-tooling`, plus `feat/lag-probe` in
Kronuz/reactor. Done criteria, for the lead to set:

- K1 Kronuz/reactor: `reactor::LagProbe` per loop (interval 1 ms), log2 buckets in µs, no allocation
  after start, a `snapshot()`; a test where a handler busy-waits 5 ms lands in the ≥ 4,096 µs bucket, and
  fails with rearming removed; ASan, UBSan and TSan clean; CI green on every platform.
- K2 Xapiand: every HTTP loop and the control loop run a probe; `:metrics` exports
  `xapiand_reactor_lag_seconds` per loop; the reactor pin moves; docs E2E on strata 566 of 566, 0
  regressions.
- K3 `harness/stall_load.py`: phases `unloaded` and `loaded`, the heavy and light loads above, options
  `--overload`, `--lua-loop` (later steps fill them) and `--index-size`; prints and asserts its effective
  configuration and non-zero counts; writes light p50, p99 and p99.9 per phase, lag deltas per loop,
  503 and error counts, and phase times as JSON; a 20,000-document dry run gives non-empty results.
- K4 `harness/thread_census.sh`: thread names and counts by prefix for a running node (macOS `ps -M`,
  Linux `/proc/PID/task/*/comm`); its dry run lists today's pools.
- K5 `harness/async_check.sh`: section 1's greps as rules with a file:line allowlist; by default it
  prints counts that match section 1's totals on `3b0dc433f`; `--strict` exits non-zero on any hit
  outside the allowlist.
- K6 `harness/profile_light.sh`: samples a node at concurrency 1 on the light mix and attributes CPU
  and wall per request to named frame groups (offload hop, scheduler, engine, HTTP), like
  `profile_bulk.sh`; dry run on `3b0dc433f`.
- K7 The ledger names the commands the lead runs for the pre-7c baseline (T5, T11, T12); the developer
  makes no timing claim.

## 6. Open questions for the owner

**Q1. When does the Xapian path go?** 7c removes the pools Xapian's synchronous calls need. (a) Delete
the Xapian engine, remote protocol, replication and `DataStorage` at 7c's start (X1), moving Phase 8's
code removal first; Phase 8's closing benchmark compares against the final Xapian-only release built from
the `xapiand` branch, which is what users upgrade from. (b) Keep Xapian until Phase 8 behind a private
engine-internal thread, an interim bridge Phase 8 deletes. (c) Keep it on the reactor pools; the "no
offload pool" row then holds only for a strata-only build. Recommendation: (a). It blocks X1 and X2's
scope, not S1 or L1 to L5.

**Q2. Xapiand's WAL in the single-node release.** (a) Unchanged until 7b: WL and FS stay as background
writers no reactor waits on (D8). (b) Pull `kronuz::journal` forward as each strata index's log at
`rf = 1`, moving 7b's "the journal replaces the WAL" into 7c with its own design. (c) Move WAL writes and
fsyncs into chores. Recommendation: (a): it touches no code 7b deletes and passes the gate as written.
Blocks nothing before X6.

**Q3. Do the libraries drop their offload path now?** Detent's distributor uses http's `should_offload`
and workers (`detentd.cc:125`, `main.cc:278-284`, `host.cc:243`). (a) Xapiand stops using it in 7c; the
library removal is a separate change coordinated with Detent's agent. (b) Remove it from http and
reactor in 7c and migrate Detent in the same series. Recommendation: (a). Blocks L3's scope only.

**Q4. Where do blobs live?** (a) In-place in strata's stored record (D9), up to 64 MiB, merges rewriting
them; the `stored` type of the store API (`docs/src/content/docs/reference-guide/documents/store-api.md:268`)
is removed under R-RM or kept as a label. (b) Keep `DataStorage` with its I/O in chores. (c) A strata
large-value section (format work). Recommendation: (a), removing the `stored` type. Blocks X1's
`DataStorage` deletion.

**Q5. Does T6 (faults at 2x RAM) gate 7c?** Passing it needs residency prefetch from each query plan and
the asynchronous residency backends (residency ships only `madvise`), which is Phase 6's work. (a) Gate it
at 7c, pulling that in. (b) Report it at 7c and gate it at Phase 6, as a carried row. Recommendation: (b).
Blocks nothing.

**Q6. Search visibility by default.** (a) Keep read-your-writes (D6): a search after a write awaits a
coalesced refresh, a segment write until write-buffer D11 lands. (b) A periodic refresh with an explicit
wait option, as Elasticsearch's `refresh` and 7b's API do. Recommendation: (a) for 7c, since the E2E
suite asserts it; T5's mixed load reports its cost, and 7b decides the default. Blocks nothing.

## 7. Risks and what is not covered

- **Library steps on the path.** L2 (paused `feature/scheduler`), L4 (prospect's sliced search) and L5
  gate X2 and X3; write-buffer steps 2 to 4 are not required (D5, D6 stand in) but delete the stand-ins.
- **One large document.** Parse-ahead bounds it off the reactor; the buffered-body cap is new behavior for
  clients that send larger bodies.
- **Locks left on reactors.** D1's allowlist keeps short map locks; under contention a reactor can park
  on one for a wake's latency (research `:50-92`: about 10 µs p50, 63 to 77 µs p99 on the Mac). T4
  catches it.
- **Cluster mode.** The control-plane commands become coroutines (D13), but cluster mode is 7b's gate;
  T1 to T13 run `--solo`.
- **Not read:** `schema.cc` beyond the cited ranges, `discovery.cc` beyond the cited line, and Detent beyond
  the three cited files.
