> Status: CURRENT (2026-10-30) reads through shared reader views as built on `phase7c/x2b-handlers-views` (X2b) and `phase7c/x3-views` (X3: the admission cap, the lock-free acquire, the wait rule); the design is `docs/design/phase7c.md` D6 and C9, C16. Not reviewed.

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
   Core::index ── reader_handle ──▶ hazard registry ◀──────── one record per held view (this index's own)
        └─ Core::note(): revision, committed length, work owed ──▶ atomics the control loop reads
```

1. **A view is what the last refresh published.** A host refreshes before it reads (`ShardEndpoint::fresh`, a turn and
   a query chore, only when something was applied since), so a request reads its own writes. `find_key` and the other
   reads see flushed segments, not the write buffer (the writer's own lookups, under the turn, still read the buffer).
   A metadata write marks the index dirty, so the next read refreshes and sees it.
2. **Each index bounds its readers, and the bound is an admission.** An index's `Core` owns a hazard registry
   (`readers_per_index` records, each a cache line) and a `reactor::AdmissionGate` of the same size. A view in use holds
   one record of the index it reads and one admission, which `Viewed` returns when it is released. A search of many
   indexes takes one of each in each index and shares none, and a commit scans only its own index's records. A view past
   the cap is refused by the gate before it takes a record (`Shard::views_refused()` counts them): the adapter raises
   `errc::busy`, a **503**, and the request retries nothing: more searches hold that index at once than it admits, which
   is overload. The registry's own `too_many_readers` maps to the same 503 and cannot happen while the gate holds.
   The handle that lends the views (`Core::readers`) is a plain atomic pointer: `view()` takes no lock and does no
   shared read-modify-write (a `shared_ptr` load would take libc++'s spinlock pool and count on one control block). The
   owner keeps every handle it ever published until the `Core` ends, so a pointer a reader loaded before a reopen stays
   valid; a handle of a replaced index raises `index_closed`.
   The registry outlives every guard on it: a reader that lets go after a drain that timed out (the shard dropped
   while the view is held) writes its record back into the registry, so a `Core` destroyed with a record still
   claimed keeps the registry (`strata::detail::keep_forever`) as the index keeps the view.
3. **Close waits for readers, off the turn.** `Shard::close` and a reopen close the index first (`Core::begin_retire`:
   nobody can acquire a new view), then wait for the holders of the old views to let go (`drain_readers`, polling on the
   host's timer with no thread held, 30 s at most, then `busy`). An index is never destroyed under a view. The registry
   runs the two halves apart (`begin_close` under the turn, `finish_close` after it released the turn), so the drain holds
   no turn; the entry is `closing`, so whoever gets the turn next finds the entry unusable and awaits the close.
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
- Views cost one hazard record per reader and no allocation; the index's registry size, not a lock, bounds the readers
  of one index, and the overload answer is the 503 the design already gives every other bounded wait.
- One snapshot per index is all a search needs. A search over several shards takes the views one after the other, so
  the shards may be at different moments (they never were across nodes); each shard's snapshot is whole.

## Limits

- **Readers per index.** `readers_per_index` (`manager.cc`) is 16 searches per reactor, plus twice the pool workers and
  the control loop (a search holds its view while it waits demoted for its next slice, so searches on one index
  overlap on a reactor; the 16 is an estimate; the pool and control terms are doubled because a read that runs under a
  view, a scan's callback or a spy, may take a second view of the same index on its thread). On a 16-CPU host with the
  default 10 reactors that is 10 x 16 + 2 x 17 = **194 records**, 12,416 bytes per open index (24,832 on arm64, where a
  record is 128 bytes), allocated when the index opens, so a pool of 200 open indexes holds 2.5 MB of records and 10,000
  open indexes would hold 124 MB. The chunked registry (X3-1) makes this zero until the first reader and lets the bytes follow the load.
- **Scan cost.** A refresh, a commit or a reclaim of an index scans that index's records: 0.024 us at 64 records and
  0.116 us at 256 (the architect's probe, x86_64, idle), about 0.08 us at 174, up to twice that when the scan also
  stamps the diagnostics. It does not grow with the other indexes or with the searches running on them.
- **Indexes per search.** A search keeps every index it reads open until it ends, and the pool's open-index budget is
  sized from the file-descriptor limit. `max_indexes_per_search` (`--max-indexes-per-search`) caps the shards one
  search may name, counted after wildcard and comma expansion and checked before any view is taken (`DatabaseHandler::match`).
  It defaults to the pool size and never exceeds it (`main.cc`, after the descriptor computation); a search over more
  answers **400** naming the limit, since retrying it cannot help. The per-index cap above stays a 503.
- Reads see the last refresh, not the buffer: `find_key` right after an `add` without a refresh finds nothing, and
  neither do `stats`, `doc_count` or `declared_fields`. A caller of a `view()` read refreshes first; every Xapiand
  read goes through `engine_for_read`, which does.
- A reopen or close that waits for a long search holds no turn meanwhile; writes and refreshes find the entry
  `closing` and wait for the close (the request's deadline, then 503). Idle closes only pick indexes nobody uses, so
  they rarely wait.
- Each held view keeps its segments' files on the disk; a search that runs for hours holds the space of merged-away
  segments. A held view also keeps its index in the housekeeping set (one extra turn per tick) until it ends.

## The rule for a request that holds a view

A request takes its views after every await that needs a turn: its `fresh()` refreshes and the opens. While it holds
a view it awaits nothing that takes a turn, opens an index or waits for a drain. A search that held A's view and
awaited A's turn could meet a close or reopen that waits for A's readers, a cycle that only the drain's timeout ends.
The rule makes the cycle unrepresentable, and two things keep it so.

- **Structure.** `engine::search` takes its views and then awaits only the budget's checkpoint (`run` is a coroutine, and
  the only suspension in the search is the slice), so a search holds views across a yield and across nothing else. `get`,
  `find_key`, `scan` and the other `Viewed` readers hold theirs across no `co_await`.
- **A check in debug builds.** Each request's task runs through its own scheduler (`engine::Run`), whose resumes make the
  request's `TaskCounters` the running ones. `Viewed` counts the views the running task holds, and `take_turn`,
  `ensure_open` and `drain_readers` call `engine::check_no_view_held`, which aborts (a test replaces the abort with a
  throw) when the count is not zero. Release builds compile the check out; a task on a blocking wait
  (`engine::sync_wait`) is counted the same way.

## Code

`src/engine/strata/shard.cc` (`Core`, `StrataReaders`, `Shard::view`, `retire`, `drain_readers`, `begin_close`, `finish_close`), `src/engine/strata/viewed.h`, `src/engine/host.h` (`TaskCounters`, `check_no_view_held`), `src/engine/host_asio.h` (`AsioHost`, `Run`),
`src/engine/strata/search.cc`, `src/engine/open.h` (`default_readers_per_index`), `src/database/pool.{h,cc}` (`readers_per_index_`,
`housekeeping`), `src/database/handler.cc` (`check_search_fanout`), `src/manager.cc` (`readers_per_index`).
Tests: `tests/test_reactor_requests.cc` (a budgeted search shares its loop, a cancelled one stops, the cap counts, the handle is read while the index is replaced, the wait rule), `tests/test_engine_search_strata.cc` (`test_view_holds_nothing`), `tests/test_engine_host.cc`
(`test_a_full_registry_answers_busy`, `test_a_view_outlives_a_drain_that_timed_out`), `tests/test_pool.cc`
(`test_a_held_search_delays_no_refresh_or_commit`, `test_readers_cap`, `test_readers_cap_is_per_index`,
`test_a_search_over_more_indexes_than_records_is_served`, `test_a_close_drains_its_readers_without_the_turn`, `test_search_fanout_limit`,
`test_released_views_are_reclaimed_by_the_housekeeping`); `harness/search_fanout_check.py` (the limit over HTTP).
