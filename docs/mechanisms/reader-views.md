> Status: CURRENT (2026-10-30) reads through shared reader views as built on `phase7c/x2b-handlers-views` (phase 7c step X2b); the design is `docs/design/phase7c.md` D6 and C9, C16. Not reviewed.

# Reader views: a search holds no lock

## What

A search, a lookup by id or key, a scan, `stats`, a metadata read and `doc_count` read **one published view** of the
index (strata's `reader_handle::acquire`), not the index itself. Holding a view blocks nothing: a write, a refresh, a
commit, a merge and the idle close go on, and the view stays readable until its holder lets go. `core.mtx` and
`Pinned`, which made every search hold the index's lock for its whole run, are gone.

## How

```
   writer side (one task at a time: the index's turn)         reader side (any thread, no turn)
   ──────────────────────────────────────────────────         ─────────────────────────────────
   apply / refresh / commit / reclaim / close / reopen        Shard::view() ─ acquire ─▶ Viewed (a guard)
        │ refresh publishes a new view                              │ segments(), fields(), metadata(), last_docid()
        ▼                                                           ▼
   Core::index ── reader_handle ──▶ hazard registry ◀──────── one record per held view (shared by the host)
        └─ Core::note(): revision, committed length, work owed ──▶ atomics the control loop reads
```

1. **A view is what the last refresh published.** A host refreshes before it reads (`ShardEndpoint::fresh`, a turn and
   a query chore, only when something was applied since), so a request reads its own writes. `find_key` and the other
   reads see flushed segments, not the write buffer (the writer's own lookups, under the turn, still read the buffer).
   A metadata write marks the index dirty, so the next read refreshes and sees it.
2. **The registry bounds the readers.** `engine::ReaderRegistry` (`DatabasePool::readers()`) holds one hazard record per
   view in use, shared by every index of the host: its size is the searches the host runs at once times the shards
   each reads (`opts.num_http_servers * 1000 * opts.num_shards`). When every record is claimed, `acquire` throws
   `too_many_readers`; the adapter raises `errc::busy`, which is a **503**, and the request retries nothing.
3. **Close waits for readers.** `Shard::close` and a reopen close the index first (`Core::begin_retire`: nobody can
   acquire a new view), then wait for the holders of the old views to let go (`drain_readers`, polling on the host's
   timer with no thread held, 30 s at most, then `busy`). An index is never destroyed under a view.
4. **Figures for the threads that must not wait.** The revision, the committed length and the work owed (merges not
   published, files queued for removal, **views readers released and nothing has freed**) are atomics the owner updates
   at the end of a refresh, commit, reclaim or open (`Core::note`). The control loop's housekeeping and `do_commit` read
   them without a lock (X2-B1).
5. **Released views are reclaimed** (C16). A view a reader releases is freed by the owner's next refresh, commit or
   `reclaim`. The idle housekeeping runs `reclaim` as a merge chore when the work owed is non-zero, which includes
   retired views, so the files a merge replaced go back to the disk without another write.
6. **Without a host** (tests, the CLI) `host` is off, a read flushes the buffer itself, and the shard is for one thread
   at a time; the threaded tests act as the host, taking turns per shard and refreshing after a write.

## Why

- A search under `core.mtx` made a refresh, a commit and every write to its index wait for it; the held lock was the
  reason a long search stalled writes (the old `test_pin_holds` asserted it: writes waited until the search ended).
- Views cost one hazard record per reader and no allocation; the registry's size, not a lock, bounds the load, and the
  overload answer is the 503 the design already gives every other bounded wait.
- One snapshot per index is all a search needs. A search over several shards takes the views one after the other, so
  the shards may be at different moments (they never were across nodes); each shard's snapshot is whole.

## Limits

- Reads see the last refresh, not the buffer: `find_key` right after an `add` without a refresh finds nothing. Every
  Xapiand read goes through `engine_for_read`, which refreshes.
- A reopen or close that waits for a long search holds the index's turn meanwhile (writes wait, at most the turn's
  deadline, then 503). Idle closes only pick indexes nobody uses, so they never wait.
- Each held view keeps its segments' files on the disk; a search that runs for hours holds the space of merged-away
  segments.

## Code

`src/engine/strata/shard.cc` (`Core`, `Shard::view`, `retire`, `drain_readers`), `src/engine/strata/viewed.h`,
`src/engine/strata/search.cc`, `src/engine/open.h` (`ReaderRegistry`), `src/database/pool.{h,cc}` (the registry, `housekeeping`).
Tests: `tests/test_engine_search_strata.cc` (`test_view_holds_nothing`), `tests/test_engine_host.cc`,
`tests/test_pool.cc` (`test_a_held_search_delays_no_refresh_or_commit`, `test_readers_cap`,
`test_released_views_are_reclaimed_by_the_housekeeping`).
