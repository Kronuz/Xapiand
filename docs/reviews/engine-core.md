# Xapiand review: engine core (X2)

- Date: 2026-10-21.
- Branch: `phase0.5/engine-core`, worktree `~/code/Kronuz/Xapiand-wt/engine-core`, HEAD `47bd0a66f`, five commits over master `5cce4860d` (half A `7eae6786c..d77cf2e0b`, half B `3be508883`, ledgers `47bd0a66f`). Local only, nothing pushed.
- Reviewed against: `strata/docs/design/xapiand-on-strata.md` (D1 to D13, row X2, gates T3, T4), `docs/status/engine-core.md`, `strata/docs/reviews/calibration.md`.
- Reviewer: code reviewer, fresh context. Nothing under `src/xapian/`, `sortable_serialise.{cc,h}` or `src/server/remote_protocol_views.cc` was opened (see item 5 for one disclosure).

## Verdict: APPROVE WITH CONDITIONS

No blocker for landing on Xapiand's local master. The default path (pool off) is behaviourally the master path: same E2E result, same WAL replay, same version answers, same retry decisions for every Xapian exception type I could construct. Two items must be recorded as owed before the offload pool can default on or 7c starts (CR1, CR2), and the ledger needs three corrections (CR3, CR4, CR9). Three pre-existing concurrency bugs surfaced; I reproduced each on master (CR8) and none is caused by this branch.

Escalate to the architect, not to a fix round: CR1 and CR2 (D13 says "never on a reactor thread" and "Xapiand's existing offload pool"; the branch does neither and the design needs to rule on the bridge). CR8 needs a designer or the pool owner (checkout and cleanup concurrency), separately from this branch.

## Conditions (to land)

1. CR1 and CR2 are recorded in `docs/status/engine-core.md` as owed, with the rule that `XAPIAND_ENGINE_OFFLOAD_THREADS` stays off until both close.
2. Ledger corrections: CR3 (the "keeps existing Xapian catches working" claim has three exceptions), CR4 (the retry rule and the ladders have no test), CR9 (the env var is documented only in code comments).
3. Nothing else blocks.

## Blockers

None.

## Findings

### CR1. `sync_wait` runs on reactor threads (condition, not a blocker)

The coordinator asked for this to be recorded and ruled blocker or condition. Ruling: **condition**, owed before the pool defaults on or 7c starts.

Evidence, two reactor-thread callers confirmed by running, not only by reading:

1. HTTP `FLUSH` runs inline on the HTTP reactor (`SearchService::should_offload` returns false for it, `src/server/search_service.cc:27`; `flush_view`, `src/server/search_views.cc:1511-1531`). It calls `DatabasePool::cleanup(true, false)`, which reaches `ShardEndpoint::clear` (`src/database/pool.cc:476`), `Shard::do_close`, `Shard::commit` and `task::sync_wait(wdb->commit(...))` (`src/database/shard.cc:907`). The ASan report in `.scratch/review/hnode0.log` shows `flush_view` called from `serve_connection_body` under `asio::io_context::run` in `reactor::TcpServer::start`, i.e. the reactor thread. The lldb run `lldb_threads.out` shows `engine::xapian::Shard::commit < Shard::commit < Shard::do_close < ShardEndpoint::clear < DatabasePool::cleanup` (5 hops with the pool on).
2. The control-plane reactor (thread `Xapiand:MAIN`, `XapiandManager::run_loop`) runs `XapiandManager::setup_node_async_cb` (`src/manager.cc:767-835`), which opens shards, reads metadata, and writes the node document (`DatabaseHandler::update` to `replace_by_key`, `set_metadata`). `lldb_threads.out` lists these under `[Xapiand:MAIN]` with `reactor::Signal::send` frames: 11 + 11 + 11 + 1 + 1 hops. `load_nodes` (`manager.cc:1568-1590`) is the same shape; I did not trace it separately.
3. Not determined: `DocIndexer::~DocIndexer` commits from a last-reference destructor (`lldb_threads.out`, 5 hops, thread unnamed). Whichever thread drops the last reference pays the commit.
4. Everything else I sampled runs on worker threads: HTTP workers (unnamed), `Xapiand:DP`, `DI`, `AC` (committer), `WL`. The remote protocol and replication handlers say they offload their Xapian dispatch (`remote_protocol_service.h:26`, `replication_protocol_service.h:29`); I did not run the cluster, so those two are read-only evidence.

Why it is not a blocker now:

- The default path calls the engine inline on the same thread that called Xapian before; master blocked the same reactor in the same place.
- With the pool on, the reactor blocks until a pool thread finishes. I found no path where a pool thread waits on a reactor, or on the pool: the only user code that runs on a pool thread is the `finalise` hook (`finalise_write`, `src/database/shard.cc:104`), which edits a document and calls no shard operation. Pool on survived 7 E2E runs, the 16-client hammer with pool sizes 1 and 3, 8 concurrent restores with pool sizes 1 and 4, and 450 DUMP and RESTORE cycles (see CR8 for the one pool-on stall, which also happens with the pool off).

Why it still must be owed: D13 states `sync_wait` never runs on a reactor thread, and a reactor waiting on a pool is the pattern that stalls the loop (every connection on that reactor waits behind a commit) and deadlocks once 7c puts reactor-resident work inside the pool's path.

Required before default-on or 7c: move the `FLUSH` cleanup and `setup_node`/`load_nodes` database work onto an offload thread (or make them coroutines), and add a debug assertion in a single `engine` wrapper around `sync_wait` that fails when the thread is a reactor thread. Probe: `lldb_threads.py`, output `lldb_threads.out`.

### CR2. `Offload` is a second thread pool, and its lifetime is unguarded (condition)

`src/engine/xapian/shard.{h,cc}:45-135` hand-writes a thread pool (mutex, condition variable, deque, joined threads). Calibration (2026-10-17, "one mechanism twice") asks for a search of the existing mechanisms first. Xapiand already has `ThreadPool` (`thread.hh`, used by the `DP`, `DI`, `WL` pools), `Kronuz/threadpool`, the reactor's offload pool, and `Kronuz/task` ships `task::asio_scheduler` (`task_asio.hh`) over `asio::thread_pool`, which Xapiand already links; the branch pins `task` and then forces `TASK_ASIO OFF` (`CMakeLists.txt`). D13 says the Xapian implementation runs "on Xapiand's existing offload pool". The ledger says the choice was left to the lead; it should be recorded as a deviation and closed by reusing one of the above before the pool defaults on.

Smaller points on the same code:

1. `set_offload` is never reset. `main.cc:811-831` owns the pool in a local `unique_ptr` declared before `manager`; the normal path calls `manager.reset()` first, but the "uncleanly done" path (`managers != 0`) and an exception out of `manager->run()` destroy the pool while `offload_pool` still points at it, and any later shard operation then posts to freed memory. Reset the pointer in a scope guard.
2. `XAPIAND_ENGINE_OFFLOAD_THREADS` accepts any positive `atoi` value; no upper bound (`main.cc:813-816`).
3. `open_local` opens writable databases with `DB_RETRY_LOCK` inside a pool thread (`engine/xapian/shard.cc:452,457`); a lock wait occupies a worker for its whole duration. With a small pool this is the one place a worker can block on something other than CPU and I/O. I could not make it block in the probes (one process, one writer per shard), so this is a documentation point.
4. Bounded and documented: the queue is bounded by the number of blocked callers (each caller waits in `sync_wait`), and the comments in `engine/xapian/shard.h:36-45` and the ledger state the deadlock rule. The env var appears in no user or developer document (CR9).

### CR3. Translated exceptions lose their subtype (non-blocking; ledger claim is too strong)

`Failure(const X& original)` copies the caught static type (`src/engine/xapian/errors.h:41`), and `rethrow_translated` catches `DatabaseOpeningError` and `DatabaseError` as the base. Probe `ladder_probe.cc` (run output `ladder_probe.out`) shows a translated `DatabaseVersionError`, `DatabaseLockError` and `DatabaseCreateError` is no longer caught by `catch (const Xapian::DatabaseVersionError&)` and so on. No catch site in Xapiand names those three (`git grep` for the three names outside `src/xapian`), so no behaviour changes today, and the retry decisions are identical (CR5). The ledger sentence "keeps existing Xapian catches working" should say "except three Xapian subtypes nobody catches", or `Failure` should be templated on the thrown type.

### CR4. The headline behaviour change and the nine ladders have no test (non-blocking)

Nothing exercises the retry rule ("a retry keeps the caller's expected version"): the ladders are reachable only through a database error in the middle of a write, `RANDOM_ERRORS_DB_THROW` sits before the retry loops (`shard.cc:885` and its siblings), and `XAPIAND_RANDOM_ERRORS` is off. `test_engine_shard` covers versions, conflicts and the pool, not retries. The ledger admits the behaviour is "not reachable from the E2E suite". I verified the decision tables with `ladder_probe.cc` (below), which copies the kind switch of `Shard::retry_or_throw` and `Shard::reopen`; a copy is not a test of the code. Follow-up: a fault hook on `engine::xapian::Shard` (throw once after the write) so one test drives `Shard::add_document` through a retry and checks the version and the document count.

### CR5. Items 2 and 3 of the brief: error kinds and the version collapse hold

Item 2 (catch sites). Method: read all nine ladders, `reopen` (both loops) and `reopen_*` against `5cce4860d` side by side, then ran `ladder_probe.cc`: for 21 exception types (every Xapian database, network, document and argument error, `RangeError`, `InternalError`, `SerialisationError`, `QueryParserError`, `UnimplementedError`, `std::runtime_error`) it computes the old outcome from the old catch clauses and the new one through `rethrow_translated` and the kind table, at `t == 0` and `t == 5`, for the write ladders, the first `reopen` loop and the second. Result: **no difference in any row** (the only three flags are the CR3 subtype rows). Hierarchy checked by compile-time `static_assert` (`hier.cc`): `DatabaseNotFoundError`, `DatabaseNotAvailableError` and `DatabaseVersionError` are `DatabaseOpeningError`; `DatabaseLockError` and `DatabaseCreateError` are not (they stay in the "close and throw" branch in both versions); `NetworkError` is not a `DatabaseError`. Outside `shard.cc`: the three handler loops, `index_resolver_lru.cc` and `schemas_lru.cc:600` still catch the version conflict (as `engine::Error` kind or as the Xapian type); `http_utils.h` still maps every kind to the same HTTP status because `Failure` derives from the Xapian type and copies its system error string. No Xapiand wrapper class derives from `Xapian::DatabaseError` without `engine::Error`, so no wrapper falls out of a ladder. `engine::Error` is not a `std::exception`; no catch of `std::exception` in these paths could have caught the old types either (Xapian errors are not `std::exception`).

Item 3 (version rule). The collapse matches the old three dances for index, update, patch, replace by key (string id), replace by numeric id, add (POST) and delete. The old comparison is by serialised string, the new by number; they agree for every integer version the API can carry (CR6 for the one place they do not). `versions.sh` runs 29 requests (stale version, version 0 on an existing and on a new id, version N on a new id, `_version` in the body, PUT, POST, UPDATE, PATCH, numeric id, delete then recreate) and the answers are **identical to master** `5cce4860d` (`versions-master.out` against `versions-0.out`) and identical with the pool on (`versions-3.out`). The stated change is right: the old retry compared the version it had just assigned with the stored one and raised the spurious 409. Concurrent writers: a shard is checked out exclusively for writing, and the check and the write happen under that checkout, so the rule is as safe as before; a retry that re-runs `finalise` is idempotent (version term, version slot and embedded id and version are re-set to the same values because the stored version has not moved). WAL replay (`version_ == false`, no expected version, stored version plus one) is unchanged in logic and verified (item 4).

### CR6. `expected_version` casts a double to `uint64_t` unchecked (non-blocking)

`src/database/shard.cc:97`, `src/engine/xapian/shard.cc:155` and `src/engine/xapian/convert.cc:219` do `static_cast<rev>(sortable_unserialise(...))`. A client `?version=18446744073709551614` serialises as 2^64 as a double, and the cast is undefined behaviour (UBSan `float-cast-overflow` fires in `castprobe.cc`; on arm64 and x86-64 the result I measured is 2^63, never 0, so the answer is still a conflict, not a silent pass). Old code compared strings, so it had no such cast. Clamp or reject versions at or above 2^53.

### CR7. Item 1, deadlock analysis for the default and the on path

Default (pool off): `Hop::await_ready()` is true (`engine/xapian/shard.cc:46-49`), so the task body runs on the calling thread inside `sync_wait`; no thread waits on another, so it cannot deadlock. On (pool N): the caller blocks in `sync_wait`; the pool thread runs the body, resumes the `detached` driver, and signals under the mutex (`task.hh` `sync_wait`); a deadlock needs a pool thread to call `sync_wait` on the same pool, and none does (only `finalise_write` runs there). The queue is unbounded but holds at most one entry per blocked caller. Evidence: `test_engine_shard` checks the call really runs on another thread with the pool, E2E x7 pool on, hammer with a pool of one thread, `stallhunt` dump `stall-1-2.lldb` (pool thread idle while a request waits, so the wait is not on the pool).

### CR8. Pre-existing concurrency bugs found (not caused by this branch; reproduced on master)

1. `DatabasePool::cleanup` use-after-free under concurrent `FLUSH`: ASan `heap-use-after-free lru.h:107` at `pool.cc:1106`, two reactor threads in `flush_view`. Reproduced on master (`uafhunt.sh`, `uaf-8987.log`) and on the branch (`uaf-8988.log`), pool off. `pool.cc` differs from master only in exception types.
2. Lost wake-up in `ShardEndpoint::checkin`: `shard->_busy.store(false)` and `writable_cond.notify_one()` run without `mtx` (`pool.cc:422-423`), while the waiter in `_writable_checkout` evaluates its predicate under `mtx`. Symptom: a request waits for a shard nobody holds until its timeout (`stall-1-2.lldb`: one thread in `_writable_checkout`, no other thread in any shard, pool thread idle). Rounds of the 16-client hammer with a 20 s client timeout that lost a request: master 8 of 90 (three runs), branch pool off 14 of 90, pool of 1 thread 3 of 30, pool of 3 threads 2 of 30. The counts do not separate the branch from master at these sizes. The same hang is the likely cause of the one pool-on E2E failure (RESTORE `ESOCKETTIMEDOUT`, node log "Request took too long ~60 s", `e2e-on/`): 1 failure in 8 pool-on runs, 0 in 5 pool-off runs; I could not reproduce that exact request in 450 DUMP and RESTORE cycles, so attribution of that single failure is by mechanism, not proof.
3. `Shard::reopen` loops `DB_RETRIES + 1` times on success (no `break` after `reopen_writable` or `reopen_readable`; `shard.cc:664-690`, identical to master `shard.cc:569-595`). Observed: 11 consecutive opens per reopen (lldb hit counts 55 and 110, and "Read and execute operations WAL" printed 11 times per restart on master and on the branch). With the pool on each is a pool hop. Cheap fix, out of scope here.

### CR9. Documentation and ledger accuracy (non-blocking)

1. `XAPIAND_ENGINE_OFFLOAD_THREADS` is mentioned only in `main.cc`, `engine/xapian/shard.h` and the ledger; add it to `docs/dev/testing.md` with the "keep off" rule from CR1 and CR2.
2. The ledger says half B was proven "also with `XAPIAND_ENGINE_OFFLOAD_THREADS=4` (same result)". I got that result 7 times and one stall (CR8); say so.
3. The ledger "Seam count" is exact: `.scratch/seam.sh` gives 52 files and 1297 lines.
4. `src/engine/xapian/{convert,shard}.cc` carry MIT headers and include the GPL `sortable_serialise.h` to call `sortable_serialise` and `sortable_unserialise`. `src/database/utils.h` already did. It is a use, not a derivation; the licence note in `ARCHITECTURE.md` (the GPL list) should name these two files.

### Item 4. WAL compatibility: holds

`Document::serialise` / `unserialise` go through `Xapian::Document::serialise` / `unserialise` (`engine/xapian/convert.cc:229-241`), so the stored line is Xapian's own and old lines parse by the same function. `test_engine_document` (844 checks, 200 random call sequences, byte comparison against a Xapian document and round trip of `unserialise(xdoc.serialise())`) passes under ASan+UBSan. End to end: `walrec.sh` (the repo's `wal_recovery_check.sh` with a scratch run dir) PASS 50/50, pool off and on. `walreplay.sh` (30 documents, rewrites, version-checked updates, deletes, a recreate with `version=0`, the UPDATE verb, a refused stale update, all left in the WAL, `kill -9`, restart, replay) gives documents and `_version`s identical to a run without a crash, and identical to the same script on master's binary (master has no pool). I had no WAL file written by a pre-branch binary to feed to the new code, so "old lines replay" rests on the shared serialiser and on the master-built comparison above, not on an old file.

### Item 5. Clean room: no evidence of derivation

`git diff --stat 5cce4860d..HEAD` touches nothing under `src/xapian/`, nothing in `sortable_serialise.{cc,h}` and not `src/server/remote_protocol_views.cc`. The new engine files use only the public Xapian API (documents, term and position iterators, values, exceptions, `Database` and `WritableDatabase`) and Xapiand's own `Data`, `serialise_length`, `sortable_serialise` callers. The classification ladder follows the public exception hierarchy; I checked the hierarchy by compiling, not by reading the headers. Disclosure: one repo-wide `git grep` I ran for `lock_shard` call sites printed matching lines from `remote_protocol_views.cc`; that is call-site text from a search, not a read of the file. I also made a scratch copy of master with `git archive` to build the baseline and did not open anything in it.

### Other observations (non-blocking)

1. The design sketch has `struct Error : std::runtime_error`; the branch made `Error` a mixin that is not a `std::exception`, and added `errc::database`. The reason (an ambiguous `std::exception` base in the wrapper classes) is sound; the architect should update D9 and section 3.
2. `engine::Document::serialise` is declared in the engine-neutral header and defined in `engine/xapian/convert.cc`, so any user of `engine::Document` links the Xapian implementation. Fine until S1; D10 will need a seam for it.
3. Build warnings from this branch's files: none new in `src/engine`; the `schema.cc` shadow warnings and `handler.cc` unused constant I did not attribute.

## What I ran

Build: `QUIET_LEVEL=light quiet run review-engine`, 4 jobs, nice 19, ASan+UBSan (`-fsanitize=address,undefined`, RelWithDebInfo, `-DBUILD_TESTS=ON -DLTO=OFF`), x86_64 build at `.scratch/review/build-asan`; master `5cce4860d` built the same way at `.scratch/review/build-master` for baselines.

1. Unit tests: `ctest` 10 of 10 pass; `test_engine_document` 844 checks, `test_engine_shard` 52 checks.
2. Docs E2E (339 requests, 576 assertions, newman 6.2.3 from `phase0.5-analysis/newman6`, `e2e_assert.py` and `e2e_diff.py` against the X1 final capture): pool off 5 runs GREEN; pool on (4 threads) 7 runs GREEN and 1 run with one RESTORE timeout (CR8). UBSan: six lines per run, all third party (`lz4.c:549`, `lz4.c:1069`, `rapidjson/internal/stack.h:117` x4); none in `src/engine`, schema, shard, WAL or handler. No ASan report in any E2E run. (A first attempt with newman 4.5.0 failed every request; newman 6 is required.)
3. `wal_recovery_check.sh`: PASS, pool off and on (3 threads).
4. `walreplay.sh`: SAME, pool off and on; same on master.
5. `versions.sh`: identical to master; identical pool on.
6. `ladder_probe`: no difference in 21 types x 2 retry counts x 3 ladders (CR3, CR5).
7. `hammer.sh` (16 clients index, delete, commit, FLUSH): no deadlock, no ASan report with pool sizes 1, 3 and off when FLUSH comes from one client; with concurrent FLUSH from all clients ASan finds the CR8 item 1 use-after-free, on master too.
8. `restore_stress.sh` (8 concurrent restores of 300 documents, pools 1, 4, off), `dump_restore_loop.sh` (150 cycles each, pools 4, 1, off): no stall.
9. `lldb_threads.py`: which threads reach `Offload::post` (CR1).
10. `stallcount.sh`, `stallhunt.sh`, `uafhunt.sh`: CR8.

## Probes and files kept (all in `.scratch/review/`)

`build.sh`, `build-master.sh`, `e2e.sh` (output `e2e-*.out`, per-run directories `e2e-on*`, `e2e-off*`), `walrec.sh`, `walreplay.sh` (`walreplay-ref*.txt`, `walreplay-replayed.txt`), `versions.sh` (`versions-0.out`, `versions-3.out`, `versions-master.out`), `ladder_probe.cc` (`ladder_probe.out`), `hier.cc`, `castprobe.cc`, `lldb_threads.py` (`lldb_threads.out`), `lldb-post.cmds`, `hammer.sh`, `stallhunt.sh` (`stall-1-2.lldb`), `stallcount.sh` (`sc-*.res`), `uafhunt.sh` (`uaf-*.log`), `restore_loop.sh`, `restore_stress.sh`, `dump_restore_loop.sh`, `node.sh`, `shard_old.cc` (master `shard.cc` for side-by-side reading).

## Not run

- `cluster_check.sh 2` and `3`, and every remote protocol and replication path (only read; the ledger reports them GREEN with the pool on).
- TSan (Apple clang's TSan is unreliable on this host; the pool-on concurrency evidence is ASan plus stress, not a race detector).
- Linux and FreeBSD builds; CI on the other platforms; `loadtest.py` and any benchmark (not allowed).
- A WAL file written by a pre-branch binary replayed by the new code; the load and soak layers; the TLS layer.
- Fault injection into the retry ladders (no hook exists; CR4).
- A statistically meaningful pool-on E2E count (8 runs); 20 or more on CI hardware would settle whether the single RESTORE stall differs from the pool-off rate.
