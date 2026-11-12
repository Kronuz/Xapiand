> Status: DRAFT (2026-10-29) Revision 2, designing (ready for the architect's re-check): closes R1 to
> R7 and folds C1 to C10 of `docs/reviews/phase7c-design.md`. Ledger: `docs/status/phase7c.md`.

# Phase 7c: Xapiand that never blocks a reactor

Reads Xapiand master `3b0dc433f`; strata `d93bf7d` (its `include/` is unchanged at `5b0e6ed`) and
`design/write-modes` `ae2cf70`; prospect `b3ad3e3`; task `aa447e7`; reactor `0c04d0d`; http `6a07f99`
(Xapiand pins `dc3be77`); threadpool `feature/scheduler` `5caedce`; storage `671eaac`; Lua 5.4.7 as
fetched (`CMakeLists.txt:1100`). Clean room: `src/xapian/`, `src/sortable_serialise.{cc,h}` and
`src/server/remote_protocol_views.cc` were not opened.

Phase 7's single-node gate is 7a (passed, `docs/gates/phase7a.md`) plus 7c (strata `docs/PLAN.md:651-674`).
7c makes Xapiand's blocking primitives asynchronous and removes the offload pools. The owner ruled
on revision 1 (strata `docs/status/xapiand-on-strata.md`, "Owner answers on the 7c design"): the
Xapian path goes first (X1), the WAL stays until 7b, http drops its offload path entirely, blob
storage is not decided and stays `DataStorage` with its I/O in chores, the 2x-RAM fault row gates at
Phase 6, and read-your-writes stays the default.

## Summary

1. Every request is a `task::task` on the reactor that accepted it. A budget bounds each continuous
   run (a slice restarts on every resume), and a two-level scheduler runs fresh work ahead of
   demoted work (D2). An admission gate bounds in-flight requests per reactor (D2). Nothing on a
   reactor waits (D1).
2. The only work that leaves a reactor is an **index chore** on one host `WorkPool`: open, refresh,
   commit, close, merge, reclaim, check, blob I/O and parse-ahead (D4, D9).
3. `DatabasePool` becomes a registry that never waits. Every lease operation, timers included,
   is a turn of a per-index async mutex with a deadline, and a task holds at most one turn (D5).
   strata runs in a host mode where a writer call does no I/O (D6).
4. http's handlers become coroutines, with an awaitable body, a disconnect cancel and a lifetime
   rule. The offload pools, the control-plane worker, DP, DI, AC and the cleanup thread are deleted
   (D3, D4, D13, D16). Lua runs each call in its own environment and yields (D10).
5. S1 builds the gate tooling and the pre-7c baseline, and X1 deletes the Xapian path. Both start
   now; neither needs anything else in this design.

## What this depends on and changes

| Document | Relation |
|---|---|
| strata `docs/PLAN.md` Phases 6 (`:640-649`), 7 (`:651-674`), 8 | Gates 7c with Phase 6's reactor rows; T6 gates at Phase 6 (owner). X1 moves Phase 8's Xapian code removal to 7c's start |
| strata `ARCHITECTURE.md:163-185` | Implements its budget and guarantee; its "least-attained service" is two-level here (D2) |
| strata `write-buffer.md` (`ae2cf70`) D2, D8, D11, D12 | Depends; D5 and D6 stand in until write-buffer steps 2 to 4 land, then are deleted |
| strata `xapiand-on-strata.md` D7, D13 | D7 kept (read-your-writes). D13's bridge rule is replaced by D1 |
| `docs/reviews/engine-offload-architect.md` C7 | Changed: the only hop is a chore. Its V2 and V5 negative tests carry forward (D4) |
| `docs/reviews/phase7c-design.md` | R1 to R7 and C1 to C10 closed below; each names its decision, row or criterion |
| threadpool `docs/research/scheduler.md` (`:673-790`) | Depends on `WorkPool`; adds a merge aging rule (D4); keeps the gate as admission (`:786-789`) |
| http, reactor, prism, Detent | http loses its worker pool, `should_offload` and the body hand-off (owner, Q3); prism migrates in L3; Detent migrates separately and keeps its http pin until then |

## 1. Inventory: blocking primitives on reactor-reachable paths

After 7c every view runs on its reactor, so everything a view reaches is reactor-reachable. Counts
are greps on `3b0dc433f`; `harness/async_check.sh` (S1) classifies every hit (D14).

**A. Engine bridges, 33 sites (30 after X1).** `engine::sync_wait` (`src/engine/sync.h:44-48`) at
`handler.cc:271, 772, 814, 1178, 1425, 1727, 1759` and `database/shard.cc:438, 446, 459, 552, 568, 600, 716,
973, 1034, 1052, 1070, 1105, 1171, 1339, 1449, 1542, 1553, 1555, 1559, 1628, 1691, 1745, 1778, 1821, 1852,
1894`; `:438`, `:552`, `:568` are Xapian-only. `main.cc:836`, `:872` (CLI) reach the same handler.

**B. Pool checkout, 36 sites (30 after X1).** Seven condition-variable waits in `pool.cc` (`:191`,
`:198`, `:295`, `:302`, `:816`, `:820`, `:1088`, each with a `timeout_tp`); `cleanup_mtx` held across
commits (`:1103`); 26 `lock_shard` in `handler.cc`, the multi-endpoint checkout (`:1160`, `:1208`,
`:1211`), `schema.cc:2778`, `:2844`, `discovery.cc:710`. Six replication sites leave with X1
(`replication_protocol_service.cc:143`, `:160`, `:173`, `replication_protocol_views.cc:516`, `:682`, `:986`).

**C. The strata adapter and strata, 41 adapter sites plus 5 inside strata.** `engine/strata/shard.cc`:
27 `core.mtx` sites; `registry_mutex` (`:274-280`); six refresh-before-read sites that may write a
segment (`:395`, `:987`, `:1057`, `:1142`, `:1196`, `:1234`); four commits that fsync (`:191`, `:832`,
`:854`, `:876`); three opens (`:215`, `:348`, `:353`); the per-search pins (`pin.h:33-55`,
`search.cc:632-650`). Inside strata (`index.hh`), on the caller's thread: `apply` refreshes inline
(`:691`); admission drains (`:705`, and `install` at `:1472`: reclaim, `finish_deletes`, `sync_dir`,
`:3531-3545`); `apply` and `writer_scope` run owed chores (`:747`, `:2570`: `publish_merges`, `retain`); a lease
taker waits on a check-in's chores (`owner.hh:120-123`); `evaluate` faults on mapped segments.

**D. Storage and fsync, 16 sites.** `DataStorage` reads (`shard.cc:1208-1227`, from `handler.cc:717`,
`:1793-1803`), writes (`:1317`, `:1426`, `:1515` into `:1230-1280`), and the volume fsync
(`storage_commit`, `:969`, `:1283-1290`). Nine WAL appends queue to WL (`wal.cc:942-951`); its fsync is
the FS debouncer's (`wal.cc:116`, `:131`).

**E. Locks and waits in request code, 42 sites.** `schemas_lru.cc` (19 locks; nested database calls
at `:110`, `:163`, `:277`, `:338`); `index_resolver_lru.cc` (`:579`, `:868`; calls at `:252`, `:299`, `:399`,
`:436`); Lua (`processor.cc:155`, `:190`, `:304` held across `:318`, foreign read `:247-251`);
`DocIndexer::wait` (`handler.cc:2199`, `:2210`, `:2214`; DI queues `:2059`); the streamed restore
(`objects_mtx`, `search_views.cc:2870-2961`; blocking `body_reader->read`, `:2957`).

**F. Sleeps, 1 site.** `manager.cc:834`.

**G. Unbounded CPU on one request, 5 kinds.** prospect's search loops (`shards.hh:237`, `:279`) and the
host callbacks in them (`engine/strata/search.cc:689-711`); a Lua script (`processor.cc:318`); dump
scans (`handler.cc:772`, `:814`); decoding a buffered body of any size (http `http_request_parser.h:170`).

**H. Threads and hops, 11 kinds.** The reactors' `asio::thread_pool`s (`manager.cc:1059`, `:1066`); the
control-plane worker (`manager.cc:156`, `:467-488`); the remote and replication pools (leave with
X1); `engine::release_on_pool` (`release.h:40-55`); DP and DI (`manager.cc:240-241`); AC (`handler.h:335`);
FS (`xapiand_fsyncher.h:52`); WL (`manager.cc:236`); `DatabaseCleanup` (`cleanup.cc:80`); TR, DU, SU, IU,
PU (7b's). In the libraries: http's offload and streamed-body worker (`http_asio.h:633-756`,
`:780-790`), the synchronous `HttpHandler::handle` (`http_handler.h:190`), `Reactor::pool_`
(`reactor.h:116-128`).

**I. Destructors that reach database work, 3.** `~DocIndexer` (`handler.cc:1920`), `~Request`
(`search_views.cc:2719`), the strata core's closing commit (`engine/strata/shard.cc:186-196`).

**J. First-use loads and spins, 5 (review F8).** `engine::kind_of` (`kind.cc:59-89`: a global mutex,
then two `fs::exists`, uncached on a double miss; from `schema.cc:2977`, `database/shard.cc:384`);
`getStopWordsEntry` (`stopper.cc:48-67`: a global mutex across reading the file; from
`multivalue/generate_terms.cc:545`, `engine/text.cc:295`); `mime_type` (`server/mime_types.cc:120`, a
static that reads `mime.types` at `:54`); the census (`census.cc:139-146`, `ENGINE_CENSUS` builds);
`reader_handle::published_position` (strata `index.hh:1257-1262`, spins then `yield`).

## 2. The end state

```
 HTTP reactor r (one per CPU)                    WorkPool (one per host)
  ├ admission gate, scheduler, budget  ──chore──▶ WorkGroup per open index (+ per upload, RESTORE)
  ├ registry lookup (never waits)      ◀─post()──┤  query: read refresh, open, blob read
  ├ lease turns: apply (no I/O)                  │  flush: commit, close, check, blob write, parse
  └ lag probe                                    └  merge: merges, reclaim (strata's executor)
 control loop: setup, autocommit and idle-close tasks, shutdown
```

Threads left on the single-node path: the HTTP loops, the control loop, the `WorkPool` workers, the
logger, the debouncer wheel, completion's bounded fallback worker, and WL and FS until 7b (D8).

## 3. Decisions

**D1. The rule.** No reactor thread waits: no `sync_wait`, condition variable, sleep, file syscall,
`yield`, or lock whose critical section does I/O, waits or runs unbounded work. A lock taken on a
reactor guards O(1) to O(log n) work on an in-memory map and is allowlisted with its file:line and
reason (D14). Work that must wait suspends its coroutine. Inventory J: with one engine after X1,
`kind_of` returns strata's kind with no probe; stop words and `mime.types` load at startup before the
listeners open; `published_position` is replaced by `try_published_position` (`index.hh:1264`), and a
read that gets none awaits `task::yield()` and retries; the census is compiled out of every gate
build (`ENGINE_CENSUS` OFF, `CMakeLists.txt:72`) and classed as such.

**D2. Scheduling and admission.** Each HTTP reactor and the control loop own a
`task::asio_scheduler` (`task_asio.hh:43-88`; `TASK_ASIO` turns on at `CMakeLists.txt:773`, and the
comment at `:770-772` says the Asio binding runs the views). A request is a `task::task<void>` with
a `task::budget` (slice 200 µs, an estimate T4 tunes).

- *A slice is a continuous run (R2).* `budget` today sets its deadline when the slice starts, so time
  suspended counts (`task.hh:608-633`; the review's `budget_probe`). Kronuz/task changes it: each
  `running_on` scope, which every scheduler wraps around a resume (`task.hh:106-115`), stamps a
  thread-local slice start and restores the outer one on exit; `exhausted()` compares against it. A
  task resumed after a chore, a turn, an event or a body read starts a fresh slice; only a run
  longer than the slice is demoted. `budget_probe` becomes L1's test (0 demotions; 1 with the
  stamp removed).
- *The policy is two-level*, as `asio_scheduler` implements it: ordinary posts in FIFO order, demoted
  handles one per drain pass between them. No attained-service counter exists or is added.
- *Admission (R3).* Each reactor keeps two counters, each with its own cap and its own 503 (with
  `Retry-After: 1`): in-flight requests, counted from headers complete to response settled (cap
  1,000, today's offload queue limit, `manager.cc:1059`); and `asio_scheduler::demoted()` (cap 256,
  an estimate). Admission checks both at headers complete. reactor's `OffloadGate` (`reactor.h:99-112`)
  stays as this gate, renamed `reactor::AdmissionGate`, since nothing is offloaded. The io_context
  queue is bounded by the gate: each admitted task has at most one handle queued, and idle
  connections post nothing.
- *Reactor count (C5).* One reactor per CPU. `MAX_HTTP_SERVERS` (`opts.cc:57`, `:525`) and
  `--http-servers` go (PLAN, "Parallelism adapts"): request CPU now runs only on reactors.

**D3. HTTP handlers are coroutines; http has no offload path.** Kronuz/http deletes the worker pool,
`should_offload`, `wants_body_stream`'s worker and `ChannelBodyReader`'s blocking read
(`http_asio.h:344`, `:675-756`, `:780-790`; `http_handler.h:165-224`); reactor deletes `Reactor::pool_`.
`HttpHandler::handle` becomes `asio::awaitable<void> handle(Request&, ResponseWriter&)`, awaited on
the connection's executor. `SearchService` wraps each view's task in `task::as_awaitable`
(`task_asio.hh:92-107`); prism (`src/main.cc:317`, `:325`) migrates in L3.

- *Body bridge (R6.1).* A streamed body is a `BodyStream` whose `async_read(std::string&, token)` is an
  Asio operation over a bounded `asio::experimental::channel` that the connection coroutine feeds
  (today's `feed`, `http_asio.h:688-707`, kept). Kronuz/task gains the reverse bridge in
  `task_asio.hh`: `task::await_asio<Sig>(init)`, an awaiter that starts an Asio operation with a
  handler that stores its results and resumes the task through `scheduler_ref::post` on the
  scheduler it suspended on. RESTORE reads with `co_await task::await_asio<...>(...)`, and its
  `objects_mtx` goes: the parser state lives in the task.
- *Cancel (R6.2).* For each request, http arms `ConnectionInput::monitor` (`http_asio.h:455-480`)
  for the handler's whole run. That is the one-byte read it already keeps for deferred replies;
  pipelined bytes stay buffered for the parser. The monitor takes a callback, which requests the
  request's `task::cancel_source` on EOF or on a read error. Reads (search, GET, scans, dumps) pass
  the token to their budget and stop at the next checkpoint. Writes take no token: once admitted,
  a write completes and its response is attempted, so a client that half-closes after a PUT still
  gets its answer. The token is a flag, so a task queued on a turn, an event or a chore stays
  queued until it resumes. Each of those waits is bounded (D5, D4) and checks the token when it
  resumes.
- *Lifetime (R6.3).* The connection coroutine owns `request` and `writer`. Before it awaits the
  handler, it disables Asio cancellation and then awaits completion, as the streamed path already
  does (`http_asio.h:735`). A cancellation that arrives meanwhile becomes `cancel_source.request()`.
  Frames end only by completing: nothing calls `destroy()` on a suspended task. The destructors of
  `task::event`, `task::async_mutex` and `asio_scheduler` assert that no waiters or demoted
  handles remain. The shutdown order is D16.

**D4. Index chores: the only work that leaves a reactor.** A chore is index work that may wait on a
device or run long. `co_await chore(group, cls, fn)` posts a `WorkItem` (threadpool
`feature/scheduler`, `work_pool.hh:85-95`) and resumes the awaiter with `asio_scheduler::post()`, the
one thread-safe entry (`task_asio.hh:43-54`), never `post_demoted` (C10.1). Each open index owns a
`WorkGroup`; each RESTORE and each streamed upload owns its own group for its lifetime. Groups have
one lane per class (`work_pool.hh:1110-1128`), so classes do not queue behind each other in a group.

| Class | Chores | Why |
|---|---|---|
| `query` | read-your-writes refresh, open, blob read | A request waits on it; the reserve worker (`reserve = 1`, `:673`) keeps one free for them (C8.1, C8.3) |
| `flush` | commit, close, check, a flush owed by a write, blob write, parse-ahead, RESTORE prepare | Throughput work; parse-ahead and RESTORE prepare in the RESTORE's own group, so DRR splits service with the index's group (C8.1); a prepare chore runs about 1 ms |
| `merge` | merges, reclaim deletes (strata's executor binds here) | Background; at most `merge_cap` workers |

Aging (C8.2, threadpool L2): a merge lane that has waited longer than 100 ms (an estimate) takes the
next background pick ahead of `flush`, so sustained bulk load cannot starve merges. T5 reports the
merge backlog. No chore waits on its own index's group (threadpool review, architect ruling on
L2-B2, AC2): a wait serves only the group it waits on, and a wait on a group already on the calling
stack aborts. A chore that needs its index's other work either runs it in a child group it waits on,
or returns, and the task holding the index's turn awaits the group's idle notification. L2's
negative tests: V2's shape (a worker waits on its own group) aborts, and V5's shape (a reactor calls
a waiting entry) hits `engine::check_may_block` (C8.4). Deleted: the reactors' pools, `release_on_pool`, DP, DI, AC and
the cleanup thread.

**D5. The registry and the lease.** `DatabasePool` becomes a map from path to an entry: the reader
handle, the lease turn, the `WorkGroup`, the revision, a use count, the idle time, and a state
(`opening`, `open`, `closing`). A miss single-flights one open chore, and concurrent requests await
one `task::event` (Kronuz/task, new: a one-shot completion that resumes each waiter through its own
scheduler's `post()`). Readers take no checkout: they use the entry's `reader_handle`.

- *Every lease operation is a turn (R4.1).* apply, refresh, commit, close, merge publication and
  blob writes enter the entry's `task::async_mutex` (Kronuz/task, new: FIFO, waiter nodes in frames,
  the next holder resumed through its scheduler's `post()`). A chore that needs the lease (flush,
  commit, close) runs while its awaiter holds the turn. The autocommit debouncer's wheel thread and
  the idle-close timer only `asio::post` a task onto the control loop; that task awaits the turn
  and then the chore. Nothing calls strata off a turn, so `owner_break` (`owner.hh:124`) stays
  unreachable.
- *Close (R4.2).* The idle-close task picks entries with a zero use count. Under the map lock it
  marks them `closing`, then takes the turn and runs the close chore. A lookup that finds a
  `closing` entry awaits that entry's `closed` event, then single-flights a new open. A reader
  whose `acquire` throws `index_closed` (strata `index.hh:1250-1253`) retries through the registry
  once. A writer that gets the turn of a `closing` entry releases it and retries. `FLUSH` spawns
  one close task per open index and awaits them all, holding no turn itself.
- *One turn per task, and a deadline (R4.3).* A task holds at most one turn. Schema and
  index-settings writes (D11) complete, and release their turn, before the document's turn
  begins: a schema only grows, so a document that then fails leaves harmless fields. Xapiand's
  `IndexTurn` guard counts turns in the request context and asserts at most one in debug
  builds. Each wait also has a deadline: `async_mutex::lock_until(asio_scheduler&, deadline)`
  (in `task_asio.hh`) arms a `steady_timer`, and the grant and the timeout settle the waiter
  node with one compare-exchange; a timed-out node is skipped at handoff. The deadline is the
  request's remaining timeout, or 30 s (an estimate). Past it the request gets 503.
- *Write unit (C6).* The turn covers the apply, the revision update and the WAL enqueue as one unit.
  `add` enqueues at `database/shard.cc:1339` before `:1354`'s engine call, and commit enqueues at
  `:993` after `:973`; both stay inside the turn, so WAL order equals apply order across reactors.

Deleted: the seven condition-variable waits, readable pooling, `max_database_readers`,
`lockable_cond`, `cleanup_mtx`, `cleanup.cc:64-80`. When write-buffer step 2 lands, writers join
strata's write group and the host mutex goes.

**D6. The strata adapter holds no lock; strata runs in host mode.** `core.mtx`, `registry_mutex` (moved
to D5), `Pinned`, the shard ordering in `search.cc:632-650`, and `Hop` with every `scheduler_ref on`
parameter are deleted. A search takes one view guard per shard, after it awaits
`publish_through(p)` as a `query` chore when the shard's view is behind its read position. That
refresh is coalesced (`index.hh:1312-1318`), which keeps 7a's D7. Host mode closes R5:

1. Xapiand never calls `check_out()` (`index.hh:762-776`). Then nothing sets `chores_owed_` (only
   `check_in()` does, `:787-801`), and neither `apply`'s `run_chores` (`:747`) nor `writer_scope`'s
   handoff (`:2570`) nor the chore-bit wait (`owner.hh:120-123`) can run. Finished merges publish
   inside refresh and commit chores (`:807-813`, `:884-895`). The idle task also runs a refresh
   chore on an index with `merges_pending() > 0`. Debug builds assert `!chores_owed()` after
   every turn.
2. strata gains `index_options::defer_io` (L5). When it is set, `apply` neither refreshes (`:691`) nor
   drains (`:705`). It returns `verdict::flush_owed` or `verdict::drain_owed` before stage 1, with
   nothing applied. The turn runs `refresh()` or a new public `reclaim_for_space()` as a chore, then
   retries the apply once more, before it applies any later position (C3 of the L5 review: a later
   position makes the retry `skipped`). `defer_io` covers `apply` only and forces eager segments, so
   no first-read map happens inside it (L5-B1). `install` (`:1472`, which 7c does not call) drains
   inline and runs only as a chore. Xapiand sets no storage permit in 7c, so strata's fault tests
   cover `drain_owed`.
3. `evaluate`'s page faults on mapped segments are T6's subject, reported at 7c and gated at Phase 6
   (owner).

*Readers (C9).* Each index owns its hazard registry (`Core`, as the hostless path already does), sized
`readers_per_index`: the threads that can hold a view of one index at once, times 2 (`manager.cc`).
`too_many_readers` (`hazard.hh:47`, `:211`) maps to 503 and means more concurrent readers of one index than the
host runs. A search takes one record in each index it reads, so its fan-out is bounded by the open indexes, not by
the registry: a search walks its indexes in batches, each batch within the open-index budget, and a
runaway pattern answers 400 (`docs/mechanisms/wide-search.md`; the architect's original ruling,
`docs/reviews/phase7c-x2b-code.md`, capped the whole search at the pool size). T8 tests the 503 with a 16-record registry, and T13 states the registry's bytes
and the commit's scan cost (`docs/mechanisms/reader-views.md`, Limits).

**D7. Fsyncs and durability.** Every fsync, `F_FULLFSYNC` and directory barrier runs inside a chore:
strata's commit through completion's `PosixIO::sync_files`, and `storage_commit`
(`database/shard.cc:1283-1290`), which runs first in the same commit chore, as `:969` orders it today
(R7.2). `?commit=true`, `COMMIT` and `FLUSH` await their commit. *Coalescing by position (C7):* a
commit captures the applied position when its chore starts under the turn. A waiter whose write is
at or below that position joins it. A later write waits for the next commit, and a failure reaches
exactly the waiters joined to the commit that failed. Autocommit keeps its throttle and debounce
values (`handler.h:335`) and fires as D5's control-loop task.

**D8. Xapiand's WAL is untouched until 7b** (owner). Appends queue to WL without waiting
(`wal.cc:942-951`), inside the turn (D5). The synchronous mode belonged to the replica and goes with
X1. 7b replaces the WAL with the journal.

**D9. Blob I/O runs in chores over the existing `DataStorage` (R7).** No blob format changes, and Q4
stays open.

- *Buffered blobs* (a stored body up to the cap, D12): `storage_push_blobs` (`shard.cc:1230-1280`) runs
  as a `flush` chore inside the document's turn, before the apply, so the locator and the document
  land in one turn.
- *Streamed uploads* (`stored` with `Content-Length` above the cap; `stored` is decided by
  `search_views.cc:892`): the headers-complete hook picks the stream path. The upload reserves a
  fresh volume number from the entry's atomic `next_volume`. The shared writer's rollover
  (`shard.cc:1254`, `:1260`, `:1266`) also takes from `next_volume`, so no other writer appends to
  that volume. The body goes through Kronuz/storage's new streamed record write (L6:
  `begin(size)`, `append(chunk)`, `end()`, the XXH32 carried to the footer as `write` does,
  `storage.h:822`) in `flush` chores on the upload's own group, 1 MiB per chore (an estimate). The
  last chore syncs the volume. The document then applies with its locator in a normal turn, so no
  turn is held during the upload. A disconnect removes the volume in a chore. A body above the cap
  with no `Content-Length` gets 411.
- *Reads:* `storage_get_stored` (`shard.cc:1208-1227`) opens its own reader in a `query` chore. A blob
  above the cap streams out through `ResponseWriter::stream` (`http_handler.h:97`), one chore per
  chunk, using the library's chunked `read` (`storage.h:972`).

**D10. Lua: a call's own environment, yields, and bounds (R1).** Each thread that runs scripts (a
reactor, or a worker in a parse-ahead chore) keeps its own compiled-script cache, and the locks at
`processor.cc:155`, `:190`, `:304` go.

- *Isolation.* A script compiles once as `local _ENV = ...; <body>` on one line, so line numbers hold.
  Each call creates a table holding `_method`, `_doc`, `_old_doc` and the params, with metatable
  `{__index = base}` (base holds the opened libraries), and runs `func(env)` in its own
  `lua_newthread`. Interleaved calls on one script share no globals. A global a script sets lives
  for that call only; today it persists, serialized by the per-script lock.
- *Yields.* A count hook (`LUA_MASKCOUNT`, every 10,000 instructions, an estimate) yields when the
  slice is spent and `lua_isyieldable(L)` holds. The task awaits `budget.checkpoint()`, then calls
  `lua_resume`. When the call is not yieldable (inside a C call such as a `table.sort`
  comparator), the hook keeps counting: past 100 consecutive hook calls (10^6 instructions, an
  estimate) without a yield point, the call ends with an error. That bounds a comparator or a
  `gsub` callback that never returns.
- *Bounds.* An instruction cap (10^8 per call, an estimate), checked in the hook. A **work meter** (5 x 10^8 units per call, a unit
  about a nanosecond): the library and the VM charge what they touch (bytes scanned, compared or copied, matcher steps, table
  elements, stack values moved), and a call that spent 200,000 units since it last yielded yields at its next instruction. Code
  with no Lua instructions is therefore *not* free: it is bounded by the meter, per call and across calls, not by the sizes the
  memory cap allows (that earlier claim was false for time). A live-byte cap per script state (64 MiB, an estimate), enforced by
  `lua_Alloc`. Lua's pattern matcher has a depth cap (`lstrlib.c` `MAXCCALLS`, 200) but no step cap, so the build carries one
  patch (`contrib/lua/xapiand-limits.patch`, generated from 5.4.7, every change to Lua's sources) that counts matcher steps per
  pattern item, class member and copied byte, raises "pattern too complex" past max(10^6, 64 x subject length) per API call, and
  charges the steps to the meter. **No uninterruptible stretch passes 16 x 10^6 units** (`max_call_work`, about 20 ms): the work
  done since the hook last ran is capped, so a library call that would touch more is refused and the matcher's budget is at most a
  fifth of it in steps. A single non-yieldable call therefore holds a reactor at most about 20 ms, which keeps T4's 1 ms p99 for
  benign scripts and bounds it at about 20 ms for hostile ones; the one exception is the result's conversion (below), at most about
  0.7 s. A call that crossed a bound is **condemned**: the host's flag makes the base's `pcall` and
  `xpcall` rethrow whatever the error carries and the hook raises at every instruction, so nothing a script runs while an error
  unwinds can outlive the cap. Scripts have no to-be-closed variables, `__gc`, `__close` or `__mode`, no `string.pack` family,
  and a Lua `table.sort`. The converted result is bounded by 16 MiB of strings (the body cap) and 262,144 value units (an object is 8); the conversion holds
  the reactor at most about 0.7 s for a result at its caps (2.7 microseconds a value, accepted: the write's own indexing of the same
  document holds it longer), and the whole document is converted, so a script over a document past the cap answers 413. A drift test
  checks that the patch applies to the pinned tarball's hash and that the compiled tree equals the patched tarball file for
  file, and the patch is listed in `docs/dev/patches.md`. `docs/mechanisms/lua-bounds.md` describes how it fits together.
- Each bound returns a 4xx. Foreign scripts load through an awaited handler read
  (`processor.cc:247-251`).

**D11. Schema and settings caches hold no lock across I/O.** The nested reads and writes
(`schemas_lru.cc:110`, `:163`, `:277`, `:338`; `index_resolver_lru.cc:252`, `:299`, `:399`, `:436`) become
awaited operations, and each completes before any document turn (D5). A miss single-flights one
load per path (`task::event`). The maps' own critical sections are allowlisted (D1), and updates keep
their version compare-and-swap.

**D12. CPU on a reactor is sliced.** prospect's search awaits `budget.checkpoint()` at every window
boundary and every 4,096 ordinals of the exhaustive loop (`shards.hh:237`, L4); host callbacks run
inside those slices. Dump scans check every 64 documents. A buffered body has a cap (16 MiB, an
estimate), which http enforces at headers complete from `Content-Length`, or as bytes arrive.
Above it, a `stored` upload streams (D9), RESTORE streams as today, and anything else gets 413.
Documents above 64 KiB (an estimate) are decoded in chores. A RESTORE batch is parsed in parse-ahead chores and
its plain `index` objects are prepared (fields, terms, raw record) in `flush` chores of the RESTORE's group, each about
1 ms, in input order with schema changes committed in input order; an object that reads the stored document (patch,
update, merge, a script, `_schema`) is prepared on the reactor after every earlier object applied, and every apply
stays in the shard's turn (`docs/perf/x7-loop-stalls.md`, the RESTORE chore design).

**D13. The control plane is coroutines on its loop.** `control_offload` and the control reactor's
worker are deleted (`manager.cc:156`, `:467-488`). Node setup, `load_nodes`, `new_leader` and the
shutdown checks are tasks on the control loop, which await handler operations. The setup retry
awaits a timer (`manager.cc:834`).

**D14. Enforcement.** `engine::check_may_block` (`engine/sync.cc:48-71`) guards every remaining
blocking entry: the CLI's `sync_wait`, `WorkGroup::wait` and the registry's close-all at shutdown.
`reactor::LagProbe` records each loop's lateness. It runs at 1 ms while the loop served a request in
the last second and at 100 ms otherwise, so an idle loop wakes 10 times a second. Its log2 µs
buckets have 4 sub-buckets each, and T4 reads a percentile as its bucket's upper edge: "1 ms" means
the 1,024 µs edge. Destructors do no database work: `~DocIndexer`, `~Request` and the core's
closing commit become awaited closes, and their destructors assert that the close ran.
`harness/async_check.sh --strict` runs in CI. Every hit is in one of four classes: allowlisted
(file:line, reason), owed (the decision that removes it), compiled out of gate builds, or
unclassified. `--strict` fails on unclassified hits; `--strict --owed` also fails on owed hits, and CI runs both
(the gate after X6: the allowlist holds no owed row).

**D15. Removed options.** `--http-clients`, `--http-servers`, `--bulk-preparers`, `--bulk-indexers`,
`--committers`, `--fsynchers` (`opts.cc:237-288`), and with X1 the remote and replication options
(`opts.cc:262-301`), each with a changelog bullet. `--writers` stays while D8 holds.

**D16. Shutdown order (R6.3), as the code runs it (X6).** A signal reaches a control task, which waits
for the requests in flight (`shutdown_checks`; the acceptors still accept, up to about 45 s). Then
`join()` runs on the main thread with one deadline, `shutdown_write_bound` (35 s) after it starts:

1. The housekeeping and the signal read stop, and the control tasks drain.
2. **Stop 1, the HTTP service** (`stop()` on a thread of its own): the acceptors close, `abort_all()` on
   each reactor requests every live request's cancel source (http registers one `Abortable` per request
   through `Reactor::track`), reads end at their next checkpoint, admitted writes finish (or answer 503
   when their turn does not come in 30 s), and the loops stop.
3. **Stop 2, the autocommits**: the waits that had not fired are dropped; closing an index commits what it
   has pending.
4. **Stop 3, the indexes**: every index nobody uses closes on the main thread, which is not a reactor (its
   turn, then its close chore on the `WorkPool`, polled in slices of at most 500 ms); the registry needs
   no loop. `WorkGroup::wait` runs off-reactor under `engine::check_may_block`.
5. **Stop 4, the write-ahead logs**, then **Stop 5, the fsync threads**, then destruction (the registry,
   and with it the `WorkPool`, last).

**The HTTP stop's arithmetic (W3).** Reactor R's `stop()` waits one grace for the live requests, cuts the
connections of what is left (`force`), and waits one more grace: at most 2 x grace, then it stops the loops. `join()`
sets the grace from the budget that is left when Stop 1 starts, `left`: `grace = max(250 ms, (left - 5 s) / 2)`
(`XapiandManager::http_shutdown_grace`), so the stop ends before the deadline and 5 s remain for the autocommits
and the registry. With the default bound (35 s = 30 s `DB_TIMEOUT` + 5 s) and nothing else spent, the grace is
15 s and the stop takes at most 30 s. An admitted write waits for its index's turn for at most `DB_TIMEOUT`
(30 s) from its admission, which is before Stop 1, so it applies or answers `503` within 2 x grace: the stop no
longer destroys it while the budget has time. Its client is answered only when that happens within the first
grace; after it the connection is cut, the write still applies. A stalled client (one that stops reading) holds
the stop for the first grace, where the fixed 5 s used to cut it. With less budget left (the drains took
time) the grace shrinks to what is left, down to 250 ms, and a write still waiting then is destroyed at Stop 1
as before (task's phase 3).

An index that cannot close within the bound (a held turn, a hung disk), or an HTTP service that cannot
stop, is logged and abandoned, and the node exits with status 75 (`EX_TEMPFAIL`) after flushing the logs;
the next start replays them. A second signal ends the wait at once, with the same exit. Differences from
the first draft of this decision: the acceptors close at the start of `join()`, after the clients wait,
and not first; the loops stop before the indexes close, not after.

T7 runs this under load on ASan and TSan builds.

Extraction ledger rows owed: `task::event`, `async_mutex`, `lock_until`, `await_asio`, the
per-resume slice and `demoted()` (Kronuz/task); `AdmissionGate`, `LagProbe` and the removed pool
(Kronuz/reactor); coroutine handlers, `BodyStream`, the disconnect monitor and the removed offload
(Kronuz/http); `WorkPool` and aging (Kronuz/threadpool); `defer_io` (strata); the streamed record write
(Kronuz/storage); in Xapiand, the deleted pools, `control_offload`, `release_on_pool`, the cleanup
thread, the Xapian engine and the remote and replication services. How-it-works notes owed: the
scheduler and admission (D2), chores (D4), the registry and lease (D5), Lua (D10), shutdown (D16).

## 4. Gate rows

Timing rows (T4, T5, T11, T12) are the lead's, on `main`: five interleaved trials, 95% bootstrap
interval, smoke then confirm under `quiet exclusive`. Branches make functional claims only. Load
(S1's K3): heavy is concurrent exhaustive searches with aggregations, wildcard and phrase
searches, and a RESTORE stream into its own index `restore-target`, at 4x the reactor count in
clients. Light is open loop at a fixed rate: GET by id and single-term searches on `accounts`, plus
10% write-then-GET pairs on `accounts` (each awaits a refresh, R2). `accounts` is replicated to
1,000,000 documents. Every run records phase times, segment count and merge backlog.

| Row | Target | Procedure |
|---|---|---|
| **T1** no offload pool | The census lists only section 2's threads; WL and FS stay until 7b | `harness/thread_census.sh` during `stall_load.py`; `async_check.sh --strict` exits 0 |
| **T2** E2E on strata | 0 assertion regressions against 7a's rerun (566 of 566); 0 guard hits, 0 ASan reports; a `stored` upload of 2x the body cap, and one just under it, read back byte-equal | 7a's T6 procedure (`e2e_capture.sh`, `e2e_diff.py`) on a Debug ASan build; `harness/stored_stream.sh` |
| **T3** recovery | `wal_recovery_check.sh` and `engine_crash_check.sh` pass; writers on two reactors into one index, killed mid-run, replay to the model | As written, `XAPIAND_ENGINE=strata`; `engine_crash_check.sh --reactors 2` |
| **T4** reactor stall | p99 lag ≤ 1 ms (the 1,024 µs edge, D14) on every loop under the mixed load; p99.9 and max reported | `stall_load.py --phase loaded`; lag-histogram deltas from `:metrics` |
| **T5** light-query p99 | Under heavy load ≤ 2x its unloaded p99 + 1 ms (PLAN `:645-647`), and below the pre-7c baseline; a same-index run (RESTORE into `accounts`) reported as Q6's cost | `stall_load.py`, `unloaded` then `loaded`, then `--restore-into accounts` |
| **T6** reactor faults | Major faults on reactor threads per 1,000 queries ≤ 1 at 2x RAM: reported at 7c, gated at Phase 6 (owner) | Linux gate VM, `getrusage(RUSAGE_THREAD)` per loop, `stall_load.py --index-size 2xram` |
| **T7** concurrency and shutdown | 0 TSan and 0 ASan reports; the final dump equals the model; a SIGTERM at 300 s exits 0 with every acknowledged write durable | `engine_mixed_load.py BIN OUT --engine strata --secs 600 --term-at 300` on TSan and on ASan builds |
| **T8** overload | A light and a heavy overload each give a non-zero 503 count; 503s come within one slice of the cap; in-flight and demoted peaks reported; RSS within 10% of T4's run (an estimate); a 16-record index registry gives 503s, not errors | `stall_load.py --overload light 4` and `--overload heavy 4` (4x the saturating rate, K3); `test_readers_cap` |
| **T9** cancellation | A read whose client disconnects frees its reactor within one slice, and the cancel counter rises; a write whose client half-closes completes and is durable | Functional tests: a heavy search disconnected after 50 ms; a PUT then `shutdown(SHUT_WR)` |
| **T10** Lua bounds | Two calls interleaved on one script keep their own `_doc`; an infinite loop ends at the instruction cap; one inside a `table.sort` comparator ends at the non-yieldable bound; an exponential pattern, a quadratic plain `find`, a pattern with a long literal run or class, a vararg chain, a loop of library calls over a large string, a swallowed bound (a to-be-closed handler), an allocation past the cap and a single library call or pattern past `max_call_work` each end; a result past its byte or value budget answers 413; a foreign script and a local one of the same name never swap; all 4xx; T4 holds with four looping scripts, and with four looping library calls; with 16 clients each re-sending a request whose script makes one worst-case call, a light GET's p99 stays near the cap's 20 ms plus the host's own | Unit tests (the review's `lua_yield_probe` cases among them) plus `stall_load.py --lua-loop 4` |
| **T11** replaced layer wins by a lot | Dispatch cost per light request is at most half the offload hop's, in CPU and in wall; parity fails (owner, 2026-10-27) | `harness/profile_light.sh` on both binaries at concurrency 1: CPU from frames, wall from K6's timestamps |
| **T12** no throughput loss | RESTORE docs/s (20,000 accounts) and saturated query QPS each ≥ 1.0x pre-7c `main`, on a host with more than 10 CPUs; the row states its CPU count | `profile_bulk.sh`, `loadtest.py`, interleaved |
| **T13** every gate | ASan, UBSan, TSan; coverage at TESTING.md's targets; restore corpora replayed; per-open-index cost (entry, turn, `WorkGroup`, strata's three groups, a `DataStorage` handle) at 10,000 open indexes, and the per-index reader registry's bytes, stated | strata PLAN, "Every gate"; `small-indexes.md`'s open-index rows |

## 5. Steps

```
 S1 tooling + baseline ──────────────────────────────────────────┐
 X1 drop Xapian ──────────────┐                                  │
 L1 task, L2 threadpool ──────┼─> X2 registry, lease ─> X3 views ─┼─> X6 ─> X7
 L3 http, L4 prospect ────────┤                  X4 Lua, X5 ─────┘
 L5 strata, L6 storage ───────┘
```

1. **S1, tooling and baseline**, and **X1, the Xapian deletion**, start now, in parallel.
2. **L1 to L6**, each on its own branch in its own repository, start now:
   - L1 task: `event`, `async_mutex` with `lock_until`, `await_asio`, the per-resume slice,
     `demoted()`, and the destructor asserts.
   - L2 threadpool: land `feature/scheduler` (sanitizers and CI on the rebased branch, K1 to K3
     reruns, consumer builds), plus merge aging and the V2 and V5 tests.
   - L3 http and reactor: the offload removal, coroutine `handle`, `BodyStream`, the disconnect
     monitor callback, `AdmissionGate`, and the body cap; prism migrates.
   - L4 prospect: sliced search.
   - L5 strata: `defer_io` and the `WorkPool` executor binding.
   - L6 storage: the streamed record write.
3. **X2**: handler and shard operations as tasks; the registry and lease (D5); host mode (D6); chores
   (D4, D7, D9). `sync_wait` leaves every server path.
4. **X3**: views on the reactors (D2, D3); `FLUSH`, RESTORE parse-ahead, streamed uploads (D9, D12).
5. **X4** Lua (D10); **X5** schema, resolver and control plane (D11, D13); inventory J (D1).
6. **X6**: deletions (D4, D13, D15), destructor asserts, shutdown (D16), `--strict` with owed hits.
7. **X7**: the lead runs T1 to T13 on `main`.

**S1 (step 1).** Branch `phase7c/tooling` in `Xapiand-wt/phase7c-tooling`, plus `feat/lag-probe` in
Kronuz/reactor. Done criteria:

- K1 Kronuz/reactor `reactor::LagProbe` (D14 intervals and buckets) with a `snapshot()`. Tests:
  a 5 ms busy handler lands in the ≥ 4,096 µs bucket and fails with rearming removed; a counting
  allocator sees zero allocations after `start()`; after 1 s idle the wake rate drops to 10 per
  second. ASan, UBSan and TSan clean, CI green on every platform (C4).
- K2 Xapiand: every HTTP loop and the control loop run a probe; `:metrics` exports
  `xapiand_reactor_lag_seconds` per loop; the reactor pin moves; docs E2E on strata 566 of 566.
- K3 `harness/stall_load.py` (C3):
  - the RESTORE writes `restore-target`, and `--restore-into accounts` gives the same-index run;
  - light latency is measured from each request's scheduled send time, and the achieved and
    target rates are printed and asserted within 5%;
  - `--saturate` finds the saturating light rate by doubling the rate until p99 exceeds 10x the
    unloaded p99, then bisecting, and `--overload light|heavy N` uses it;
  - every phase writes light p50, p99 and p99.9, lag deltas per loop, 503 and error counts,
    in-flight and demoted peaks, segment count, merge backlog and phase times as JSON;
  - `--lua-loop` and `--index-size` are present (later steps fill them);
  - a 20,000-document dry run gives non-empty results.
- K4 `harness/thread_census.sh`: names and counts by prefix (macOS `ps -M`, Linux `/proc/PID/task/*/comm`);
  its dry run lists today's pools.
- K5 `harness/async_check.sh` (C1). Rules for file syscalls, `std::ifstream`, `fopen`, mutexes,
  `yield`, sleeps, and the condition-variable and `sync_wait` families. Every hit is in one of
  D14's classes, owed hits name their decision, and inventory J is among the hits. Done is zero
  unclassified hits on `3b0dc433f`.
- K6 `harness/profile_light.sh` (C2): CPU per request from frame samples, by named group (offload
  hop, scheduler, engine, HTTP); wall from timestamps at the post, the start of the run and the
  resume on the loop. These are taken by an http branch `trace/hop` that records them under a
  macro around the offload `co_spawn` (`http_asio.h:779-789` at `dc3be77`), used only for this
  baseline build. The dry run on `3b0dc433f` shows a non-zero hop share in CPU and in wall.
- K7: the ledger names the commands the lead runs for the pre-7c baseline (T5, T11, T12) on master
  before X2 lands; the developer makes no timing claim.

**X1, the Xapian deletion.** Branch `phase7c/drop-xapian`. Delete `src/xapian/`, `src/engine/xapian/`,
the `XAPIAN_OBJ` target and engine selection, the remote and replication services (`src/server/`
`remote_protocol_*`, `replication_protocol_*`, by `git rm`, unopened), their options (D15), the Xapian-only
tests (`test_wal_replay_xapian`, `CMakeLists.txt:2217`), and the Xapian-only inventory sites (A 3, B 6).
`sortable_serialise` stays: strata paths use it (`engine/strata/shard.cc`, `serialise.cc`), and Phase 8
removes it. `DataStorage` stays (D9). Done criteria:

- The lead confirms that the final Xapian-only release branch (`xapiand`) exists before X1 lands.
- No reference to `xapian.h`, `engine/xapian`, `remote_protocol` or `replication_protocol` remains
  outside the changelog and docs history, by grep.
- Debug ASan and Release builds pass, and CI is green on every platform. Docs E2E on strata gives
  566 of 566 with 0 regressions; T3's checks pass.
- The changelog's `Removed` lists the engine, the protocols and the options. ARCHITECTURE,
  AGENTS and the licensing note say the binary stays GPL until Phase 8 removes `sortable_serialise`.
  The extraction ledger has its row. Cluster mode has no replication from X1 until 7b, and the
  changelog says so.

## 6. Owner answers and open questions

Answered (2026-10-29): Q1 (a), delete the Xapian path at 7c's start (X1). Q2 (a), the WAL stays
until 7b (D8). Q3, http drops its offload path entirely; Xapiand, prism and Detent migrate, Detent
separately (D3). Q4 is not decided and is out of 7c: blobs stay in `DataStorage`, with I/O in
chores (D9). Q5, T6 gates at Phase 6. Q6, read-your-writes stays the default (D6).

No new question blocks a step. Three choices are made here and are reversible:

1. EOF cancels reads but not writes (D3).
2. A streamed upload needs `Content-Length` (D9).
3. Lua carries one limits patch: a work meter, a matcher step budget and no unbounded features (D10).

## 7. Risks and what is not covered

- **Library steps on the path.** L1 to L6 gate X2 and X3. Write-buffer steps 2 to 4 are not
  required, but they delete D5 and D6's stand-ins.
- **Threads.** Reactors and `WorkPool` workers are both one per CPU. Chores are mostly device waits
  and parked workers use no CPU, but T12 shows whether the CPU-bound parse-ahead competes with the
  reactors.
- **Per-shard streamed volumes.** Each upload above the cap gets its own volume file, until the
  owner decides Q4.
- **Locks left on reactors.** Allowlisted map locks can park a reactor for a wake's latency (research
  `:50-92`, about 10 µs p50 on the Mac); T4 catches it.
- **Cluster mode** is 7b's gate. T1 to T13 run `--solo`.
- **Not read:** `schema.cc` beyond the cited ranges, `discovery.cc` beyond `:710`, and Detent.
