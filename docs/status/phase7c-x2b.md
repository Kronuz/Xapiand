> Status: CURRENT (2026-10-30) the X2b developer's ledger (phase 7c step X2b: the handler, views and schema callers as tasks, and the reader views); reviewed (approve with conditions), the architect's ruling R1 to R4 folded in, not pushed.

# phase7c X2b (build): handlers and views as tasks, reader views

Branch `phase7c/x2b-handlers-views` in `Xapiand-wt/x2b` (local only, from `phase7c/x2-registry-lease` at `Xapiand:7951486a9`, rebased onto the
X2 review `Xapiand:db50b90b3`). Design: `docs/design/phase7c.md` (D2, D3, D5, D6, C9, C16); X2's ledger: `docs/status/phase7c-x2.md`; X2 review:
`docs/reviews/phase7c-x2-code.md`. Mechanism notes: `docs/mechanisms/reader-views.md`, `docs/mechanisms/index-registry-and-lease.md`.

## State

Code review done (`docs/reviews/phase7c-x2b-code.md`); the architect's ruling on the reader registry (R1 to R4) is implemented and proved below, ready for
the lead to confirm and merge. K1 to K6 are ticked below with their evidence. Nothing is blocked. What X2b could not convert needs L3 (X3) or the Lua yields (X4) and is listed under "Needs L3".

## Done criteria, ticked

- [x] **K1. No `sync_wait` on a server path.** `grep -rn sync_wait src/` outside `src/engine/` lists, in non-test code:
  `search_views.cc:2988` (the one bridge: the library's synchronous `handle` waits for the view's task on an offload worker), `handler.cc` x5
  (`DocPreparer`, `DocIndexer` and `~DocIndexer`, threads of the bulk indexer), `lua/processor.cc:267` (a script's foreign read), `main.cc` x2 (the CLI),
  and `pool.cc` x3 and `database/shard.cc` x8 (shutdown on the main thread, a detached task with no loop, and the open chore's WAL replay: X2's
  allowed rows). `handler.cc` (33 sites) and `schema.cc` (2) no longer wait on the engine for a request; the views, the schema cache
  (`SchemasLRU`), the resolver (`IndexResolverLRU`, `XapiandManager::resolve_index_*`), `Schema::index` and `Document` are tasks.
  `harness/async_check.sh --strict`: **0 unclassified** (252 hits: 174 allowed, 65 owed, 13 compiled out), exit 0. The 5 handler sites and the
  bridge are owed to X3 (D12, D3), the Lua site to X4 (D10); each runs on a thread that is not a reactor, behind `check_may_block`.
- [x] **K2. Reader views replace `core.mtx` and `Pinned`.** `Shard::view()` returns `Viewed`, one hazard-guarded view; the 27 `core.mtx` sites and
  `pin.h` are gone (`grep -rn "core.mtx\|Pinned" src` is empty).
  - Pool level: `test_a_held_search_delays_no_refresh_or_commit` holds a search inside its spy for 1 s; a write, a refresh (`engine_for_read`), a
    commit, `revision()`, `merges_pending()` and `housekeeping()` all finish meanwhile (within 3 s, in practice at once), and the held search still
    returns its 3 hits. Engine level: `test_view_holds_nothing` (`test_engine_search_strata.cc`, replaces `test_pin_holds`): 15 operations across 3
    shards finish during the hold, in under 800 ms.
  - **Mutant M1** (a search holds a shared lock that refresh, commit, apply, `revision()` and `merges_pending()` take exclusively: the old
    locking) -> `test_a_held_search_delays_no_refresh_or_commit` fails (`CHECK(!(delayed))`).
- [x] **K3. C16's held-guard test.** `test_released_views_are_reclaimed_by_the_housekeeping`: a search holds a view, 60 more writes (each refreshed) make
  merges replace the segments it names, the commit leaves the retired view owed (`merges_pending() > 0`); the guard is released and nothing is
  written; only `pool.housekeeping()` runs, and the owed count reaches 0 and the files of the strata directory fall below the peak. **Mutant M3**
  (the owed count ignores retired views) -> fails at both owed checks and at the file count.
- [x] **K4. `too_many_readers` is a 503.** `strata::rethrow_translated` maps it to `errc::busy` (the existing 503 row of `catch_http_errors`).
  `test_readers_cap` (T8: a `DatabasePool` with `set_readers_per_index(16)`): 16 held searches, the 17th answers `busy`, a write and a commit still go,
  and a record freed serves the next search. `test_a_full_registry_answers_busy` (engine, a 2-record registry). **Mutants** M2 (mapped to `database`)
  -> both fail; M10 (the pool gives the engine no shared registry) -> `test_readers_cap` fails.
- [x] **K5. Proof.**
  - Unit tests, 26 of 26 ctest targets on **Release**, **Debug** (asserts on, `-Werror=return-type`) and **ASan+UBSan** (`detect_leaks=0`), no report.
  - **TSan**: the 9 concurrency targets (`pool`, `engine_host`, `engine_error`, `blocking_guard`, `wal_replay`, `offload_followups`, `engine_strata`,
    `engine_search_strata`, `debounce_or_run`) clean, and `harness/engine_mixed_load.py --engine strata --secs 120`: 12,793 documents, equals the
    final dump, 0 failures, 0 sanitizer reports. The first TSan run found a race in my new test (`spies` grown while a thread read it): fixed in
    `Xapiand:b19f4d17f`.
  - Docs E2E (newman 6.2.3): **641 of 641** assertions (371 requests) on the Release, Debug and ASan servers, no "blocks on database work" line.
  - `harness/wal_recovery_check.sh` PASS (50 of 50); `harness/engine_crash_check.sh --reactors 2` PASS (A: 50 of 50; B: 2,589 acknowledged, 2,589
    recovered; D: 2 writers, 5,588 recovered, each a prefix); `harness/graceful_shutdown_check.sh` PASS, scenario 4 included (SIGTERM at 0.5, 1.0, 1.5 s).
  - **X2-B1 probe** (`.scratch/review-x2/probe_mtx.cc`, 30,000 buffered documents, a commit of 171.6 ms): `merges_pending()` worst 0.016 ms idle and
    **0.001 ms during the commit** (the review measured 169.7 ms with `core.mtx`).
  - **Mutants**, one per new rule (`.scratch/mut.py`; each applied to a committed tree, built, run, reverted):

    | | Rule | Killed by |
    |---|---|---|
    | M1 | a search holds the index (old locking) | `test_a_held_search_delays_no_refresh_or_commit` |
    | M2 | too_many_readers is `busy` | `test_readers_cap`, `test_a_full_registry_answers_busy` |
    | M3 | retired views are owed | `test_released_views_are_reclaimed_by_the_housekeeping` |
    | M4 | a commit that times out leaves the write standing (X2-N2) | `test_a_commit_that_times_out_leaves_the_write_standing` |
    | M5 | a finishing node serves an open index (X2-N3) | `test_a_finishing_pool_serves_an_open_index` |
    | M6 | the uuid is read under the lock (X2-N1) | `test_the_uuid_is_read...` on TSan build: SEGV or allocation-size-too-big in 4 of 5 runs |
    | M7 | close waits for the readers | `test_close_waits_for_the_readers` |
    | M8 | a reopen waits for the readers | `test_a_reopen_waits_for_the_readers` (abort: the index destroyed under a view) |
    | M9 | the integrity check takes the turn | `test_the_integrity_check_takes_the_turn` |
    | M10 | the pool shares one registry | `test_readers_cap` |
    | M11 | a metadata write is visible after the next refresh | `test_engine_strata` (`metadata_keys`) |
    | M12 | a chore is awaited by a task on a scheduler (X2-N8) | `test_a_chore_needs_a_scheduler` (Debug) |
- [x] **K6.** `docs/mechanisms/reader-views.md` (new), the registry, search and fields notes updated, three changelog bullets under Changed.

X2 review items: **X2-B1** done (the revision and the work owed are atomics, `Core::note`; probe above; M1 covers the reads); **X2-N1** (uuid under
the entry's lock; M6); **X2-N2** chosen: the write stands and the autocommit commits it, because the write is applied and logged and a 503 would
make an auto-id client add it twice; making apply and commit one turn would end commit coalescing (`Shard::finish_write`; M4); **X2-N3** (the
`finished` check moved to the `empty` branch; M5); **X2-N8** (the assert; M12).

## Architect ruling on the reader registry (R1 to R4)

Binding text: `docs/reviews/phase7c-x2b-code.md`, "Architect ruling on the reader registry". Mechanism: `docs/mechanisms/reader-views.md`.

- [x] **R1. A registry per index.** `Core` owns a `StrataReaders` (strata's `reader_registry`) of `OpenOptions::readers_per_index` records; `DatabasePool::readers_`,
  `engine::ReaderRegistry` / `make_reader_registry` and `manager.cc`'s formula are gone; phase7c.md C9 rewritten. `readers_per_index()` (`manager.cc`) =
  2 x (reactors + HTTP offload workers + pool workers + bulk preparers + bulk indexers + 1 control loop): 174 on a 16-CPU host with the defaults (11,136 B
  per open index, 22,272 B on arm64); the x2 is for a read that takes a second view of its index under a callback, which I did not audit away. Bytes and scan cost
  are in `reader-views.md` Limits.
- [x] **R2. The registry outlives every guard.** `~Core` keeps the registry (`strata::detail::keep_forever`) when a record is still claimed. `OpenOptions::drain_timeout`
  (default 30 s) lets a test reach the timeout path. Test `test_a_view_outlives_a_drain_that_timed_out` (`test_engine_host.cc`): hold a view, close (busy after 100 ms),
  drop the shard, release the view.
- [x] **R3. `max_indexes_per_search`.** `--max-indexes-per-search` (`opts.max_indexes_per_search`, 0: the pool size), capped in `main.cc` at the pool size the descriptor
  computation leaves (200 by default); `check_search_fanout` at the top of `DatabaseHandler::match`, after wildcard and comma expansion, throws `errc::invalid` (a 400)
  with the limit and the option. The per-index cap stays a 503. Tests: `test_search_fanout_limit` (at, +1, the 1,010 case, 0 unset), `test_a_search_over_more_indexes_than_records_is_served`
  (40 indexes, 2 records each), `test_readers_cap_is_per_index`, and over HTTP `harness/search_fanout_check.py` (limit 1,010: 1,010 -> 200, 1,011 -> 400; defaults: 200 -> 200,
  201 and the review's 1,010 -> 400, never 503).
- [x] **R4. The N2 rule** is written in `reader-views.md` ("The rule for a request that holds a view"), with why X2b satisfies it (`run` is synchronous).

Carried forward to X3 (not done here, per the ruling): **X3-1** strata's chunked registry (revision 4; then `readers_per_index` goes); **X3-2** the per-index cap becomes D2's
in-flight admission cap; **X3-3** N2 made structural (a debug check in the request context; close and reopen drain off the turn); **X3-4** `Shard::view()` takes a lock
(`core.readers.load()` is a `std::atomic_load` on a `shared_ptr`): the acquire path must have none, and T13 measures `view()` at 1 and 8 threads; **X3-5** N1's T13 row
(per-index scan cost and bytes at 10^4 open indexes, with the cap from X3-2). X3 must not start suspending searches before X3-1 to X3-3.

## Done (commits)

- `Xapiand:6863b1076` the reader views: `Viewed`, the registry, `Core` without `core.mtx`, close and reopen that wait for readers, the atomics, `Shard::check`.
- `Xapiand:690d2cfc3` the handler, schema, resolver and views as tasks; the pool's registry and the X2 review items; the allowlist.
- `Xapiand:1afabbd06`, `0ab64f5dd`, `b19f4d17f`, `393de1dc9` the tests, the docs and this ledger.

## Needs L3 (and X4)

1. `SearchService::handle` (`search_views.cc:2988`) waits for the view's task with `engine::sync_wait` on an offload worker: awaiting it needs L3's
   coroutine `HttpHandler::handle` (X3).
2. The bulk indexer's threads (`DocPreparer`, `DocIndexer`, `~DocIndexer`, `handler.cc`) wait for the handler's tasks: D12's parse-ahead chore (X3).
3. A Lua script's foreign read (`lua/processor.cc:267`): X4's yields.
4. `restore_database_view` still blocks its worker in `next_object` and `DocIndexer::wait` (streamed body, condition variables): X3.
5. `FLUSH` still waits for its close tasks on the offload worker (`pool.cc`, X3).

## Next

- The code review of this branch (concurrency: the close protocol with readers, `Core::note`'s atomics, the registry sizing).
- X3 (views on the reactors) and X4 (Lua).

## Blocked on

Nothing.

## Log

### 2026-10-30

- Read the design (D2, D3, D5, D6, C9, C16), X2's ledger, the registry note and the X2 review; strata's `index.hh` reader handle, hazard registry and
  `defer_io` note; task's `task.hh`.
- Choices that shaped the cut: the adapter's writer side relies on the host's turn alone (the threaded engine tests act as the host: a turn per
  shard, a refresh after a write); reads see one published view, so `find_key` no longer sees the write buffer and a metadata write is visible
  after the next refresh; a closing index waits for the holders of its views (30 s, then `busy`) instead of being destroyed under them.
- A shard dump pages in 256 documents so a stored blob is read between pages (a scan's callback cannot await).
- `engine::strata::Shard::view()` is the only way to a view; `engine::search` holds one per distinct index.
- Not done on purpose: no new fuzz target (no decoder or parser was added or changed); no timing claim (the lead measures T5, T11).
- The host held `quiet` strict holds for about an hour; the builds queued behind them.
