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
