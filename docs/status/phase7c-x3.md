> Status: HISTORY (2026-11-08): superseded by Xapiand's `.campaign/xapiand/` (`campaign map`); kept for its results and reasoning. Was: (2026-10-30) the X3 developer's ledger (phase 7c step X3: views on the reactors as coroutine handlers); built, proved and reviewed (approve with conditions, `docs/reviews/phase7c-x3-code.md`); the review's fixes are in; not pushed.

# phase7c X3 (build): views on the reactors

Branch `phase7c/x3-views` in `Xapiand-wt/x3` (local only, from master `Xapiand:d25fb70a4`). Design: `docs/design/phase7c.md` (D2, D3, D9, D12, D16, T rows, section 5's X3);
the architect's ruling and the X3 items: `docs/reviews/phase7c-x2b-code.md` (X3-1 to X3-5); http's L3 ledger `docs/status/phase7c-l3.md`, "What X3 must change".

## State

Steps 1 to 4 done and committed (the proof is below); the review ruled approve with conditions and its fixes are folded in ("Review fixes"). Remaining: see "Remaining".

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
- [x] **4. Proof** (below). Review fixes below. Not done: M31, and the lead's measurements.

## Proof (at `Xapiand:a89f6b0d4`)

- `grep -rn sync_wait src/` outside `src/engine/` lists the CLI (`main.cc` x3), the registry's shutdown and detached task (`pool.cc` x2), the open chore's WAL replay on a
  worker (`database/shard.cc` x7) and Lua's foreign read (`lua/processor.cc:268`, X4, let through by `engine::BlockingAllowed`). `harness/async_check.sh --strict
  --extern strata=<build>/_deps/strata-src`: exit 0, **0 unclassified** (247 hits: 190 allowed, 44 owed, 13 compiled out), no stale row.
- Unit tests: **27 of 27** ctest targets on **Release**, **Debug** and **ASan+UBSan** (`detect_leaks=0`), each rebuilt at the head.
- **TSan**: the concurrency targets (`pool`, `engine_host`, `engine_error`, `blocking_guard`, `wal_replay`, `engine_strata`, `engine_search_strata`, `debounce_or_run`,
  `reactor_requests`) 9 of 9, 0 reports; `harness/engine_mixed_load.py --engine strata --secs 120`: 20,373 documents, the final dump equals the model, 0 failures, 0 sanitizer reports.
- Docs E2E (newman 6.2.3, `harness/engine_e2e.sh`): **641 of 641** (371 requests) on the Release, Debug and ASan servers; no "blocks on database work" line in any node log.
- `harness/wal_recovery_check.sh` PASS (50 of 50). `harness/engine_crash_check.sh --reactors 2` PASS (A: 50 of 50, the strata check on 5 shards; B: 3,723 acknowledged, 3,723
  recovered; D: 2 writers, 6,714 recovered, each a prefix). `harness/graceful_shutdown_check.sh` PASS, scenario 4 included (SIGTERM at 0.5, 1.0, 1.5 s).
- **T2's upload rows** (`harness/stored_stream.sh`, on Release, Debug and ASan): a `stored` upload of 33,554,432 bytes (2x the cap), of 16,777,217 (one over) and of 16,773,120
  (just under, buffered) each read back byte-equal; four 20 MiB uploads at once, byte-equal; a size past the record limit 413, a POST of a big blob with no id 400, a big
  stored body for a view that cannot stream 413, a big JSON body 413; a client that leaves mid-upload leaves no volume and the node serves.
  `harness/restore_stream_check.sh` (same three builds): 20,000 NDJSON documents, a 5,000-document MsgPack stream, a chunked body and a stream with a bad line, each counted back;
  a client that leaves mid-stream keeps what was indexed (13,076 of 20,000 here, which survive `kill -9` and a restart).
- **Found by the new rows and fixed**: the Debug E2E aborted on the first RESTORE (the indexer's last reference was a copy inside a `std::function` the reactor destroyed; the
  chore takes it by reference); ASan found a write into a freed Core (`test_a_view_outlives_a_drain_that_timed_out`: a view returned its admission to a gate that lived in the
  Core; the gate now lives in the registry, which outlives every guard, `21012b19f`); TSan found a race the new reader-and-reopen stress exercises (`Handle::seen` was a plain
  integer a reader compares while the reopen writes it; atomic, `a89f6b0d4`; without it `test_views_are_acquired_while_the_index_is_replaced` reports it under TSan).
- **Mutants**, one per new rule (`.scratch/mut.py`: apply to the committed tree, build, run, restore; the numbers continue X2b's M1 to M16):

  | | Rule | Killed by |
  |---|---|---|
  | M17 | a search ignores its budget | `test_a_search_with_a_budget_shares_its_loop`, `test_a_cancelled_search_stops_and_lets_go` |
  | M18 | the per-index cap is not an admission (no gate) | `test_the_cap_is_an_admission_that_counts` (`views_refused()` stays 0) |
  | M19 | a task that holds a view may wait for a turn | `test_a_task_that_holds_a_view_waits_for_nothing` |
  | M20 | ... for an open | same |
  | M21 | ... for a drain | same (abort) |
  | M22 | the close drains readers under the turn | `test_a_close_drains_its_readers_without_the_turn` |
  | M23 | a bulk body is parsed on the loop | `test_a_bulk_body_is_parsed_off_the_loop` (the loop's CPU is a third of the parse's, or more) |
  | M24 | no demoted cap at admission | `test_a_loop_past_its_demoted_cap_refuses_new_requests` |
  | M25 | FLUSH does not await its closes | `test_flush_closes_every_idle_index_before_it_returns` |
  | M26 | a group of chores is joined on the loop | `test_quiet_when_a_loop_drops_a_group_of_chores` |
  | M27 | an upload drops a byte of a chunk | `harness/stored_stream.sh` (PUT answers 500) |
  | M28 | any view takes a streamed body | `harness/stored_stream.sh` (413 row) |
  | M29 | a failed restore does not commit what it indexed | `harness/restore_stream_check.sh` (`kill -9` row) |
  | M30 | a replaced index's views handle is freed | `test_views_are_acquired_while_the_index_is_replaced` (`view_handles_kept`) |

  Not killed, with the reason: the rule that the shared writer's rollover and an upload take volumes from one counter (`next_volume_`) has no mutant, since reaching a rollover
  takes a 32 GiB volume; a revert to `++volume` is the only change and a review reads it. The cancel hookup (`hreq.on_cancel`) is covered by the `< N` row of the restore harness; its
  mutant (M31) is not run yet.

## Done (commits)

- `Xapiand:b4915e5dd` step 1: the pins, the coroutine service, FLUSH as a task, the close split and the view counters.
- `Xapiand:b7b1361b9` step 2: searches with a budget, the admission cap, the wait rule and its mutants, the lock-free acquire.
- `Xapiand:df93cf455` step 3: the bodies, RESTORE parse-ahead, the task bulk indexer, streamed uploads, FLUSH, the allowlist and the notes.
- `Xapiand:21012b19f` the admission gate lives with the registry (ASan); the kept views handles are tested.
- `Xapiand:a89f6b0d4` `Handle::seen` is atomic (TSan).

## Review fixes (`docs/reviews/phase7c-x3-code.md`)

- **X3-B1** (the CLI `--restore` never exits). Cause, found by tracing the two stuck autocommit tasks to their engine-commit chores: `join()` drains the control loop's
  detached tasks with `io.restart(); while (...) io.run_for(10ms)`. An `io_context` that runs out of work is *stopped*, and a chore that finishes after that posts its
  resume to a stopped context, which nothing runs; X2's drain had the flaw, the restore's late autocommits (`.xapiand/indexes`) exposed it. The loop restarts the context every
  iteration. The drain and `run_loop` also bind the thread to the control host with `engine::HostBinding` (the review's hypothesis; a scope that puts back the previous
  binding, which is also **X3-N3**), so resumed tasks find their host. Test: `harness/cli_roundtrip_check.sh` (`--dump` then `--restore --shards 2` of 3,000 documents,
  both exit 0 within a bound under `timeout -s KILL`, same sha256); on ASan without the per-iteration restart (mutant **M32**) the restore never exits (137 at the bound), with it
  both exit 0. Not reproduced on the Release build either way (timing).
- **X3-N1** the mechanism note says a shutdown reaches `abort_all` only after the in-flight requests drain (D16's order is X6's). **X3-N2** the cancel callback also fails the request's
  `BodyStream`, so a read waiting for a stalled body ends (and an upload aborts and removes its volume). **X3-N3** `HostBinding`. **X3-N4** Debug+ASan and Debug+TSan builds
  (`build-dasan`, `build-dtsan`) of `test_reactor_requests` (341 checks), `test_pool` (180), `test_engine_host` (95), `test_blocking_guard` (13): all clean, so the wait rule runs
  under both sanitizers. **X3-N5** the comment in `Viewed::release` says what the gap gives (the registry's own 503). **X3-N6** `index_uploaded` fails if the document's shard differs
  from the blob's, and the `writable_storage` test is inside the chore; the read-side memory (about 3x the blob) is in the note's Limits and stays on D9's read item. Tests for
  `upload_stored` and for a failing `when_all` child remain harness-only/read (listed below). **X3-N7** the two stale `DocPreparer` mentions.
- Final sweep at the fix commit: ctest 27 of 27 and docs E2E 641 of 641 on Release, Debug and ASan+UBSan; `stored_stream.sh`, `restore_stream_check.sh`, `cli_roundtrip_check.sh` on ASan.

## Remaining (not done here)

1. **X3-1**: strata's chunked registry (revision 4); then `readers_per_index` (16 searches per reactor plus the pool terms, an estimate) goes, and a chunk is allocated on the first reader.
2. **X3-5**: N1's T13 row, the per-index scan cost and bytes at 10^4 open indexes with the cap from X3-2; and **X3-4**'s measurement of `Shard::view()` at 1 and 8 threads on one index
   (the lock is gone; the number is the lead's).
3. **X4**: Lua's yields and its foreign read (`engine::BlockingAllowed` is the one place a reactor still waits; `async_check.sh` lists it).
4. D9's read side (a blob past the cap streamed out in chunks; today `storage_get_stored` reads it whole) and `reclaim_volumes` for orphan upload volumes.
5. The control plane's one-worker pool (`Reactors::pool`) and `--http-servers` / `MAX_HTTP_SERVERS` stay until X5 and X6 (D13, D15).
6. Unit tests for `upload_stored` and for a failing `when_all(index, next_batch)` child (the harness and a read cover them); M31 (the cancel hookup).
7. T12 and T4: RESTORE preparing and applying run on the request's reactor (the parse is what leaves it); the lead measures.

## Next

- The code reviewer's ruling, then its findings folded in here.
- The lead merges, measures T4/T5/T12 and X3-4/X3-5, and starts X4.

## Blocked on

Nothing.

## Log

### 2026-10-30

- Choices that shaped the cut: a per-request scheduler (`engine::Run`) exists in debug builds only, to count what a task holds; Release resumes straight through the host.
  The streamed upload needs a document id (a new numeric id is placed by the shards' load after the body), and goes to the shard the id places it in. RESTORE's results
  vector, counters and ordering are the old indexer's; its commit now precedes the response.

- Pins moved to the heads the task names (reactor 76c32b4, http fce24c6, http-log d2315b8, task 9a85e89, threadpool e961e44, strata c82ac9f, prospect c77d6eb, storage e4c5746,
  compressors dc4474b).
