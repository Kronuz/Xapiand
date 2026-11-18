# Wide searches off the HTTP loops' critical path

> Status: DRAFT (2026-11-18) for quest wide-search-loop (x7-gate SR3, security S2). Not reviewed. Builds on
> `docs/mechanisms/wide-search.md` (batches of K indexes) and `docs/design/loop-scheduling.md` (the heavy lane).

## Summary

A search over thousands of indexes stalls an HTTP loop for 49-100 ms at 2,500 indexes and up to 393 ms at 10,000
(x7-gate SR3), and 64 clients sending such searches by name list push every loop's lag into the 1-2 s bucket, with a
light GET's p99 at 86.9 ms (security S2). This note attributes the stall and chooses the bound.

## Attribution

A reactor watchdog (`harness/wide_search_reactor_stalls.patch`, env `XAPIAND_LAG_SPIN=20000`) signals a loop whose
turn passes 20 ms and walks its stack every 1.5 ms while it stays stalled; `harness/wide_search_stalls.py` symbolizes
the captures. Job 261118-043331-286e1f, master's `src`, 2,000 indexes for S2 and 2,500 for SR3, macOS x86_64:

- **SR3, one client.** The loops stall in `IndexResolverLRU` resolving the search's names: each name misses the
  resolver's cache (`--resolver-cache-size`, 100 entries, against 2,500 names), so each loads its settings document
  from `.xapiand/indexes` and decodes it on the loop (`load_settings`, `decode_stored_frame`, `decompress_zstd_into`).
  `resolve_index_endpoints` in `search_views.cc` awaits the names one after another and nothing in the chain
  suspends, so the whole list is one turn: 2,500 names hold a loop 87-91 ms (job 261118-022616-80ea60), 10,000 up
  to 393 ms (x7-gate). The batches' pins and searches are not where the loops stall: no capture holds
  `search_batch` or `strata::search`.
- **S2, 64 clients.** The same misses, taken by 16 loops at once, meet on the resolver's one `std::mutex`
  (`resolve_index_lru_mtx`, taken 3 to 5 times a name: `resolve_index_settings`, `cached_settings`,
  `store_settings`). A loop blocked on it is a thread blocked: of 19,659 captures, 9,000 hold the resolver's lock
  paths and most of the rest end in `__psynch_mutexwait`. Every loop's lag reaches the 1.3-2.6 s buckets and a light
  GET's max 1.8-2.9 s.

So the stall is per-name resolution, not per-index work, and concurrency turns its cost into lock contention.

## Decisions

**D1. The name resolution yields between slices.** `wide_search::resolve` resolves a search's names one at a time
with a `task::budget` checkpoint before each, the request's budget: after `REQUEST_LIGHT_SLICES` slices the search
runs in the heavy lane (loop-scheduling D2), so a light request on the same loop waits at most one slice behind it.

**D2. A batch's pins and engine reads yield between shards.** No capture holds them today; the checkpoints keep a
batch of K indexes K short steps when K is large (`--concurrent-search-indexes`, or the pool at 5,000, where x7-gate
measured 115-131 ms p99.9 with nothing reopening). `DatabaseHandler::search_batch` checkpoints the request's budget before each shard it pins and each engine it
refreshes.

**D3. A node-wide bound on the indexes wide searches pin at once.** `wide_search::Pins` holds the pool's soft_open
(what the pool keeps open): a batch of a search wider than one batch takes its indexes' count before it pins them and
gives it back when it ends. A batch that does not fit waits its turn, first come first served, holding no thread; a
batch wider than the bound takes all of it, alone. Searches that fit in one batch do not take a place: they hold at
most K indexes each and are bounded by admission (1,000 in flight per loop), as every light request is.

Why the pool's soft_open: one wide search per loop holds K = soft_open / loops, so the bound is what the loops held
together before, now a ceiling instead of a sum that grows with the clients (S2: 64 searches each held their batch).
The alternative, a count of wide searches at once, bounds the same thing less precisely: a batch is whole indexes, so
its width varies with the shards per index.

## Measured (probe build: master `src` plus the call-site patch)

| Row | master | probe | Jobs |
|---|---|---|---|
| SR3, 2,500 indexes: http loops' window max | 131-229 ms buckets (87-91 ms in 80ea60) | 640 / 1,536 us buckets (max since start 624 / 1,305 us) | 043331-286e1f, 043334-e2a804 |
| SR3, 10,000: http loops' max since start | 395 / 475 ms | 6.6 / 9.2 ms | 043346-9d63ea, 045057-0e3792 |
| SR3, 2,500: p99 lag, search p50 | 640-768 us, 10.7 s | 384-448 us, 1.28 s | same |
| S2, 64 clients by names: light GET p99 / max | 83.8 / 1,819 ms | 5.3 / 26.3 ms | same |
| S2, 64 by names: loops' window max | 1.57 s buckets | 115-131 ms buckets | same |
| Stall captures (>= 20 ms), S2 phase | 19,659 | 308 | same |

The host was shared (strict holds beside light ones) and moved between runs (alone p99 0.8-2.5 ms), so these are the
probe's direction, not the gate's numbers: the branch's rows below replace them.

## Branch measured (call sites in, Xapiand:8e5040da8 src; master src = the same tree with 8e5040da8 reverted)

Both arms run binaries with the same reactor (the stall sampler inert). Exclusive holds are the host alone.

| Row | master | branch | Jobs |
|---|---|---|---|
| SR3 10,000, exclusive: http loops' max since start | 322 / 373 ms | 2.07 / 2.07 ms | 051433-c76a1c, 051434-89f2f5 |
| SR3 10,000, exclusive: http loops' p99 | 384 / 448 us | 1,536 / 2,048 us | same |
| SR3 10,000, exclusive: search p50, first search | 4.8 s, 16.3 s | 5.5 s, 6.1 s | same |
| SR3 10,000, strict: http loops' max since start | 318 / 634 ms | 2.06 / 2.10 ms | 051431-0d7e48, 050300-6ee958 |
| S2 64 by names, strict: light GET p99 / max | 1.28 / 1,140 ms | 1.69 / 3.39 ms | 051429-5d9b36, 050258-4c6f40 |
| S2 64 by pattern, strict: light GET p99 / max | 17.6 / 1,674 ms | 1.86 / 18.7 ms | same |
| S2 64: loops' window max | 1.5-2.1 s buckets | 5-6 ms buckets | same |

The control loop, which runs no search, sits at a 2,048-2,560 us p99 in every run, and the http loops of the
probe's idle phase at 2,048 us: an idle loop's wake on this host (macOS x86_64) is late by up to 2.5 ms. The
branch's remaining max at 10,000 is that floor: of the late beats over 2 ms at 10,000 (job 050303-e61fcf, a 250 us
beat on every loop), 40 of 42 are off CPU (the thread preempted), the other two the node's start; with the beat
keeping the loops awake their p99 is 512 us. One reading disagrees: master's exclusive run has its http loops at a
384-448 us p99, so whether the branch's 1.5-2 ms p99 is the idle wake or many short turns is decided by the
exclusive A/B with the beat on (jobs in the journal). The resolver's `std::mutex` is still taken on the loops; at
64 clients the branch meets T5, so it stays.

Two hypotheses measured and dropped:

- A resolver cache sized past the search (`--resolver-cache-size 100000`) changes nothing measurable: SR3 10,000
  max 3.5 / 2.9 ms against 2.06 / 2.10 ms, S2 names light p99 1.65 against 1.69 ms (050301-091601, 050259-a1757e).
  The misses' cost is sliced, so `src/index_resolver_lru.*` stays as it is.
- The 6.6-9.2 ms turns of the probe build at 10,000 (045057-0e3792) were the probe patch's; the branch's call sites
  do not show them (above).

## search_batch for concurrent pins (cheap-reopen D3)

A change that pins a batch concurrently keeps three things of `DatabaseHandler::search_batch`:

1. The hold first: a search wider than one batch takes `wide_search::pins().take(end - begin, token)` before any pin,
   and keeps it until the batch's searches end. Concurrent pins never exceed the hold, so the node-wide bound stands.
2. A budget checkpoint per shard on the loop: launch the pins from a loop that checkpoints before each launch (and
   before each `engine_for_read`), so a batch of K never runs K launches or K resumptions in one turn.
3. Cancellation through the request's token: a batch waiting in the line leaves it when its request is cancelled;
   pins already started are released by their `lock_shard`s.

## Limits

| Limit | Value | Mechanism | Reason |
|---|---|---|---|
| Indexes wide searches pin at once | the pool's soft_open (`--database-pool-size`, 200) | `wide_search::Pins`: a FIFO line (`task::async_mutex`, cancellable) whose head waits for room on a `task::event`; room comes back as batches end | the pinned set must have a number whatever the clients send (S2) |
| Loop time between yields, a wide search | one slice (`REQUEST_SLICE`) plus one name's resolution or one shard's pin | a budget checkpoint per name and per shard | T4's 1 ms p99 lag on every loop |

The head of the line is not cancellable while it waits for room: room comes back as running batches end, which needs
nobody else, so its wait is bounded by one batch of every running search. A batch behind the head leaves the line when
its request is cancelled.

## Call sites

`resolve_index_endpoints` in `src/server/search_views.cc` resolves a search's names with `wide_search::resolve`;
`DatabaseHandler::search_batch` in `src/database/handler.cc` takes the batch's pins and checkpoints per shard.

## Tests

`tests/test_wide_search.cc`: the bound (a batch that fits takes its count at once and gives it back, a batch wider than
the bound takes all of it), the line (a waiting batch keeps its place: a narrower one does not pass it), and the
cancellation of a batch behind the head. `harness/wide_search_check.py gate` reads SR3 (each loop's lag over the timed
searches, `--sample` the call graph); `harness/wide_search_starve.py` is S2's probe with T5's bound as its verdict.
