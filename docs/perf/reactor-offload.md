# reactor-offload: does request work run on the reactor threads, and does it stall the loop?

> Status: CURRENT (2026-11-16), exploration on `explore/reactor-offload`, master's `src/` at Xapiand:eec77b11b.
> Nothing here changes `src/`. Harness: `harness/reactor_offload_probe.py`.

Question. Does Xapiand run CPU-heavy request work (searches, aggregations, indexing) inline on a reactor thread, where
it stalls every other connection on that loop, or does it hand that work to the `WorkPool`?

Answer, in short.

1. Searches, aggregations and the CPU of an index write run on the reactor thread of the connection. Nothing moves
   them to the `WorkPool`; this is the standing design (`docs/design/phase7c.md` D3: "HTTP handlers are coroutines;
   http has no offload path").
2. They do not hold the loop for the whole request. Each request carries a 200 us `task::budget`, and the search,
   the scan and the hit loop `co_await` it at window boundaries, so a long request is a chain of short turns
   (`docs/design/loop-scheduling.md`).
3. The `WorkPool` runs device work only: stored-blob reads, opens, refresh, flush, fsync, commit, merge, the WAL
   read, RESTORE's body parse.
4. Measured, 1 reactor, 300,000 documents: four closed-loop slow clients (an aggregation over every document, a
   100-hit wildcard, an aggregation by state) move a trivial GET's p99 from 2.3-3.1 ms to 3.7-4.2 ms and the loop's lag
   p99 from 1.5-1.8 ms to 3.1 ms. Slicing works for searches. Index writes are the exception: a RESTORE stream moves
   the trivial GET's p99 to 17-24 ms.

## Where each piece runs

A reactor is one `io_context` on one thread (`reactor/reactor.h:702`, the `rp->run()` thread). The session is spawned on
that reactor's scheduler (`reactor.h:443`) and every `co_await` resumes on it.

Search request, thread by thread:

| Step | Where | Thread |
|---|---|---|
| Session reads and parses the head, calls the handler | `http/http_asio.h:1078` (`co_await handler->handle`; the streamed-body form is `:1015`) | reactor `http<N>` |
| `SearchService::handle` builds the request and its `task::budget(200 us, light 8 slices)` | `src/server/search_views.cc:3163`, `:3179` | reactor |
| The view runs as a child task | `src/server/search_views.cc:3240` | reactor |
| `search_view` | `src/server/search_views.cc:1958` | reactor |
| `get_mset` -> `match` -> `search_batched` | `src/database/handler.cc:1252`, `:1551`, `:1602` | reactor |
| `search_batch` pins the shards, refreshes the read view (a chore when the index owes a refresh), calls the engine | `src/database/handler.cc:1470`, `:1517` | reactor; the refresh on a worker |
| `engine::search` -> `run` -> `p::search_task`: the plan is built, then the posting lists are scored | `src/engine/strata/search.cc:788`, `:730` | reactor |
| The aggregation spy: `request.spy->see(view, weight)` per match, inside the scan | `src/engine/strata/search.cc:714-719` | reactor |
| The scan's yield: a window of 256 ordinals when there is a visitor (4,096 without), then `co_await slice` | `prospect/shards.hh:190`, `:241` at the pinned prospect (CMakeLists.txt:773) | reactor |
| The hit loop (100 hits): `checkpoint()` per hit, `Shard::get`, JSON | `src/server/search_views.cc:2034` | reactor |
| A stored record's read, when it is not in memory | `src/database/shard.cc:1824`, `:1879` (`ChoreClass::query`) | `WorkPool` worker |

Index write (`PUT /index/id`):

| Step | Where | Thread |
|---|---|---|
| `write_document_view` -> `db_handler.index` | `src/server/search_views.cc:925`, `:954` | reactor |
| `prepare`: the schema fetch, `Schema::index` (fields, terms, values), `finish_prepared` | `src/database/handler.cc:484-512` | reactor |
| `replace_document_term` -> the shard's turn | `src/database/handler.cc:1992`, `:2027` | reactor |
| Blob push, commit, fsync | `src/database/shard.cc:1151` (`storage_push_blobs`), `:692`, `:1160` (`chores->run(flush)`) | `WorkPool` worker |
| The engine's apply: `core.index->apply(t)` | `src/engine/strata/shard.cc:755` (via `apply_t`, `:916`) | reactor |
| A refresh, drain or promote the index owes before it applies | `src/engine/strata/shard.cc:758` (`ChoreClass::flush`) | `WorkPool` worker |

A chore is `IndexChores::run` (`src/database/pool.cc:96-130`): the awaiter posts a `WorkItem` to its group on
`work_pool_` (`pool.cc:626`), a worker runs the function, and the coroutine resumes on the scheduler it suspended on
(`pool.cc:129`). Every `ChoreClass::query` and `flush` call site is device or index maintenance work; no scoring,
aggregation or `Schema::index` runs in one. A RESTORE body's parse runs in a chore of the request's group
(`search_views.cc:2976`, `:3040`); its prepare and apply are per object on the reactor with a checkpoint between
(`src/database/handler.cc:2503-2512`), as `docs/perf/x7-loop-stalls.md` describes.

## Measurement

Binary: master's `src/` (Xapiand:eec77b11b, RelWithDebInfo, LTO off) built in this worktree under `quiet light`.
Host: macOS, Intel i9-9980HK, 16 logical CPUs, the whole run under one `quiet strict` hold (exclusive on macOS).
Harness: `harness/reactor_offload_probe.py` starts a node with `--processors N` (N reactors plus the control loop), loads
300,000 `accounts` (the repo's 1,000 sample rows cycled with distinct ids, so term dictionaries stay small), warms each
slow query once, and runs 20 s phases:

- trivial: an open-loop `GET /accounts/<id>` at 200/s over 16 connections, latency from the scheduled time, first 2 s
  dropped (3,600 samples per phase);
- trivial2 and trivial3: the same again, so the first, the middle and the last give the noise floor (the A/A);
- mixed, mixed2: the trivial stream plus 4 closed-loop clients cycling `agg_age` (`_terms` on `age`, `_query: *`, every
  document), `wildcard` (`favoriteFruit` `ba`, 100 hits) and `agg_state` (`_values` on `contact.state`, `gender: male`);
- slow-alone: the 4 slow clients only (the slow queries' own latency);
- restore: the trivial stream plus one client RESTOREing 5,000-document chunks into another index back to back.

The lag column is the loop's own `xapiand_reactor_lag_seconds` (a 1 ms probe timer; lateness is how long a turn held the
loop), delta per phase, percentiles as bucket upper edges. Results: `.scratch/results/p{1,2}.json` (not committed; the
tables below are the numbers).

### 1 reactor (`--processors 1`): trivial GET latency, ms

| Phase | p50 | p99 | p99.9 | max | loop lag p50 / p99 / p99.9, ms |
|---|---:|---:|---:|---:|---|
| trivial | 1.49 | 3.05 | 14.4 | 18.8 | 0.32 / 1.79 / 3.07 |
| trivial2 (A/A) | 1.44 | 2.30 | 4.08 | 7.7 | 0.32 / 1.54 / 3.07 |
| trivial3 (A/A, after the slow phases) | 1.41 | 2.39 | 4.37 | 6.1 | 0.32 / 1.54 / 3.07 |
| mixed | 1.67 | 4.24 | 25.3 | 35.0 | 0.45 / 3.07 / 5.12 |
| mixed2 | 1.66 | 3.72 | 5.42 | 8.8 | 0.45 / 3.07 / 5.12 |
| restore (index writes) | 1.75 | 24.2 | 40.6 | 50.1 | 0.32 / 3.58 / 16.4 |

The A/A spread is 0.8 ms at p99 (2.30-3.05), 10 ms at p99.9 (one early outlier). With slow queries: p50 +0.2 ms, p99
+1.4 to +1.9 ms over the A/A median, loop lag p99 1.5-1.8 ms to 3.07 ms. The slow queries' own latency with 4 clients on
one loop: `agg_age` p50 784 ms (a 186 ms query alone), `agg_state` 387 ms, `wildcard` 70 ms; the node used 19 CPU-seconds
in 20 s, one core.

### 2 reactors (`--processors 2`), ms

| Phase | p50 | p99 | p99.9 | max | http0 lag p50 / p99 / p99.9 |
|---|---:|---:|---:|---:|---|
| trivial | 1.26 | 1.80 | 2.87 | 5.4 | 0.32 / 1.28 / 2.56 |
| trivial2 (A/A) | 1.23 | 1.73 | 2.60 | 5.6 | 0.32 / 1.28 / 1.79 |
| trivial3 (A/A) | 1.28 | 1.78 | 2.30 | 4.5 | 0.32 / 0.45 / 1.28 |
| mixed | 1.31 | 2.39 | 3.48 | 5.0 | 0.38 / 1.79 / 2.56 |
| mixed2 | 1.32 | 2.45 | 3.33 | 4.2 | 0.38 / 1.79 / 2.56 |
| restore (index writes) | 1.19 | 17.5 | 30.8 | 44.9 | 0.16 / 1.54 / 16.4 |

`http1` is within one bucket of `http0` in every phase. Slow queries move p99 by +0.65 ms over the A/A. The node used 38.6
CPU-seconds in 20 s, both loops saturated by 4 slow clients. RESTORE ran 3,872 docs/s on 1 reactor and 6,403 on 2.

Each loop's `max_us_window` shows 49-82 ms in every phase of `http0`, the trivial-only ones included, so that figure is
the probe's floor here (the scrape's own turn or a periodic task), not a result of the slow queries.

## Reading

- Searches and aggregations run on the reactor and share its core with the trivial GETs, but the 200 us budget and the
  256-ordinal scan window keep each turn short: the loop-lag p99 doubles (1.5-1.8 to 3.1 ms on one loop) and a trivial
  GET's p99 costs 0.65 ms (2 loops) to 1.4-1.9 ms (1 loop) more. That is the price of sharing a core, and it is bounded.
- What offload would buy is the core, not the latency tail: with 1 reactor the slow queries take the loop's whole CPU
  (agg_age p50 784 ms for a 186 ms query), and the trivial GETs' p99 stays within 4.3 ms only because the lanes order
  them first (`loop-scheduling.md` D1).
- Index writes are not sliced finely enough: a RESTORE client moves the trivial p99 from about 2.3 to 17-24 ms and the
  loop's lag p99.9 to 16 ms on both loops. The turn that holds the loop is a single object's prepare and apply (about
  0.5 ms of loop wall per document by `x7-loop-stalls.md`) plus whatever the chunk does between checkpoints; this
  exploration did not attribute the 16 ms. Candidates: the per-batch commit, a 5,000-document chunk's body decode and
  parse hand-off (`Request::decode_large_body`, `search_views.cc:2853`), and the shard turn.

## Sketch: moving a search onto the WorkPool (not built)

A search turns heavy only on the aggregation and big-scan path, and it is already bounded, so the offload is justified
only if the loop's CPU, not its latency, becomes the limit. The smallest change keeps the coroutine on the reactor and
runs the scoring in one chore:

1. `src/engine/search.h`: `SearchRequest` gains `engine::Chores* chores` (null: today's path).
2. `src/database/handler.cc` (`match`, near `:1579`, where `request.budget = budget` is set): set
   `request.chores` to the index's chores, or the pool's query group.
3. `src/engine/strata/search.cc:730`: when `request.chores` is set, replace
   `co_await p::search_task(snapshot, build, options, slice)` with
   `co_await request.chores->run(ChoreClass::query, [&] { found = p::search(snapshot, build, options); })`.
   Prospect already has the blocking `search(multi, build, o)` (`prospect/shards.hh:316`, `search_task` run to
   completion on the calling thread). The snapshot, the plan and the callbacks are locals of the suspended frame, so
   they stay alive; the spy and `SegmentDocView` are touched by one thread at a time (the frame is suspended);
   `current_bucket_budget` is already thread-local (`src/aggregations/aggregations.h:54`); `CallbackGuard` in this file
   must be checked for thread affinity.
4. Cancellation: the blocking search does not poll the request's token, so a client that disconnects mid-aggregation is
   noticed at the chore's end. Pass a `std::atomic_bool` (the token's flag) into `search_options` or keep the slice
   with a worker-side `exhausted()` check.
5. The hit loop after the search (`search_views.cc:2034`) stays on the reactor with its checkpoints; `Shard::get` is
   already sliced.
6. Test: `tests/test_loop_yield.cc` style, a search whose chore runs on a thread other than the loop's, and a mutant
   with `chores = nullptr` that runs on the loop.

Cost and conflicts. A heavy search holds a worker for its whole length, so the pool's `threads()` and its DRR shares
(`work_pool.hh`) become the concurrency limit for heavy searches. `phase7c.md` D3 deletes "request CPU off the reactors"
on purpose (the per-core design, one loop per CPU), so this reverses a ruling and needs the owner's decision, and a
measurement first: a loaded node where the loops are saturated and the trivial p99 misses with slices on. This run does
not show that. The write side is already designed: `docs/perf/x7-loop-stalls.md` ("RESTORE's prepare as an index
chore") moves `Schema::index` and the record work to the `WorkPool` and keeps the apply on the loop.

## Limits of this run

- One macOS host, one dataset shape (1,000 rows cycled), so wildcard expansion is cheap and a many-term wildcard or a
  high-cardinality aggregation was not measured.
- The generator is one Python asyncio thread; it is the same in the A/A and the mixed phases.
- Three trivial phases per arm, each 3,600 samples: p99.9 and max are single events.
- 5,000-document RESTORE chunks, not the 20,000 of the x7-gate harness.
- Linux and arm64 not measured.
