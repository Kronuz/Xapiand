> Status: CURRENT (2026-10-30) the X3 developer's ledger (phase 7c step X3: views on the reactors as coroutine handlers); in progress, not pushed.

# phase7c X3 (build): views on the reactors

Branch `phase7c/x3-views` in `Xapiand-wt/x3` (local only, from master `Xapiand:d25fb70a4`). Design: `docs/design/phase7c.md` (D2, D3, D9, D12, D16, T rows, section 5's X3);
the architect's ruling and the X3 items: `docs/reviews/phase7c-x2b-code.md` (X3-1 to X3-5); http's L3 ledger `docs/status/phase7c-l3.md`, "What X3 must change".

## State

Steps 1 to 3 done and committed; the proof (step 4) is in the log below. Remaining: X3-1, X3-5, the Lua foreign read (X4) and what "Remaining" lists.

## Done criteria

- [x] **1. Pins and service.** The nine pins; `SearchService::handle` is a coroutine and `should_offload` is gone; `manager.cc` (`reactor()`, the control plane's own
  one-worker `asio::thread_pool` until D13/X5, `HttpAsioService(handler, reactors, 1000)`); `engine/release.h` and `engine/drain.h` (dead after the pools went) deleted with
  their tests; the demoted cap (256 per reactor) is `RequestRejected(503, ..., Retry-After: 1)` from `create_extension`; a disconnect or a shutdown requests the request's
  cancel source (`Request::cancel`, `Request::budget`, 200 us slice); `storage` needed `storage::posix` from the library in `xapiand_storage_io.h`; `exact_count` is a task in prospect.
  Beyond the list: `ShardEndpoint`'s `IndexChores` group is joined on a worker, never on a loop (`~WorkGroup` waits, and `block_guard` fires on a loop even when idle);
  a Lua foreign read on a reactor is let through by `engine::BlockingAllowed` until X4.
- [x] **2. Views on the reactor.**
  - Searches are coroutines with the request's budget (`SearchRequest::budget`, `DatabaseHandler::budget`, set by the search and count views): `engine::strata::run` awaits
    prospect's `search_task` over views, snapshot and plan that live in its frame. I did not use `search_pinned`: it moves the snapshot into its own frame while the
    scorer builder here captures `term_table`/`plan`, which point into the snapshot, so a frame-local `search_task` pins the same things without a second copy.
  - **X3-2** the per-index cap is a `reactor::AdmissionGate` in each `Core` (`view_gate`, limit `readers_per_index`); `Viewed` holds the lease; refusal is `busy` (503) before a
    record is taken; `Shard::views_refused()` counts. `readers_per_index` is now 16 searches per reactor plus the pool and control terms (194 records on this host).
  - **X3-3** debug check: `TaskCounters` per request (`engine::Run`, a per-request scheduler whose resumes set the counters), `Viewed` counts, `take_turn`/`ensure_open`/`drain_readers`
    call `check_no_view_held`; close drains off the turn (`Shard::begin_close`/`finish_close`; `close_marked` releases the turn between them).
  - **X3-4** `Core::readers` is a `std::atomic<const Readers*>`; the owner keeps every handle it published (`handles`) until the Core ends.
  - Listed as remaining: **X3-1** (strata's chunked registry, revision 4), **X3-5** (the T13 row).
- [x] **3. Bodies and blocking paths.**
  - `body_reader` is `body_stream`: `Request::read_chunk` reads it with `task::await_asio`; RESTORE's `objects_mtx` is gone (one chore touches the parser at a time).
  - **RESTORE parse-ahead** (D12, C8): `Request::next_batch` reads a chunk and parses it in a chore of the restore's own `IndexChores` group; `restore_database_view`
    runs `DocIndexer::index(batch)` beside the next `next_batch` (`task::when_all`). A body that arrived whole is parsed in one chore (`feed_whole_body`); a document body
    past 64 KiB is decoded in a chore (`decode_large_body`).
  - **Bulk indexers**: `DocPreparer`, the `DocIndexer` threads, `doc_preparer_pool`, `doc_indexer_pool`, `--bulk-preparers`, `--bulk-indexers` and `--http-clients` are deleted;
    `DocIndexer` is a plain object whose `index(batch)` and `commit()` are tasks (the CLI `--restore` uses it through `sync_wait`). Results, counters and the closing
    commit are as before; the commit now runs before the response, and on every exit.
  - **Streamed uploads of `stored` blobs** (D9): `wants_body_stream` takes a `PUT` or `POST` with `Content-Length` past `BUFFERED_BODY_CAP` (16 MiB) and a stored type;
    `DatabaseHandler::upload_stored` picks the shard the id goes to, `ShardEndpoint::upload_begin` takes a volume from `next_volume_` (the shared writer's rollover takes
    from it too), and `StoredUpload` writes through storage's `StreamWriter` in flush chores, the last one committing; `index_uploaded` then indexes the document in its
    own turn. Past 4 GiB (framing included) is a 413 (`PayloadTooLarge`); a streamed body for any other view is a 413; no id is a 400. Not built: streaming a large blob
    out (D9's read side) and collecting orphan volumes (`reclaim_volumes`).
  - **FLUSH** awaits `DatabasePool::flush()` (one close task per index, `when_all`); `restore_database_view` blocks no worker.
- [ ] **4. Proof.** (below)

## Done (commits)

- `Xapiand:b4915e5dd` step 1: the pins, the coroutine service, FLUSH as a task, the close split and the view counters.
- `Xapiand:b7b1361b9` step 2: searches with a budget, the admission cap, the wait rule and its mutants, the lock-free acquire.
- step 3 (the commit after this ledger line): the bodies, RESTORE parse-ahead, the task bulk indexer, streamed uploads, FLUSH, the allowlist and the notes. Its hash is in the git log.

## Next

## Blocked on

Nothing.

## Log

### 2026-10-30

- Pins moved to the heads the task names (reactor 76c32b4, http fce24c6, http-log d2315b8, task 9a85e89, threadpool e961e44, strata c82ac9f, prospect c77d6eb, storage e4c5746,
  compressors dc4474b).
