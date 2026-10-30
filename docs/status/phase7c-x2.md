> Status: CURRENT (2026-10-30) the X2 developer's ledger (phase 7c step X2: the index registry, the lease, host mode and chores); K1 to K4 and K6 to K7 done, K5 and the reader views are X2b.

# phase7c X2 (build): the registry and lease, host mode, chores

Branch `phase7c/x2-registry-lease` in `Xapiand-wt/x2` (local only, from master `e265acee6`). Design: `docs/design/phase7c.md` (D1, D4 to D7, D9,
D14, D16, section 5, rows T1 to T13); review: `docs/reviews/phase7c-design.md` (Re-check 1: R4, C6, C7, C16). Mechanism note:
`docs/mechanisms/index-registry-and-lease.md`.

## State

Ready for the code review (concurrency code). K1 to K4, K6 and K7 are done and ticked below. K5 is partial and the shared reader views
(D6's `core.mtx` and `Pinned`) are not built: both are **X2b**, with the handler's conversion to tasks (see "Next"). Nothing is blocked.

## Done criteria, ticked

- [x] **K1 Pins.** task `9a85e89`, threadpool `e961e44`, strata `c82ac9f` (`CMakeLists.txt`; `Xapiand:f8657f445`). No prospect commit pins strata
  `c82ac9f` (prospect main `b348da5` pins `16e9054`), so Xapiand declares strata ahead of prospect, where the first declaration wins; prospect
  `b348da5` compiles and passes against it.
- [x] **K2 Registry and lease (D5, R4)** (`Xapiand:f4f0fd319`; tests in the commits listed under Done). `src/database/pool.{h,cc}`, `shard.cc`.
  - *Opening never waits*: `test_opening_never_waits_on_a_thread` opens one index from two uses on one thread under `task::when_all`; a thread
    that blocked on the open could not run the chore completion it waits for. `test_concurrent_opens_are_one_flight`: eight threads, one open
    (`opens() == 1`). A failed open reaches all four waiters and is retried (`test_a_failed_open_reaches_every_waiter_and_is_retried`).
  - *Every operation takes its turn, timers included*: `test_every_operation_takes_the_turn` (add, replace, replace by term, delete, delete by
    term, metadata, commit, and the refresh a read owes) holds the turn from another thread and shows each waits; `test_the_autocommit_task_takes_the_turn`
    and `test_the_idle_close_takes_the_turn` do it for the two timer tasks; a read with nothing to refresh takes none.
  - *One turn per task*: `Turn` counts the turns a task holds on its `Host` and asserts at most one; `test_a_task_holds_one_turn_at_most`
    forks a child that takes two and expects SIGABRT (Debug builds).
  - *Turns time out with 503*: `test_a_turn_wait_times_out_with_busy` (`engine::errc::busy`, mapped to 503 in `catch_http_errors`, row added to
    `test_engine_error`); the waiter leaves nothing behind.
  - *Close protocol*: `test_a_request_racing_an_idle_close` (a lookup that finds the entry closing awaits the close, never opens a second engine
    beside it, then opens a new one; what the close committed is read back) and `test_a_stale_engine_is_replaced_before_a_queued_write` (a
    writer that gets the turn of a stale engine gives it back and the write lands on the new one).
  - *Mutants* (`.scratch/mutants-pool.sh`, each killed by the named test): M1 single flight off -> `test_opening_never_waits_on_a_thread`,
    `test_concurrent_opens_are_one_flight`, `test_a_request_racing_an_idle_close`; M2 a write skips the turn -> `test_every_operation_takes_the_turn`
    and three more; M3a the commit skips the turn -> `test_the_autocommit_task_takes_the_turn`, `test_every_operation_takes_the_turn`; M3b the
    close skips the turn -> `test_the_idle_close_takes_the_turn`; M5 the wait has no deadline -> `test_a_turn_wait_times_out_with_busy` (hung);
    M6a a lookup ignores `closing` -> `test_a_request_racing_an_idle_close`; M6b no recheck after the turn ->
    `test_a_stale_engine_is_replaced_before_a_queued_write`.
- [x] **K3 Host mode (D6, C3, C6)**.
  - `engine::strata::Shard` opens with `defer_io` (`OpenOptions::host`); `apply_t` runs the owed flush or drain as a flush chore and applies the same
    transaction again before any other write (`test_owed_flush_runs_as_a_chore_and_the_write_is_retried`: a buffer of three documents, ten
    writes, three flush chores, ids 1 to 10 in order, none of the work on the writer's thread; at the registry,
    `test_owed_flushes_do_not_reorder_writers`: four threads, ids contiguous, none repeated, each thread's rising). Mutant M7 (the owed answer is
    not retried) -> killed by `test_owed_flushes_do_not_reorder_writers`.
  - One turn covers apply, revision update and WAL enqueue (`ShardEndpoint::do_*`): `test_the_wal_order_is_the_apply_order` writes from four threads with
    the real WAL writer and reads the log back: the replaced ids reach it in ascending order. Mutant M2 (no turn) is killed by it too; the harness
    check below is the system-level proof.
  - **T3's crash case**: `harness/engine_crash_check.sh --reactors 2` (new section D): two writers on two HTTP reactors into one one-shard index,
    `kill -9` after 4 s, restart, poke, list. Result: PASS, 9,711 documents recovered, none acknowledged more than 2 s before the kill missing, each writer's
    recovered set is a prefix of its sequence (so the log's order is the apply order), the strata digest check passes before and after the restart.
    Sections A and B (single writer, 5 shards) pass too. `harness/wal_recovery_check.sh`: PASS (recovered 50/50, replay logged).
  - C7, named: `test_a_commit_joins_the_one_in_flight` (a commit whose writes the commit in flight covers joins it: one revision for two callers; a write
    that arrives meanwhile waits for the turn and for the next commit), `test_a_commit_not_covered_does_not_share_the_outcome` (a position past the
    commit's does not join, and does not receive its failure; mutant M9, join without covering, killed by it) and
    `test_a_failed_commit_reaches_only_the_joined_waiters`.
- [x] **K4 Chores (D4, D7, D9, C16)**.
  - Open, refresh, commit, close, merge publication and reclaim are chores of the entry's `WorkGroup` (`IndexChores`); strata's merges run on the
    host executor (`strata::work_pool_executor`, merge class). `storage_commit` is the first flush chore of the commit and the engine's commit
    (its fsync) the second: `test_blob_io_runs_in_chores` counts them. Blob writes are flush chores inside the document's turn, before the apply,
    and `storage_get_stored` is a query chore on a reader of its own (same test).
  - The idle task runs `reclaim()` as a merge chore when background work is owed (`ShardEndpoint::housekeeping`, from `DatabaseCleanup`'s
    minute): `test_reclaim_is_a_merge_chore`, `test_housekeeping_costs_nothing_on_a_quiet_index`. **C16's second half is not built**: views that
    readers released stay unfreed until the shared reader views exist (X2b), so there is no guard to hold.
  - No chore waits on its own group (AC2): the WAL replay and the engine open run in the open chore with the engine's chores inline, and the
    engine then takes the group's; nothing in X2 calls `WorkGroup::wait` from a chore. `block_guard` is `engine::check_may_block`
    (`DatabasePool`, test_blocking_guard).
  - C7: see K3.
- [~] **K5 `sync_wait` out of the server paths** (partial). `engine/shard.cc`'s `sync_wait` calls are gone: the shard and the registry are tasks, the
  open chore waits on the engine only on a worker, shutdown waits on the main thread, and `engine::sync_wait` now resumes everything on the waiting
  thread. The handler still calls its shard operations through `engine::sync_wait` (33 sites in `handler.cc`, 2 in `schema.cc`): views and the
  schema code are synchronous until the handler is converted to tasks (X2b), so `grep -rn sync_wait src/` still lists them.
  `harness/async_check.sh --strict`: 0 unclassified (310 hits: 141 allowed, 156 owed, 13 compiled out); the X2 rows moved: the pool's waits and
  checkouts, the shard's `sync_wait` (7 left, all chore code), the blob I/O rows are gone; the handler's rows say X2b. `block_guard` = `engine::check_may_block`.
- [x] **K6 Proof.**
  - Release and Debug (`Debug`, asserts on) builds link; unit tests **26 of 26** ctest targets on each (Debug includes the one-turn death test and the
    assertion mutant M4, killed by it).
  - Docs E2E, Release: 371 requests, **641 of 641** assertions (`harness/engine_e2e.sh`, newman 6.2.3).
  - ASan+UBSan (Homebrew LLVM 22, `RelWithDebInfo`, one build): unit tests 26 of 26 (`detect_leaks=0`), docs E2E 641 of 641, no report. The first ASan run
    of `test_pool` found a real bug: `pool.h` did not include `config.h`, so `ShardEndpoint` had a different layout in `pool.cc` (no
    `XAPIAND_DATA_STORAGE` member) than in `shard.cc`, a heap overflow in the first open (fixed in `Xapiand:f4f0fd319`).
  - TSan (one build): the concurrency tests (`test_pool`, `test_engine_host`, `test_engine_strata`, `test_wal_replay`, `test_engine_error`, `test_blocking_guard`,
    `test_offload_followups`, `test_debounce_or_run`): 8 of 8, 0 warnings (the first run found a data race in the test itself: a `CHECK` in a helper thread; fixed).
    `harness/engine_mixed_load.py --engine strata --secs 120` on the TSan server: model equals the final dump (15,773 documents), 0 failures, 0 sanitizer reports.
  - `harness/stall_load.py --dry-run` on the Release server: passes (20,000 documents loaded, unloaded and loaded phases, 0 errors, JSON written; no timing
    claim from a dry run on a shared host).
  - `harness/wal_recovery_check.sh` PASS; `harness/engine_crash_check.sh --reactors 2` PASS (sections A, B, D).
- [x] **K7** `docs/mechanisms/index-registry-and-lease.md`; changelog (Changed: 503 on a turn timeout, the device work on a shared pool; Removed:
  `--max-database-readers`).

## Done (commits)

See `git log master..` for the branch; the substance:

- `Xapiand:f8657f445` pins (K1).
- `Xapiand:f4f0fd319` the registry, the lease, host mode and chores: the engine, the pool, the shard, the WAL target, the manager's control host, the
  handler's calls.
- the registry tests and crash check D; the allowlist, blob and housekeeping tests, the mechanism note and the changelog; the final commit adds this ledger,
  the busy rows of `test_engine_error`, the TSan test fix and the shutdown guard in `_spawn`.

## Next

- **X2b** (the lead's call): (1) the handler's operations as tasks, then the views and the schema callers (D3, D11), which removes the 33 + 2
  `sync_wait` sites and the offload threads' blocking; K5 closes there. (2) The shared reader views (D6): `Pinned` and `core.mtx` go, a search takes a
  view guard per shard after awaiting the refresh chore, and C16's held-guard test becomes possible. (3) The turn's deadline from the request's
  remaining time (X3).
- The lead: the code review of this branch (concurrency), then land.

## Blocked on

Nothing.

## Log

### 2026-10-30

- Read: the design (D1, D4 to D7, D9, D14, D16, section 5), the review's Re-check 1, task `task.hh` and `task_asio.hh` and its mechanism note,
  threadpool `work_pool.hh` and its note, strata `index.hh`'s host mode and its note, and Xapiand's `pool`, `shard`, `handler` call sites, `wal`, `manager`.
- Choice that shaped the cut: views, the schema code and Lua are synchronous callers (X3, X4, X5), so the registry is tasks all the way down and the
  handler keeps one explicit bridge, `engine::sync_wait`, which now runs a task on a `BlockingScheduler` that belongs to the waiting thread (every resume is
  posted to its queue, so a turn handed over never runs inside another task's stack, and a turn's deadline is a timer in that queue). The control loop's timer
  tasks run on `AsioHost`.
- Not done on purpose: no new fuzz target (no decoder or parser was added or changed; WAL replay's parser is untouched), and the cluster-era retry ladders of
  the write path are gone (a failure marks the entry stale and the next use replaces the engine).
