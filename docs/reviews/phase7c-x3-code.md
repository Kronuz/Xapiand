> Status: CURRENT (2026-10-30) code review of phase 7c step X3 (views on the reactors), branch `phase7c/x3-views` at `Xapiand:a89f6b0d4`.

# Phase 7c X3 code review

- Date: 2026-10-30.
- Reviewed: `git diff d25fb70a4..a89f6b0d4` (b4915e5dd step 1, b7b1361b9 step 2, df93cf455 step 3, 21012b19f and a89f6b0d4 fixes); the ledger `docs/status/phase7c-x3.md`; the notes `docs/mechanisms/requests-on-the-reactor.md` and `docs/mechanisms/reader-views.md`; Kronuz/http `http_asio.h` and Kronuz/task `task.hh`, `task_asio.hh` at the pinned heads (read, not changed).
- Checked against: `docs/design/phase7c.md` D2, D3, D9, D12, D16, rows T2 and T13; the architect's ruling `docs/reviews/phase7c-x2b-code.md` (X3-1 to X3-5); `docs/reviews/calibration.md`.
- Checked by: ASan+UBSan (RelWithDebInfo, Homebrew LLVM 22) and TSan builds of `test_reactor_requests`, `test_pool`, `test_engine_host`, `test_blocking_guard`; an ASan `xapiand`; a master (`d25fb70a4`) RelWithDebInfo `xapiand` as the A side for regressions; `harness/async_check.sh --strict --extern strata=...`; probes in `.scratch/review/` (below). `quiet check` before each build; holds taken with `quiet light`.
  - Not run: docs E2E (641 of 641 is the developer's; I did not rerun it), Debug+sanitizer builds (my sanitizer builds are NDEBUG, see X3-N4), fuzzing, benchmarks, Linux, arm64.

Probes kept in `.scratch/review/` (worktree `x3`): `cfg.sh` (configure with the copied dependency sources in `deps/`), `probe_upload.py` (multi-shard numeric ids, same-id and parallel uploads, lying lengths, FLUSH and DELETE racing an upload, volume numbering, `kill -9` mid-upload then restart, stalled client at SIGTERM), `p3.py`, `p4.py` (raw-socket upload rows), `p5.py` (SIGTERM with an idle, a stalled-upload and a stalled-restore client), `p6.py` (DELETE racing an upload), `p7.py` (RSS of a 512 MiB upload and read-back), `p8.py` (deeply nested JSON through the new decode chore), `p9.py` (mixed chaos: searches, abandoned searches, restores, abandoned restores and uploads, `FLUSH ?databases`, writes and deletes, then SIGTERM under load), `p10.py` (client leaves during a RESTORE), `p11.py` (every content-type and method against the stream predicate), `run/cli/` (CLI `--dump` and `--restore` round trip, logs of the failing run). `build-asan/`, `build-tsan/`, `build-master/` and `wt-master/` (a `git worktree` of `d25fb70a4`) are kept for the re-check.

## Verdict

**APPROVE WITH CONDITIONS: lands once X3-B1 (the CLI `--restore` no longer exits) is fixed and checked; the rest is documentation and hardening for the same fix round.**

What holds, and reproduces:

- ASan+UBSan: `test_reactor_requests` 335 checks, `test_pool` 178, `test_engine_host` 95, `test_blocking_guard` 13, all clean. TSan: the same four, 0 reports.
- `harness/stored_stream.sh` under ASan (PORT=19110): under-cap, over-cap and 2x-cap blobs read back byte-equal, four parallel uploads, the 413/400/413/413 refusals, a client that leaves mid-upload (7 volumes, node serves). `harness/restore_stream_check.sh` under ASan (PORT=19112): 20,000 NDJSON, 5,000 MsgPack, chunked, bad line, all counted back.
- My upload probes (`probe_upload.py`, ASan, 1 and 3 shards, 0 sanitizer lines in the log): numeric ids 1 to 7 across 3 shards read back byte-equal; a JSON document replaced by a streamed blob; four concurrent uploads to one id (all 204, the document is one of the four, never mixed); eight parallel ids; a short body leaves nothing (404); a longer body stores exactly Content-Length; a chunked 20 MiB blob is a 413, a Content-Length of 2^64-1 or of limit+2 is refused; FLUSH during a 30 MiB upload (5 of 5 answered 200, upload intact); DELETE of the index racing the upload (5 delays, no corruption, the blob is readable after); `kill -9` mid-upload then restart: old blobs readable, a small `stored` write, a new streamed upload and both reads work, the crashed document names nothing (the 63 MB orphan volume stays, documented).
- A 512 MiB streamed upload peaks at 359 MiB RSS (base 38 MiB) under ASan, so the write side is bounded by buffers (`p7.py`).
- Depth-limited input still holds where decode moved to a chore: 2,000,006 and 16,000,006 byte nested-array bodies to PUT and RESTORE answer 400 "nested too deeply", node alive (`p8.py`).
- The stream predicate and the routing agree: a big body with a JSON, YAML or MsgPack type (parameters or case varying) is a 413, `text/*`, ndjson, form and multipart types stream like the old `stored` rule, PATCH/DELETE/`:update`/`:search`/root all answer 413/400 without reading (`p11.py`).
- Mixed chaos (`p9.py`, ASan, 40 s, 12 searchers, 2 restorers, 2 uploaders, FLUSH, writers, about 15,000 abandoned connections): no sanitizer line, no 500, 503s only; `FLUSH ?databases` ran 66 times against live writers and searches.
- `harness/async_check.sh --strict` passes with 0 unclassified; the removed options, pools, `DocPreparer`, `release.h`, `drain.h`, `objects_mtx`, `should_offload` and `body_reader` have no remaining use in `src/`, `tests/` or `harness/` (two prose mentions of `DocPreparer` remain in `docs/mechanisms/engine-write-path.md:15` and `harness/profile_bulk.sh:127`).

What blocks: X3-B1.

## Findings

### X3-B1. The CLI `--restore` never exits (regression against master)

- Probe: `.scratch/review/run/cli/` (`restore.log`, `restore2.log`, `restore3.log`, `restore_master.log`). Dump an index of 3,000 documents with `xapiand -D n --dump c --out d.mp`, then `xapiand -D n2 --restore c2 --in d.mp --shards 2`.
  - Master (`build-master`, `d25fb70a4`): prints `Restore sha256 = f1fb...` and exits 0 in 1.4 s.
  - Branch (`build-asan`, `a89f6b0d4`): prints the same sha256 after 5 s, then sits in `XapiandManager::join()`; after 60 s `2 tasks of the index registry did not end` (`manager.cc:918`), then loops forever in `database_pool->join(500ms)` ("Is taking too long to finish the database pool"); SIGTERM does not stop it (the loop only reacts to a negative `atom_sig`), only SIGKILL does. 3 of 3 runs. `sample` of the hung process: the main thread is in `io.run_for` (`manager.cc:911`), every worker idle, so the two tasks are suspended on something that never fires.
- The ledger and the E2E do not cover it: `--restore` is only reached by `main.cc:873`, which this branch changed to `engine::sync_wait(db_handler.restore_documents(fd))`, and no harness row runs it.
- Likely cause (a hypothesis I did not patch and rebuild to prove): the old `AsioHost::post` wrapped each resume in `HostScope host(this)` (removed in the `host_asio.h` diff), and the new one relies on `Host::bind_thread`, which only the HTTP loops and `run_loop()` call. In CLI mode `run_loop()` never runs, and in the server `join()` calls `bind_thread(nullptr)` (`manager.cc:462`) before `join()` drains the control loop's detached tasks on the main thread (`manager.cc:906-919`). Tasks that the drain resumes then see `Host::current() == nullptr` (`take_turn` takes the no-host path, timers fall back to thread sleeps), and the debounced-commit and housekeeping tasks started by `run_detached` (`pool.cc:488`, `handler.cc:2475`, `pool.cc:666`) never complete. Checking it is quick: scope `Host::current` around the drain in `join()`, or restore the `HostScope` in `AsioHost::post`, and rerun `run/cli`.
- Why it blocks: it is a regression in a shipped command (`--restore`) that hangs the process uninterruptibly, and the same drain runs at every server shutdown, where a detached task in flight (an idle close, a debounced commit) would hang the same way.
- Required: fix it, and add a harness row that runs `--dump` then `--restore` on a fresh data directory, asserts the sha256 lines match, and that the process exits within a bound (the 60 s drain plus the join must not be reachable).

### X3-N1. A shutdown does not cancel requests in flight; the note says it does

- `docs/mechanisms/requests-on-the-reactor.md` ("Cancel reads, not writes") and the ledger say a shutdown requests the cancel source, so "a shutdown ends [RESTORE] at the next object". The cancel is delivered by `http_service->stop()` (`manager.cc:930`), which `join()` reaches only after `ready_to_end_http()` (`!http_clients`, `manager.cc:1041`) is true or the forced path fires (`_try_shutdown > 6`, `manager.cc:622`). D16's order (abort first, then wait) is X6's.
- Probes: `p5.py` with SIGTERM 2 s into a stalled 40 MiB `stored` upload: exit after 40.0 s ("Is taking too long to start shutting down: HTTP is busy" every 5 s); `p10.py hold`, a complete 2.2 MiB RESTORE body with the socket held open: exit after 30.1 s, the restore ran to completion. Same as master (the wait is unchanged), so not a regression, but the new text claims otherwise.
- Fix: correct the sentence (shutdown cancels at the forced stop until X6), or move the abort ahead of the wait now; either way T7's "SIGTERM under load" row is X6's.

### X3-N2. A stalled body cannot be cancelled

- `Request::read_chunk` (`search_views.cc`) awaits `BodyStream::async_read` with no tie to the request's cancel source; `handle()`'s `on_cancel` only requests the source, which `budget->checkpoint()` reads. A client that stops sending mid-upload or mid-restore holds a coroutine, an `IndexChores` group and an open volume until the connection's request timeout (30 s here; the HTTP service has no inactivity timeout for plain HTTP).
- Fix: in the `on_cancel` callback also `hreq.body_stream->fail(asio::error::operation_aborted)` when a stream exists, so `read_chunk` throws and `upload_stored` aborts (volume removed).

### X3-N3. `bind_thread(nullptr)` is not exception safe

- `run_loop()` (`manager.cc:462`) unbinds after `reactor.run()`; `control_offload` rethrows the offload's exception out of `run()` ("rethrown on the loop, out of run_loop()", `manager.cc:481`). On that path the main thread stays bound to `_rx->host`, which dies with the manager; later `Host::current()` reads on that thread see a dangling host. Use a scope guard. Same remark for any future `bind_thread` caller.

### X3-N4. The sanitizer builds do not see the wait rule

- `engine::Run`'s hop, `TaskCounters` and `check_no_view_held` exist only without `NDEBUG`. The ASan and TSan builds the ledger and I use are RelWithDebInfo, so `test_a_task_that_holds_a_view_waits_for_nothing` is exercised only in the Debug build, which has no sanitizer. The debug hop also keeps a `Run*` and a coroutine handle in the scheduler queue, and `AsioHost::post` (timers, `lock_until`) bypasses the hop, so a turn that times out resumes with no counters set; both are debug-only but run in the Debug E2E.
- Condition: one Debug+ASan and one Debug+TSan run of `test_reactor_requests`, `test_pool`, `test_engine_host` at the fix commit.

### X3-N5. The admission returns before the record

- `Viewed::release` (`viewed.h`) resets the lease, then the guard; the comment says the gate is alive while the record is claimed, which is why the order matters for lifetime, but it means a new reader can be admitted while the departing one's record is still claimed. At exactly the cap that gives the registry's own `too_many_readers` (also a 503) instead of a served read. Harmless for the 503 contract; say so in the comment, or keep a record count in the gate.

### X3-N6. Large reads and unproven placement

- Read side: GET of the 512 MiB blob peaked at 1.5 GiB RSS (`p7.py`, ASan), about 3x the blob; with the 4 GiB limit an anonymous GET of a stored 4 GiB blob wants about 12 GiB. The ledger lists D9's read side as not built; keep it on the X-list and note the limit's cost in `requests-on-the-reactor.md`.
- `DatabaseHandler::upload_stored` chooses the shard from the id (`handler.cc:509-520`), `replace_document_term` chooses it again at index time (`handler.cc:1714-1750`, and by the shard of an existing document across all shards). They agree for every case I ran (3 shards, numeric and string ids, replacing an existing document), but nothing checks that the document landed in the shard that holds the blob; a mismatch stores a locator into another shard's volumes. A `CHECK`/error in `index_uploaded` when the shard differs would close it.
- `ShardEndpoint::upload_begin` tests `writable_storage` (`shard.cc`) outside the turn while `close_marked` resets it in a chore; the caller's lease keeps an idle close away, the stale-reset close does not. Move the test under the lease's turn or into the chore.
- Test gaps: no unit test for `upload_stored` (the harness is the only cover, M27/M28 killed there), none for the `--restore`/`--dump` round trip (X3-B1), none for a failing child of `when_all(indexer->index(batch), next_batch)`. `task::when_all` waits for both children, so the parser state is never touched by two things at once; I read it, but a test would keep it so.

### X3-N7. Small items

- `docs/mechanisms/engine-write-path.md:15` and `harness/profile_bulk.sh:127` still describe the removed `DocPreparer`/`DocIndexer` threads.
- `FLUSH /` with no parameters shuts the node down (pre-existing: `flush_view`, `search_views.cc:1490`); my first chaos run tripped it. Not X3; flagging it for the owner.

## Blocking items / Required changes

- **X3-B1** CLI `--restore` hangs after the restore completes (master exits in 1.4 s): fix the control loop's drain (or `AsioHost::post`) so detached tasks run with their host, and add the `--dump`/`--restore` round-trip row with an exit bound.

## Conditions

- C1. X3-N1: the mechanism note and ledger say what the cancel does today (forced stop only) or the abort moves earlier; D16 stays with X6.
- C2. X3-N4: Debug+ASan and Debug+TSan of the three concurrency tests at the fix commit.
- C3. X3-N2 and X3-N3 are cheap and belong in the same fix round; if not, the ledger records them as carried to X6.
- C4. X3-N6's read-side memory and the shard-agreement check are recorded on the ledger's Remaining list with an owner (X-step); the `kill -9` orphan volume (63 MB in my probe) stays on the `reclaim_volumes` item.
- C5. Escalation: none needed. The design questions raised (D16 ordering versus `ready_to_end`, and the stalled-body cancel) are the architect's only if the lead wants the abort moved ahead of the wait in X3 rather than X6.
