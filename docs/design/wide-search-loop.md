# Wide searches off the HTTP loops' critical path

> Status: DRAFT (2026-11-18) for quest wide-search-loop (x7-gate SR3, security S2). Not reviewed. Builds on
> `docs/mechanisms/wide-search.md` (batches of K indexes) and `docs/design/loop-scheduling.md` (the heavy lane).

## Summary

A search over thousands of indexes stalls an HTTP loop for 49-100 ms at 2,500 indexes and up to 393 ms at 10,000
(x7-gate SR3), and 64 clients sending such searches by name list push every loop's lag into the 1-2 s bucket, with a
light GET's p99 at 86.9 ms (security S2). This note attributes the stall and chooses the bound.

ATTRIBUTION

## Decisions

**D1. The name resolution yields between slices.** `wide_search::resolve` resolves a search's names one at a time
with a `task::budget` checkpoint before each, the request's budget: after `REQUEST_LIGHT_SLICES` slices the search
runs in the heavy lane (loop-scheduling D2), so a light request on the same loop waits at most one slice behind it.

**D2. A batch's pins and engine reads yield between shards.** `DatabaseHandler::search_batch` checkpoints the request's
budget before each shard it pins and each engine it refreshes, so a batch of K indexes is K short steps, not one turn.

**D3. A node-wide bound on the indexes wide searches pin at once.** `wide_search::Pins` holds the pool's soft_open
(what the pool keeps open): a batch of a search wider than one batch takes its indexes' count before it pins them and
gives it back when it ends. A batch that does not fit waits its turn, first come first served, holding no thread; a
batch wider than the bound takes all of it, alone. Searches that fit in one batch do not take a place: they hold at
most K indexes each and are bounded by admission (1,000 in flight per loop), as every light request is.

Why the pool's soft_open: one wide search per loop holds K = soft_open / loops, so the bound is what the loops held
together before, now a ceiling instead of a sum that grows with the clients (S2: 64 searches each held their batch).
The alternative, a count of wide searches at once, bounds the same thing less precisely: a batch is whole indexes, so
its width varies with the shards per index.

## Limits

| Limit | Value | Mechanism | Reason |
|---|---|---|---|
| Indexes wide searches pin at once | the pool's soft_open (`--database-pool-size`, 200) | `wide_search::Pins`: a FIFO line (`task::async_mutex`, cancellable) whose head waits for room on a `task::event`; room comes back as batches end | the pinned set must have a number whatever the clients send (S2) |
| Loop time between yields, a wide search | one slice (`REQUEST_SLICE`) plus one name's resolution or one shard's pin | a budget checkpoint per name and per shard | T4's 1 ms p99 lag on every loop |

The head of the line is not cancellable while it waits for room: room comes back as running batches end, which needs
nobody else, so its wait is bounded by one batch of every running search. A batch behind the head leaves the line when
its request is cancelled.

## Call sites

The two call sites are outside this quest's files (`src/server/search_views.cc`, `src/database/handler.cc`):
`harness/wide_search_callsites.patch` carries them, and the probe jobs build with it applied.

## Tests

`tests/test_wide_search.cc`: the bound (a batch that fits takes its count at once and gives it back, a batch wider than
the bound takes all of it), the line (a waiting batch keeps its place: a narrower one does not pass it), and the
cancellation of a batch behind the head. `harness/wide_search_check.py gate` reads SR3 (each loop's lag over the timed
searches, `--sample` the call graph); `harness/wide_search_starve.py` is S2's probe with T5's bound as its verdict.
