# Xapiand review: Phase 7c design (preliminary, gates step 1)

- Date: 2026-10-29.
- Reviewed: `docs/design/phase7c.md` and `docs/status/phase7c.md` on `design/phase7c` at
  `Xapiand:fdcc91862` (design commit `Xapiand:0a8d235b8`).
- Checked against: strata `docs/PLAN.md` Phases 6 to 8 (`strata:d93bf7d`, `:640-698`);
  `docs/reviews/engine-offload-architect.md` (EO1 to EO5, C7); `docs/status/offload-bridge.md`;
  `docs/gates/phase7a.md`; Xapiand master `Xapiand:3b0dc433f` for every inventory citation;
  task `task:aa447e7` (`task.hh`, `task_asio.hh`); reactor `reactor:0c04d0d` (`reactor.h`); http
  `http:dc3be77` and `http:6a07f99` (`http_asio.h`, `http_request_parser.h`); threadpool
  `threadpool:5caedce` (`feature/scheduler`, `docs/research/scheduler.md:673-790`); strata
  `strata:d93bf7d` (`include/strata/core/{index,owner,executor}.hh`); prospect `prospect:b3ad3e3`
  (`shards.hh`); Lua 5.4.7 as Xapiand fetches it. The owner's answers relayed by the lead on
  2026-10-29: Q1 (a), Q2 (a), Q5 (b), Q6 (a); Q4 not decided and D9 out of 7c's scope; Q3 open.
- Checked by: citations replayed on `Xapiand:3b0dc433f` (`.scratch/review-7c/cite.sh`); greps for
  blocking primitives and file syscalls over Xapiand's own code; two probes, built with ASan and
  UBSan by `.scratch/review-7c/build.sh` (Apple clang, under `quiet light`):
  `lua_yield_probe.c` (D10) and `budget_probe.cc` (D2), outputs in `lua_yield_probe.out` and
  `budget_probe.out`. Kept: `.scratch/review-7c/` (the probes, `build.sh`, `cite.sh`, the Lua
  5.4.7 sources copied from a FetchContent tree, `strata-index-d93bf7d.hh`).
  - Not run: no Xapiand build, server, E2E, TSan, Linux or FreeBSD, and no timing of any kind.
    GPL sources (`src/xapian/`, `src/sortable_serialise.{cc,h}`,
    `src/server/remote_protocol_views.cc`) were not opened.

## Verdict

**APPROVE WITH CONDITIONS for step 1 (S1) only: the tooling and the pre-7c baseline can start once
the lead's done criteria take C1 to C4. L1 to L5 and X1 onward do not start until revision 2
closes R1 to R7.**

What holds, and reproduces:

- The inventory's citations are accurate where checked. All 33 `engine::sync_wait` sites match
  (`handler.cc` 7, `database/shard.cc` 26), as do the seven condition-variable waits in `pool.cc`
  (`:191`, `:198`, `:295`, `:302`, `:816`, `:820`, `:1088`), `cleanup_mtx` (`:1103`), the
  `DocIndexer` waits (`handler.cc:2199`, `:2210`, `:2214`), the strata adapter's refresh, commit
  and open sites (`engine/strata/shard.cc:191`, `:215`, `:225-239`, `:274-280`, `:348`, `:353`, `:395`,
  `:832`, `:854`, `:876`, `:987`, `:1057`, `:1142`, `:1196`, `:1234`), and the Lua locks
  (`processor.cc:155`, `:190`, `:304`, `:318`, `:247-251`).
- The shape is right. The plan has one hop off the reactor (chores on a per-index `WorkGroup`), no
  thread setting, and readers that never check out. It deletes offload, DP, DI, AC and cleanup
  outright, as the review's EO4 and EO5 asked, and C7 is changed knowingly.
- Step 1 is independent of every open question. It also builds what PLAN Phase 6 asks for (the
  lag probe and the light-query baseline that `phase-1.md:60` still owes).

What blocks the later steps: R1 (Lua globals leak between interleaved requests), R2 (the budget
counts wall time, not CPU used), R3 (no admission bound for light load), R4 (commits and
closes run outside the lease), R5 (strata's apply does I/O inline beyond `flush_owed`), R6 (the
coroutine handler's lifetime and cancellation are not designed), and R7 (parts of the design
depend on D9, which is now out of scope).

## Findings

### F1 (R1). D10: once the per-script lock goes, interleaved requests share one script's globals

A `Processor` owns one `sol::state` per compiled script (`processor.cc:225-298`). Each run writes
`_method`, `_doc`, `_old_doc` and the params into that state's global table, then calls the chunk
(`processor.cc:302-318`). The per-script mutex (`:304`) is what keeps two runs apart. D10 removes it
and yields mid-script, so a second request on the same reactor overwrites the first request's
`_doc` while the first is suspended. `lua_yield_probe` L1, with one state and two `lua_newthread`
coroutines, a count hook every 10,000 instructions, and globals set the way `Processor` sets them:
`A started with _doc=doc-A, reads _doc=doc-B at its end (LEAKED from request B)`. The second
request's document gets written into the first one's.

Hook yields also fail inside C calls. L2, a finite `table.sort` of 100,000 numbers with a Lua comparator:
`attempt to yield across a C-call boundary`. The script dies with an error unrelated to its own
code. L3, with the hook checking `lua_isyieldable` first: the script completes, but 543 hook calls
found no yield point against 40 yields. A comparator, `string.gsub` callback or sol2 callback
therefore runs unsliced.

Fix: give each call its own environment, either a per-call `_ENV` table whose `__index` is the
shared base (set as the loaded function's first upvalue on a per-call closure) or one `lua_State`
per in-flight run. Alternatively, keep a per-script `task::async_mutex` and say that it serializes
the script. The hook yields only when `lua_isyieldable(L)`, and the instruction cap still applies
inside C calls. T10 adds a test where two requests interleave on one script and asserts isolation,
plus an infinite loop inside a `table.sort` comparator that ends at the cap.

### F2 (R2). D2: `task::budget` measures wall time since the slice began, not CPU used

`budget` sets `deadline_ = cycles::read() + slice_` when the slice starts. It resets only inside
`checkpoint()` after it demotes (`task.hh:608-633`). Time a task spends suspended, waiting on a
chore, the lease, a `task::event` or a body read, counts against its slice. `budget_probe`, with a
200 µs slice, a 2 ms suspension and about 0 µs of work: `demotions before the wait: 0; after ...: 1`.
Under D6 and Q6's read-your-writes default, a light search awaits a refresh chore and then is
demoted at its first checkpoint, behind the heavy demoted searches. That is the opposite of D2's
"light requests run ahead of demoted heavy ones". The design also calls the policy
"least-attained service", but `asio_scheduler` has two FIFO levels, with one demoted handle per
drain pass (`task_asio.hh:56-83`). Nothing tracks the service a task has attained.

Fix (L1's scope): the slice restarts whenever a task resumes from a non-demoted post, or the budget
counts only time run. If least-attained service is meant, add an attained-service counter and
more than two levels. Otherwise call the policy two-level. T5's light mix must include light
requests that await a refresh (a GET by id after a write) so the row exercises this path.

### F3 (R3). D2: the demoted cap does not bound light overload, and T8's procedure never reaches it

Today `OffloadGate` bounds each reactor's offloaded requests at 1,000 and answers 503 beyond that
(`manager.cc:1059`, `:1066`; `reactor.h:96-112`; `http_asio.h:780-784`). Every view except OPTIONS,
QUIT, OPEN and CLOSE is offloaded (`search_service.cc:28-37`). D2 replaces the gate with a cap on
`asio_scheduler::demoted()`. That cap misses three cases:

1. A light request finishes inside its slice and is never demoted.
2. A heavy request that is suspended on a chore, the lease or an event is not in the demoted queue.
3. The io_context's own queue has no bound.

T8 drives "four times the saturating light rate", which is exactly case 1. The 503 path never
fires, and the row's RSS bound is left to luck. The threadpool research's own reactor sketch keeps
the gate as admission (`scheduler.md:786-789`: "the `OffloadGate` stays as admission").

Fix: cap in-flight requests per reactor (the gate, counted from accept to response) in addition to
the demoted cap, and give each its own counter. T8 runs a light overload and a heavy overload,
asserts a non-zero 503 count for each, and reports the in-flight peak.

### F4 (R4). D5 and D7: autocommit and idle close bypass the lease; the registry has no close protocol

D7 says "the wheel thread fires and only posts a commit chore". D5 says "a control-loop timer ...
posts close chores". strata's write lease is held by a thread token. A writer call made while
another thread holds the lease calls `owner_break` (`owner.hh:107-126`). A commit or close chore
posted without first taking the host's per-index `async_mutex` therefore races a reactor's apply
turn. That is an abort in debug builds and a data race in release.

Three more gaps in D5:

1. A request that finds a registry entry just before its idle close gets `index_closed` from
   `reader_handle::acquire` (`strata index.hh:1250-1253`). The design does not say whether that
   request reopens through the single-flight or fails.
2. Writes can nest across indexes. D11 makes schema and settings writes awaited operations on
   another index, so a task can hold index A's turn and await index S's. The design gives no rule
   against that.
3. Today's checkout waits carry a deadline (`pool.cc:191`, `:295`, `:816`, each with
   `timeout_tp`). The FIFO `async_mutex` has none, so a lease cycle hangs both indexes' writers
   forever.

Fix:

1. Every lease operation (apply, refresh, commit, close, merge publication) enters the turn first.
   The autocommit and idle timers spawn a task on the control loop that awaits the turn, then the
   chore.
2. Registry close: mark the entry closing under the map lock. Readers that hit `index_closed`
   retry through the single-flight open.
3. Add the rule "a task holds at most one lease turn", with a debug assert on a per-task count.
   Also give each turn wait a deadline that answers 503, or show why no cycle can form.

### F5 (R5). D5 and D6: "cheap turns run inline" is not true of strata's apply as pinned

L5 exposes only the `index.hh:691` condition (`flush_owed`). `apply` reaches three more I/O paths
on whichever thread runs it:

1. When a check-in found the lease held and the chores are owed, the end of `apply` runs
   `run_chores()` (`index.hh:745`, `:2583-2595`). `writer_scope`'s handoff runs `check_in()` for
   the same reason (`:2570`). That chain runs `publish_merges` (`check_segment` over the mapped
   sources, `:3122`) and `retain()`, which reads a kept manifest from disk on a cache miss
   (`:4537-4541`).
2. Storage admission calls `drain()` when the permit is short (`:705`). `drain()` reclaims files,
   waits for posted deletes in `finish_deletes()` (on the WorkPool once D4 binds the executor), and
   runs `sync_dir()` (`:3531-3545`). Xapiand sets no storage permit today (`engine/strata/shard.cc:162-169`).
   A disk-full policy for the release would enable this path on the reactor.
3. `evaluate()` on replace-by-key reads segment data through the mapping, which page-faults
   (T6's concern, now gated at Phase 6 under Q5).

Fix (L5's scope): strata gains a mode where a writer call never runs chores inline. Owed chores
are reported, and the host runs them as a check-in chore. Admission returns an owed-drain verdict,
as `flush_owed` does, instead of draining inline. The alternative is apply turns that also run as
chores, in which case T11 and T12 judge that hop. Inventory C lists the first two paths.

### F6 (R6). D3: lifetime, cancellation and the body bridge are not designed

1. **Body reads cross the wrong way.** Views run as `task::task` inside `task::as_awaitable`
   (`task_asio.hh:92-107`), the only bridge from Asio to task. RESTORE's `co_await body.read(chunk)`
   is an Asio operation, and a task cannot await an `asio::awaitable`. D3 needs the reverse bridge
   (a task awaiter over an Asio completion), named and placed in L3 or L1.
2. **No cancel token exists.** While a buffered handler runs, no read is pending on the socket
   (`http_asio.h:745-790`), so a disconnect stays invisible until the write. T9 needs http to arm
   `ConnectionInput::monitor` (`http_asio.h:458-480`, already used for deferred responses) for the
   handler's duration and to request a `cancel_source` from it. Cancellation is seen only at
   checkpoints, so a task queued on an `async_mutex` or `task::event` stays queued until its turn.
   Say so, or make the waits cancellable.
3. **Request and writer outlive their frame.** The connection coroutine owns `request` and
   `writer`. If Asio cancellation or reactor teardown destroys that frame while the detached task
   still runs, the task is left with dangling references. The streamed path disables cancellation
   and awaits completion for exactly this reason (`http_asio.h:731-736`). The `as_awaitable` path
   needs the same rule. The waiter nodes of `task::event` and `async_mutex` live in task frames
   (D5), so a frame destroyed while queued is a use-after-free. The shutdown order must be stated:
   stop accepting, request cancel on every task, let chores finish (`WorkGroup` waits off-reactor),
   resume and complete every suspended task, then stop the loops. Test: shutdown under T7's load
   on ASan and TSan builds.

### F7 (R7). D9 is out of 7c's scope (owner, 2026-10-29); these parts of the design depend on it

1. D9 itself, Q4's recommendation, and X1's deletion of `DataStorage` (section 5, step 3; Q1 (a)'s
   text; the extraction-ledger row "and `DataStorage`").
2. Inventory D's blob reads and writes and the volume commit fsync (`database/shard.cc:969`,
   `:1206-1291`, `:1317`, `:1426`, `:1515`; `handler.cc:717`, `:1793-1803`). D9 was their only
   decision, so they now have none. D7's "every fsync ... inside a chore" names only strata's
   commit and must name `storage_commit` too.
3. D12's 16 MiB buffered-body cap. Stored uploads arrive as buffered bodies today, and the owner's
   intended use for `stored` is very large files. The cap would refuse them unless the stored path
   streams the body into `DataStorage` writes run as chores.
4. T2 and T13. The store-API documents are in the E2E suite, and T13's per-open-index cost now
   includes a `DataStorage` volume.

Fix: replace D9 with "blob I/O runs as chores". `DataStorage` reads, writes and commit go to the
index's `WorkGroup` under the lease as needed, which is Q4 (b) as an interim that commits to no
format. The body cap exempts `stored` uploads and streams them instead. Remove `DataStorage` from
X1.

### F8 (C1, step 1). K5 checks counts against section 1, so it would pass with the inventory's misses

K5 is done when its counts "match section 1's totals". A grep tuned to those numbers satisfies
that, and it locks in what section 1 missed. These sites on reactor-reachable paths are not in the
inventory:

1. `engine::kind_of` (`kind.cc:59-89`): a global mutex, then two `fs::exists` stats on a cache miss.
   A path with neither directory is never cached (`:84-86`). It is reached from every
   `Schema::index` (`schema.cc:2977`) and from `Shard::kind()` (`database/shard.cc:384`).
2. `getStopWordsEntry` (`stopper.cc:51-67`): a global mutex held across reading the stop-words
   file on first use of each language. It is reached from term generation
   (`generate_terms.cc:545`) and query parsing (`engine/text.cc:295`).
3. `mime_type()` (`mime_types.cc:120`): a function-local static that reads `mime.types` (`:54`) on
   the first request, and other threads block on its initialization guard.
4. The engine census (`census.cc:139-146`): `fopen`, `fwrite` and `fflush` per line when
   `XAPIAND_ENGINE_CENSUS` is set.
5. `reader_handle::published_position()` spins and calls `std::this_thread::yield()`
   (`strata index.hh:1257-1261`).
6. strata's inline apply paths from F5.

Condition: K5's rules cover these classes (file syscalls, `std::ifstream` and `fopen`, mutexes,
`yield`, `sleep`, the condition-variable and `sync_wait` families). Every hit is classified as
allowlisted (with file:line and the reason it is O(1) to O(log n)) or owed (with the decision that
removes it). Done is "zero unclassified hits on `Xapiand:3b0dc433f`", not "counts match". Section 1
adds the sites above.

### F9 (C2, step 1). K6 cannot measure the hop's wall time with frame sampling

T11 compares the dispatch cost "in CPU and wall". The offload hop's wall time is mostly off-CPU:
the post, the futex wake, and the return post to the loop. A CPU sampler attributes none of that
to a frame at concurrency 1. Condition: K6 measures wall by instrumentation (timestamps at post,
start of run, and resume on the loop; on `Xapiand:3b0dc433f` around `http_asio.h:789`). CPU still
comes from frame samples. The dry run must show a non-zero hop share in both columns. Without
this, the pre-7c baseline step 1 records cannot serve T11.

### F10 (C3, step 1). K3's load must be fixed before the baseline is taken once

Under Q6 (read-your-writes), a light GET or search on an index that the heavy RESTORE is writing
first awaits a refresh that writes a segment. The design does not say which index the RESTORE
writes. If it writes `accounts`, T5 measures refresh I/O and the lease queue instead of reactor
scheduling, and the baseline taken in step 1 bakes that choice in. Condition: K3 fixes the
following:

1. The RESTORE writes its own index. The gating T5 run reads `accounts`. A second run with
   same-index reads is reported, as Q6's cost.
2. Open-loop latency is measured from each request's scheduled send time.
3. The achieved and target light rates are printed and asserted within 5%.
4. "Saturating light rate" (T8) is defined by a named procedure.
5. Every run records the segment count and merge backlog at its end.

### F11 (C4, step 1). K1's claims need tests, and its cost needs a number

"No allocation after start" is a claim, so K1 needs a test with a counting allocator that asserts
zero allocations after `start()`. A 1 ms probe wakes every loop 1,000 times a second, idle or not.
On the many-indexes, mostly idle host the owner targets, state that cost or make the probe idle
when the loop is idle. Log2 buckets in µs put 1 ms inside the 512 to 1,023 µs bucket and above it.
Say how T4's "p99 ≤ 1 ms" is read off them, or add sub-buckets near 1 ms.

### F12 (C5). The reactor count is capped at 10, and the design assumes one reactor per core

Section 2 draws "one per core", but `opts.cc:56-57` and `:525` cap HTTP servers at 10. Today,
request CPU runs on up to 20 offload threads (`opts.cc:59-62`). After 7c it runs only on the
reactors, so on hosts with more than 10 cores query parallelism falls. T12 ("at least 1.0x") would
then fail, or would pass only on a small host. Condition: a decision on the reactor count, against
the "Parallelism adapts" rule, and T12 run on a host with more than 10 cores, or the row states
its core count.

### F13 (C6). D8: WAL order equals apply order only if the WAL enqueue is inside the turn

The WAL entry is enqueued after the engine call in the same `Shard` operation. Examples: `add` at
`database/shard.cc:1339`, then `write_replace_document` at `:1354`; commit at `:973`, then
`write_commit` at `:993`. The enqueue goes per path to one WL queue (`wal.cc:942-951`). The writable
checkout makes that pair atomic today. Replay requires `revision == db_revision` (AGENTS). With D5's
turns, two reactors could apply in one order and enqueue in the other. Condition: the turn covers
apply, the revision update, and the WAL enqueue as one unit. T3 adds a run with writers on two
reactors into one index, then a crash and replay.

### F14 (C7). D7: coalesced commits must coalesce by position

"A commit that fails returns the error to every request awaiting it" implies requests share one
commit. A `?commit=true` write applied after the commit chore began must not join it. Condition:
a waiter joins an in-flight commit only when its position is at or below the commit's, with a
test.

### F15 (C8). D4: chore classes and queues

1. Parse-ahead shares the `flush` class and, per D4, the index's own `WorkGroup` FIFO with refresh
   and commit. A RESTORE's queued parse-ahead delays the read-your-writes refresh of every search
   on that index. Give parse-ahead its own group (or class), or put refresh-for-read at the front.
2. With strict class order, sustained parse-ahead and flushes starve merges. The research says the
   aging rule "was not needed in these runs" (`scheduler.md` R3). Under sustained bulk load the
   segment count grows. State the aging rule, and report the merge backlog (F10).
3. `reserve` defaults to 1 worker kept free of flush and merge work. 7c posts nothing at `query`,
   so that worker idles. Set it to 0, or say what runs at `query`.
4. Carry the offload review's C7 negative tests forward. V2's shape (a worker waiting on work
   queued only in its own group) must not deadlock under `WorkGroup::wait`'s helping, and V5's
   shape (a reactor waiting) must hit the guard.

### F16 (C9). D6: `too_many_readers` becomes reachable

`reader_handle::acquire` throws `too_many_readers` when every registry record is claimed
(`strata index.hh:1248-1253`). A sliced search holds its view guard across demotions and chores,
so live guards per index equal in-flight searches across all reactors. Condition: size
`index_options::readers` against the in-flight cap from F3, map the error to 503, and test it.

### F17 (C10). Smaller items for revision 2

1. Only `post()` of `asio_scheduler` is thread-safe (`task_asio.hh:40-42`, offload review C7). A
   chore's resume from a `WorkPool` worker must use it, never `post_demoted`. State this in D4.
2. `TASK_ASIO` is forced OFF today (`CMakeLists.txt:773`). D2 flips it, which is fine, and the
   comment at `:770-772` changes with it.
3. The owner's answers make some rows conditional:
   - T6 is reported at 7c and gated at Phase 6 (Q5 (b)). The row should say so.
   - T1 keeps WL and FS (Q2 (a)).
   - X1 runs first (Q1 (a)), so inventory A's three Xapian-only sites and B's six replication
     sites leave the inventory with it.

## Blocking items (for L1 to L5 and X1 onward; none for S1)

- R1: per-call Lua isolation, or a serialized per-script turn. Hook yields only when yieldable. T10
  adds an isolation case and a C-call case (F1).
- R2: the budget restarts its slice on resume, or counts only time run. The policy is named
  accurately. T5 includes light reads that await a refresh (F2).
- R3: a per-reactor in-flight admission cap alongside the demoted cap. T8 covers light and heavy
  overload and asserts 503s (F3).
- R4: every lease operation, timer commits and idle closes included, enters the turn. The registry
  has a close protocol. A task holds one turn at most, or turn waits have a deadline (F4).
- R5: strata's inline writer calls are I/O-free (chores and drain owed to the host) or apply turns
  run as chores. L5's scope grows to match (F5).
- R6: the task-to-Asio body bridge, the cancel source from the connection monitor, the
  request/writer lifetime rule, and the shutdown order, with an ASan and TSan shutdown test (F6).
- R7: D9 is replaced by "blob I/O in chores". X1 keeps `DataStorage`, D7 names `storage_commit`,
  and the body cap exempts and streams `stored` uploads (F7).

## Conditions

- C1 (S1, K5): classify every hit, with the rule set widened to the classes in F8. Done is zero
  unclassified hits, not matching section 1's counts (F8).
- C2 (S1, K6): the hop's wall time is measured by timestamps, and the dry run shows a non-zero hop
  share in CPU and wall (F9).
- C3 (S1, K3): the RESTORE writes its own index, with a same-index run reported. Latency is open
  loop from the scheduled send time. Achieved rate is asserted. The saturating rate is defined.
  Segment count and merge backlog are recorded (F10).
- C4 (S1, K1): a test asserts no allocation, the idle cost is stated, and the reading at 1 ms is
  defined (F11).
- C5: a reactor count decision, and T12 on a host with more than 10 cores or with its core count
  stated (F12).
- C6: the turn covers apply, the revision update and the WAL enqueue. T3 adds a two-reactor crash
  case (F13).
- C7: commits coalesce by position (F14).
- C8: parse-ahead gets its own queue, the aging rule is stated, the reserve is decided, and the
  V2 and V5 negative tests carry forward (F15).
- C9: `readers` is sized, and `too_many_readers` maps to 503 with a test (F16).
- C10: F17's three items.

## Owner questions (a view, not a ruling)

- Q1, Q2, Q5, Q6: answered by the owner. The design should fold them in (F17), and Q6 (a) makes
  F2 and F10 load-bearing.
- Q3 (open): (a) is right. Detent's distributor depends on the offload path, and F3 shows that an
  in-flight admission gate is needed anyway, so http and reactor keep `OffloadGate` as admission
  even after the offload pool goes. When Detent migrates, remove only the pool and keep the gate.
- Q4 (owner deciding; D9 out of scope): the design should not presume an answer. "Blob I/O in
  chores over the existing `DataStorage`" (F7) makes 7c's blob path non-blocking without choosing
  a format. That leaves a Haystack-style blob engine or a strata large-value section open for the
  owner's experiments.

Not run: no Xapiand build, server, E2E, cluster, TSan, Linux or FreeBSD run; no benchmark or
timing; no GPL source opened. The probes ran on macOS arm64 with Apple clang, ASan and UBSan.

## Re-check 1

- Date: 2026-10-29.
- Reviewed: revision 2, `Xapiand:2d8c2daab` (ledger `Xapiand:ef3f469a0`), as
  `git diff e30a844ff..ef3f469a0` (`docs/design/phase7c.md`, `docs/status/phase7c.md`).
- Checked against: R1 to R7 and C1 to C10 above; the owner's rulings in strata
  `docs/status/xapiand-on-strata.md` (Q1 to Q6, no cluster until 7b); the library code revision 2
  cites: task `task:aa447e7` (`task.hh`, `task_asio.hh`), reactor `reactor:0c04d0d` (`reactor.h`),
  http `http:6a07f99` (`http_asio.h`, `http_handler.h`), threadpool `threadpool:5caedce`
  (`work_pool.hh`), strata `strata:d93bf7d` (`core/index.hh`, `core/owner.hh`, `core/hazard.hh`),
  storage `storage:671eaac` (`storage.h`); Xapiand `Xapiand:3b0dc433f` (`database/shard.cc`,
  `lua/processor.cc`, `opts.cc`, `server/search_views.cc`).
- Checked by: five probes in `.scratch/review-7c/recheck1/`, built with ASan and UBSan by its
  `build.sh` (Apple clang, under `quiet light`), outputs beside them:
  - `lua_d10_probe.c` with `lstrlib_budget.c` (D10 as revision 2 words it, and a step-budget
    mutant of Lua 5.4.7's matcher in two placements); `lua_d10_probe.out`,
    `lua_d10_probe_reprep.out`.
  - `budget_probe_r1.cc` against `task_orig/` (`task:aa447e7` as is) and `task_slice/` (the same
    with D2's per-resume stamp in `running_on`); `budget_probe_slice.out`, `budget_probe_orig.out`.
  - `lock_until_probe.cc` (D5's `lock_until` protocol as worded, and a fix); `lock_until_probe.out`.
  - Copies read for citations: `strata-index-d93bf7d.hh`, `shard-3b0dc433f.cc`.
  - Not run: no Xapiand build, server, E2E, TSan, Linux or FreeBSD; no benchmark or timing.
    `src/xapian/`, `src/sortable_serialise.{cc,h}` and `src/server/remote_protocol_views.cc` were
    not opened. One `git grep` over `src` at `3b0dc433f` printed eight lines from
    `src/xapian/queryparser/` (the identifiers `check_infix` and `check_infix_digit`); no file
    there was opened, and later greps excluded the tree.

### Verdict

**APPROVE WITH CONDITIONS: GO for L1 to L6 and for X2 onward. R1, R2, R3, R5 and R6 are closed;
R4 and R7 are partially closed by two mechanisms that do not hold as worded (R8, R9), and each
must be fixed in the design before the library step that builds it lands (L1, L6).**

What holds, and reproduces:

- Per-call `_ENV` isolates Xapiand's own globals. `lua_d10_probe` L4, with one compiled chunk
  `local _ENV = ...; ...`, two calls in their own threads and a hook yield between them:
  `A started with _doc=doc-A, reads _doc=doc-A at its end (isolated)`.
- The per-resume slice is sound against `task:aa447e7`. Every resume already runs inside its own
  `running_on` (`task_asio.hh:49-53`, `:74-77`), and `spawn` posts the first run (`task.hh:429-432`).
  `budget_probe_r1` with the stamp: `suspended 2 ms then ~0 us of work: 0 demotion(s); ran 1 ms
  continuously: 1 demotion(s)`. Without it: `1 demotion(s); ... 1 demotion(s)`. A task that
  never suspends is still demoted, so the change does not just switch demotion off.
- Host mode closes R5's chore paths. Only `check_in()` sets `chores_owed_`
  (`strata index.hh:787-801`; other uses at `:747`, `:2570`, `:2584`), and only `checkout`'s
  destructor and `writer_scope` call it (`:769`, `:2570`). A writer call without `check_out` takes
  and releases the lease per call (`owner.hh:110-139`), so turns that move between threads never
  meet `owner_break` (`:124`). Finished merges publish in refresh and commit (`index.hh:811`,
  `:888`).
- `AdmissionGate` reuses a gate that is already lock-free (`reactor.h:99-111`), with its own
  counter beside `demoted()` and a 503 for each (design `:148-154`).

What blocks landing (not starting): R8 (`lock_until` frees a waiter node that is still reachable)
for L1, and R9 (a streamed record cannot be written through `Storage`'s compressed, 32-bit
header as specified) for L6.

### Status

- **R1 closed** (design `:302-326`, T10 `:402`). Each call gets its own `_ENV` table over a shared
  base, the hook yields only when `lua_isyieldable` holds, and T10 has the interleave case and the
  comparator case. The new bounds raise C11 to C13.
- **R2 closed** (design `:139-147`, rows `:387-388`). The per-resume stamp holds (above), the
  policy is called two-level, and T5's light mix has write-then-GET pairs that await a refresh.
- **R3 closed** (design `:148-154`, T8 `:400`). In-flight and demoted caps each have a counter and a
  503, both checked at headers complete, and T8 drives a light and a heavy overload and asserts
  503s. The bound on the io_context queue (one handle per admitted task) holds for the task chain.
  It does not cover `Reactor::track`, which C14 covers.
- **R4 partially closed** (design `:215-238`). Every lease operation is a turn, timers post a task
  that awaits the turn, the registry has `closing` with a retry rule, a task holds one turn, and
  schema writes finish first. `lock_until`'s waiter lifetime fails; see R8.
- **R5 closed** (design `:244-262`). Revision 2 never calls `check_out` (verified above), and
  `defer_io` returns owed verdicts instead of refreshing at `index.hh:691` or draining at `:705`.
  C15 bounds the retry. C16 covers views that host mode leaves unfreed.
- **R6 closed** (design `:165-186`, `:361-371`, T7 `:399`, T9 `:401`).
  - `await_asio` resumes through `scheduler_ref::post`, so the resumed run gets its own slice.
  - The handler await disables Asio cancellation, as the streamed path already does
    (`http_asio.h:735`), and frames end only by completing.
  - D16's order uses calls that exist: `abort_all` (`reactor.h:145`), `track` (`:132`) and
    `wait_sessions` (`:159`, on the main thread).
  - C14 and C17 add a shutdown bound and two monitor details.
- **R7 partially closed** (design `:269-272`, `:282-300`, X1 `:473`). `DataStorage` stays,
  `storage_commit` runs first in the commit chore, buffered blobs are written in a chore inside
  the turn, and reads run as chores. The streamed write fails as specified; see R9.
- **C1 closed** (K5 `:456-459`, D14 `:353-355`). There are four classes, done means zero
  unclassified hits, and inventory J is among them.
- **C2 closed** (K6 `:460-464`). Wall time comes from timestamps around the offload `co_spawn` on a
  `trace/hop` branch, and the dry run shows a hop share in both columns.
- **C3 closed** (K3 `:444-453`, load `:383-389`). The RESTORE writes `restore-target`,
  `--restore-into accounts` gives the same-index run, latency is open loop, rates are asserted
  within 5%, `--saturate` is defined, and segment count and merge backlog are recorded.
- **C4 closed** (K1 `:438-441`, D14 `:348-351`). The tests cover allocation and the idle wake rate,
  and "1 ms" is read at the 1,024 µs edge with 4 sub-buckets.
- **C5 closed** (design `:155-156`, T12 `:404`). There is one reactor per CPU,
  `MAX_HTTP_SERVERS` goes, and T12 runs on a host with more than 10 CPUs and states the count.
- **C6 closed** (design `:236-238`, T3 `:395`). The turn covers the apply, the revision and the
  WAL enqueue, and T3 adds `--reactors 2`. The citation is reversed: `:1339` is the engine call
  (`engine::sync_wait(wdb->add(...))`), and `:1354` is the WAL enqueue
  (`write_replace_document`). Correct the sentence. The mechanism is unaffected.
- **C7 partially closed** (design `:272-275`). Coalescing by applied position is specified, but
  no row or done criterion names its test. Name it, for example in X2's tests: a write applied
  after a commit chore starts waits for the next commit, and a failed commit reaches only its
  joined waiters.
- **C8 closed** (design `:195-206`).
  - Parse-ahead has its own group. Lanes per class exist (`work_pool.hh:1110-1128`).
  - Aging is stated (100 ms).
  - `reserve = 1` (`:673`) now serves `query` chores.
  - V2 and V5 are in L2.
- **C9 closed** (design `:264-267`, T8 `:400`, T13 `:405`). `too_many_readers` (`hazard.hh:47`,
  `:211`) maps to 503 and is tested with 16 records. The sizing is not an upper bound: a
  multi-index search holds a guard per index and shard, so the 503 path is what bounds it. On a
  64-CPU host the formula gives 64 x 1,000 x 5 = 320,000 records, and every `free_views` scans
  all of them. T13 should give that scan's cost as a number, not only state it.
- **C10 closed** (design `:190-191`, `:135-136`, T6 `:398`, T1 `:393`, inventory `:54-63`).

### Findings

#### F18 (R8). `lock_until`: a timed-out waiter's node is freed while the queue and the timer can still reach it

D5 says waiter nodes live in task frames. It also says "the grant and the timeout settle the
waiter node with one compare-exchange; a timed-out node is skipped at handoff" (design
`:216-217`, `:232-234`). That sentence settles which side wins. It does not keep the node alive
for the loser, and that fails in both orders:

1. **The timeout wins.** The task answers 503 and its frame ends, but the node is still linked
   in the mutex's queue. The next `unlock` walks to it to skip it. `lock_until_probe A`:
   `heap-use-after-free ... READ of size 8` in `unlock`.
2. **The grant wins.** The timer's completion is already queued on the io_context, and
   `cancel()` cannot recall a completion that is already queued. The task runs its turn and ends,
   and then the queued timer handler runs its compare-exchange on the dead node.
   `lock_until_probe B`: `heap-use-after-free ... WRITE of size 4`.

The mutex is taken from every reactor and from the control loop, so both orders occur in practice.

Fix (L1): the waiter's state is a small refcounted block shared by the queue entry, the timer
handler and the frame. `lock_until_probe C` shows that both orders are then harmless. The
alternative is a doubly linked queue whose timeout path unlinks under the mutex's internal lock,
plus a timer handler that holds a reference rather than a pointer into the frame. L1's tests run
both orders under ASan and TSan.

#### F19 (R9). The streamed record write does not fit `Storage`'s record format as specified

L6 specifies `begin(size)`, `append(chunk)` and `end()` (design `:292-294`). Two constraints in
the format conflict with that:

1. **Compression.** Xapiand opens `DataStorage` with `STORAGE_COMPRESS_ZSTD`
   (`database/shard.cc:180`, `:464`). `Storage::write` compresses the whole record before it
   writes the header, and the header's `size` is the compressed size (`storage.h:854-865`). A
   streamed record cannot know its compressed size at `begin`. Reads of compressed records also
   load the whole payload (`storage.h:1000-1004`), so a compressed large blob could not stream
   back out in chunks either (design `:298-300`).
2. **Size.** `StorageBinHeader::size` is a `uint32_t` (`storage.h:355`), so a record is under
   4 GiB. Volumes are capped at 32 GiB. "Very large files" (the owner's intended use for
   `stored`) needs a stated maximum.

Two smaller points:

1. The record's payload is `serialise_strings({ct_type, raw})` (`shard.cc:1257`), so the
   streamed path must write the same length-prefixed framing.
2. A crash between the upload's last chunk and the document's apply leaves an orphan volume.
   After restart, `get_volumes_range(...).second` (`shard.cc:1254`) opens the highest volume,
   which can be that orphan, for the shared writer.

Fix (L6, X3): streamed records are written uncompressed (flags 0, which `read` already handles),
with a streaming XXH32 into the footer. A body at or above 4 GiB minus the framing gets 413. The
open path sweeps volumes that no committed locator names, or opens the shared writer at the
highest volume it owns. T2's upload case adds a 413 at the size cap.

#### F20 (C11). The non-yieldable bound fails finite sorts and allows a 10^6-instruction stall

D10 ends a call after 100 consecutive hook calls (10^6 instructions) with no yield point
(design `:313-316`). `lua_d10_probe` L6, with a finite `table.sort` and a Lua comparator:

| Elements | Result | Longest non-yieldable run |
|---|---|---|
| 5,000 | ok | 19 hook calls |
| 10,000 | ok | 45 |
| 20,000 | ok | 98 |
| 50,000 | `non-yieldable bound` | 101 |
| 100,000 | `non-yieldable bound` | 101 |

A correct script that sorts 50,000 values with a comparator now gets a 4xx, while every
non-yieldable stretch up to 10^6 instructions still runs on the reactor unsliced.

Condition (X4): give the base a `table.sort` written in Lua (a merge sort), so comparators run
yieldable and sliced. Keep the bound for the remaining C callbacks (`gsub` with a function,
`string.format` with `__tostring`). T10 adds a 100,000-element comparator sort that completes and
an infinite comparator that ends.

#### F21 (C12). The matcher's step budget must span the whole API call, not one start position

`str_find_aux` and `str_gsub` call `reprepstate` once per start position (`lstrlib.c:802`,
`:964`), and `gmatch_aux` does the same per iteration (`:844`). A budget reset there never trips
on a quadratic search. Each start position costs at most n steps, below max(10^5, 8n).
`lua_d10_probe_reprep`: `string.find(rep('a', 20000), '.-b') ... per start position
(reprepstate): ran to the end (budget never tripped; ~2e8 steps)`. With the budget set in
`prepstate`: `pattern too complex (step budget)`, and a 1,000,000-byte `gmatch` and `gsub` with a
simple pattern still pass.

Condition (X4, and the patch's drift test): the budget is set in `prepstate` and counted at each
`match` entry. The patch's test includes the quadratic `find`.

#### F22 (C13). The per-call `_ENV` does not isolate the base or the library tables

`lua_d10_probe` L5: after call B writes `_G.leak` and `string.leak`, the interleaved call A sees
both (`shared base: LEAKED`). Today the same writes persist across calls of a script anyway, so
this is not a regression. D10's sentence "interleaved calls on one script share no globals"
overstates it, though.

Condition (X4): set each env's `_G` to the env itself, and either make the library tables
read-only proxies or say that they are shared within a script's state. Two smaller items:
anchor each call's `lua_newthread` (a registry ref, released at the end), and charge the 64 MiB
cap to the state's live bytes, with that cap's attribution across interleaved calls stated.
T10 includes the `_G` case.

#### F23 (C16). Host mode leaves views that readers released unfreed until the next write

Without `check_in`, views that readers still held at a refresh or commit are freed only by the
next `refresh`, `commit` or `reclaim` (`index.hh:893`, `:906`, `:2591`, `:2619-2623`). strata's
own comment counts on `reclaim` for "an idle index" (`:899-902`). D6's idle task refreshes only
when `merges_pending() > 0` (design `:253-254`). A read-mostly index therefore keeps
merged-away segment files and their mappings until the next write or the idle close.

Condition (X2): the idle task also runs `reclaim()` as a chore under the turn on an entry with
views not yet freed (the accessor at `index.hh:1320`). A test commits under a held guard,
releases it, and sees the files reclaimed without another write.

#### F24 (C15). `defer_io`'s owed verdicts must end

`apply` refreshes at `index.hh:691` when `buffer_size() + t.ops.size() > buffered_docs_cap` or
`!fits(t)`. Both can stay true on an empty buffer (`t.ops.size()` above the cap, or
`!lane_fits(t)`), and then `:697` answers `invalid`. If `defer_io` returns `flush_owed` on that
same condition, the retry after the refresh gets `flush_owed` again. Design `:257-259` says
"retries the apply once more" and does not say what follows. The same holds for `drain_owed`
when the reclaim frees nothing.

Condition (L5): an owed verdict is returned only when a refresh or a drain can change the
outcome. That means a non-empty buffer for `flush_owed`, and pending reclaims, deletes or
unlinks for `drain_owed`. Otherwise `apply` answers `invalid` or `no_space` as today. Tests: an
oversize transaction and a full disk each end after at most one owed round.

#### F25 (C14). Shutdown waits on writes with no stated bound, and `track` is linear per request

1. **No bound on writes.** D16 step 3 lets writes finish (design `:366`), and writes take no
   token. A streamed upload or a RESTORE whose client stalls holds shutdown until http's
   inactivity timeout. A write queued on a turn holds it for up to 30 s. An upload whose body
   has not fully arrived is not yet an acknowledged write, so shutdown can cancel its body read
   and remove its volume. Condition (X6): D16 cancels unfinished body reads and states the
   bound, which is the longer of the turn deadline and a shutdown grace period. T7 measures
   it.
2. **`track` per request.** `Reactor::track` takes a `std::mutex` and prunes expired entries with
   a linear `remove_if` on every call (`reactor.h:132-141`). D16 calls it once per request
   (design `:364-365`), so each request pays an O(in-flight) scan under a lock on the reactor.
   D1 allowlists only O(1) to O(log n). Condition (L3): an intrusive per-reactor list touched
   only on the loop, with `abort_all` posted to each loop, or an allowlist entry with its
   bound.

#### F26 (C17). The disconnect monitor: EOF, and pipelined input

1. **EOF.** http's current monitor reports a disconnect only on a non-EOF error
   (`http_asio.h:472`). Revision 2 cancels reads on EOF too (design `:175`). This matches Go's
   `net/http`, which cancels a request's context when its background read hits EOF, and it is
   the only way to see a full close. L3 must still make it a separate rule for the per-request
   monitor, and leave the deferred-reply monitor's EOF rule as it is unless it changes knowingly.
2. **Pipelining.** The monitor reads one byte (`:469`). When a client pipelines the next
   request, that byte ends the monitor for the rest of the handler's run, so a later disconnect
   goes unseen. State it, or re-arm into a buffer handed back to the parser.

T9 adds a read whose client half-closes, which is now expected to cancel.

### The designer's three reversible choices

1. **EOF cancels reads but not writes: agreed.** It is the common server rule (F26). Completing
   an admitted write keeps "acknowledged means durable" simple. Its cost is that shutdown needs
   the bound in F25.
2. **Streamed uploads need `Content-Length`: agreed.** Storage's format forces it anyway: the
   header carries the size before the payload, and the record carries a length prefix (F19).
   Lifting it would need a header patched after the payload, which is a format change that Q4
   should own. A chunked body crosses the cap only after 16 MiB has arrived, so it gets its 411
   then. That is acceptable, and the changelog should say so.
3. **A patch to Lua's matcher with a drift test: agreed.** No in-process alternative exists. The
   count hook does not run inside `lstrlib.c`, and a pattern has no cheap upper bound to check
   in advance. The patch is small if it follows F21: one field, a count at `match` entry, and
   the budget set in `prepstate`. The drift test pins the tarball hash. List the patch in
   `docs/dev/patches.md` as planned.

### New blocking items

These block landing, not starting:

- R8 (blocks L1's landing): `lock_until`'s waiter state outlives both the queue and the timer
  handler (shared refcounted state, or an unlink under the mutex's lock plus a referenced timer
  handler). ASan and TSan tests cover both orders of `lock_until_probe` (F18).
- R9 (blocks L6's landing and X3's streamed uploads): streamed records are uncompressed, their
  size is capped below 4 GiB with a 413, they use the `serialise_strings` framing, and orphan
  volumes are handled at open (F19).

### Conditions

- C7 (open): name the coalescing test.
- C11 (X4): a yieldable `table.sort`; T10 adds a finite 100,000-element comparator sort (F20).
- C12 (X4): the matcher budget is per API call, set in `prepstate`; the patch test includes the
  quadratic `find` (F21).
- C13 (X4): `_G` is the env itself, the library tables are read-only or stated as shared, each
  thread is anchored, and the byte cap's attribution is stated (F22).
- C14 (X6, L3): D16 cancels unfinished body reads and states its bound; `track` becomes O(1) on
  the loop or is allowlisted (F25).
- C15 (L5): owed verdicts only when a refresh or drain can change the outcome (F24).
- C16 (X2): the idle task reclaims released views (F23).
- C17 (L3): the per-request monitor's EOF rule and its pipelining limit are stated; T9 adds a
  half-closed read (F26).
- Carried: C6's reversed citation at design `:237`; C9's registry scan cost given as a number in
  T13.

Not run: no Xapiand build, server, E2E, cluster, TSan, Linux or FreeBSD run; no benchmark or
timing. The probes ran on macOS x86_64 with Apple clang, ASan and UBSan. No GPL file was opened
(the one grep that printed `src/xapian/` lines is noted above).
