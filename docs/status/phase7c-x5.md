> Status: CURRENT (2026-11-02) the X5 developer's ledger (phase 7c step X5: the schema and settings caches, the control plane as tasks, inventory J); built and proved; reviewed (approve with conditions); the review's blocker and notes N1, N4, N5 are fixed below; not pushed.

# phase7c X5 (build): the caches, the control plane and inventory J

Branch `phase7c/x5-control` in `Xapiand-wt/x5` (local only, from master `Xapiand:d5a2e746c`). Design: `docs/design/phase7c.md` (D1, D11, D13, D14, D16, section 5's X5); the review: `docs/reviews/phase7c-design.md`
(C10, C14); what X2, X2b and X3 converted: `docs/status/phase7c-x2.md`, `phase7c-x2b.md`, `phase7c-x3.md`. How it works: `docs/mechanisms/control-plane.md`.

## State

Built and proved on Release, Debug, ASan+UBSan and TSan; ready for review. Nothing is blocked. Overlap with X4 (`phase7c/x4-lua`): none in `src/lua/` or `src/script.*`. The branches will conflict textually in
`harness/async_allowlist.tsv` (X4 retires the Lua rows), `CMakeLists.txt` (the test list on one line), `docs/src/content/docs/changelog.md` and possibly `manager.cc`.

## What was there, and what is built

- **Already converted before X5**: the schema and resolver code were coroutines (X2) and no lock in `schemas_lru.cc` or `index_resolver_lru.cc` is held across a `co_await`. What the caches lacked was the single flight: N concurrent
  first uses of one schema made N reads. `manager.cc:834` (the setup sleep) and `kind_of` are gone with earlier steps; the census is compiled out.
- **K1** `SingleFlight<Key, Value>` (`src/single_flight.h`), used for the schema metadata read, the foreign schema read (only on a first use, only outside a nested load) and the settings read (not on a reload).
  Two hazards were found building it: a coroutine given `Endpoint(foreign_path)` through a lambda gets a dangling reference (the temporary ends with the lambda's `return`), so the flight's load builds the endpoint in the caller's frame; and a
  nested load (non-empty `context`) never joins a flight, or a load could wait for itself (seen once in a trace as a join on a flight its own task led).
- **K2** `src/control_plane.h`: `control::Tasks`, `Periodic` and `Debounce`. In `manager.cc`: node setup, the shutdown checks and the signal reader are tasks on the control loop; the minute housekeeping is a `Periodic`; the autocommit is a
  `Debounce<Endpoint>` (`XapiandManager::schedule_autocommit`, called from `Shard`'s write path). Deleted: `control_offload`, the control-plane `asio::thread_pool`, `DatabaseCleanup` (`database/cleanup.{h,cc}`), the AC debouncer (`committer()`).
  The node name is read and saved by `run()` on the main thread. `join()` stops the housekeeping, cancels the signal read and runs the stopped loop until the control tasks end, then stops the autocommits after the HTTP service.
- **K3** `preloadStopWords()` and `preload_mime_types()` run in `XapiandManager::init()`; a lookup reads an immutable table.
- **K4** C10: the comment on `TASK_ASIO` in `CMakeLists.txt` says what the Asio binding is used for and that a chore resumes with `post()`. C14: `shutdown_wait::wait` and `XapiandManager::shutdown_write_bound` (35 s).

## Done criteria

- [x] **K1. The caches hold no lock across I/O; concurrent first use of one schema loads it once.**
  - `tests/test_node.cc` `test_concurrent_first_uses_of_a_schema_load_it_once`: six threads, the index's turn held; while held, 1 load and 5 waiting; after, 2 loads in all (the metadata, and the foreign schema it links to) and every caller gets the one
    schema object. `test_concurrent_first_uses_of_index_settings_load_them_once`: the same for the resolver (1 load, 5 waiting). `test_a_cached_schema_is_not_loaded_again`: late uses load nothing.
  - `tests/test_control_plane.cc` `SingleFlight` cases: one load for eight callers, other keys load beside each other, a failed load reaches every waiter and frees the key, a cancelled leader leaves its waiters to load, a destroyed leader frame does not
    strand them.
  - Fail with the old behaviour (mutants on Release): **M1** flights off in `SchemasLRU` (6 loads, 0 waiting: 3 checks fail); **M2** flights off in the resolver (3 checks fail); **M3** the old shape, a `std::mutex` held across the metadata read (the joiners block
    on it, never reach 5 waiting: fails).
  - The lock classification: `async_check.sh` rows `schemas_lru.cc` mutex (19), `index_resolver_lru.cc` mutex (3): map lookups, inserts and trims under the lock, released before a read (D11).
- [x] **K2. The control plane's work runs as tasks on the control loop; nothing blocks it.**
  - `test_the_control_loop_is_free_during_a_slow_commit`: a write's autocommit fires on the loop and its commit chore is held 400 ms; the loop's probe (1 ms interval) wakes more than 150 times in the window, none 50 ms late, p99 at most the 16,384 us edge.
    `test_the_control_loop_is_free_during_a_schema_load` and `..._a_settings_load`: a schema read, and the resolver's lookup, run as tasks spawned on the control loop while the index is busy 400 ms; 20 other control tasks run meanwhile and the same bounds hold.
    `test_the_housekeeping_ticks_on_the_control_loop` (100 ms period). Building blocks: `Tasks`, `Periodic` (a stop ends a one-hour wait at once), `Debounce` (a burst is one commit after the quiet time, a write every 10 ms is committed by the forced time,
    a stop ends the watchers without firing, 5,000 keys leave the loop's p99 under 16 ms).
  - Mutants: **M4** the autocommit waits for its commit on the loop (the probe woke 150 times or fewer: fails); **M5** `Periodic::stop` does not wake the wait; **M6** a touch does not move the target; **M7** the forced time is not capped; **M8** a stop leaves the
    watchers; **M9** the housekeeping never starts. All killed. **M10** the signal reader never spawns: see the proof section.
  - `harness/graceful_shutdown_check.sh`: scenarios 1 to 3 (SIGINT with and without a client, twice) and scenario 4 (SIGTERM at 0.5, 1.0 and 1.5 s after a write whose autocommit has not fired: exits clean, the write is there after a restart) pass on all builds.
- [x] **K3. Inventory J classified.** `harness/async_check.sh --strict --extern strata=<strata>`: **0 unclassified**; 245 hits (201 allowed, 31 owed, 13 compiled out). The owed rows left name D10 (4, X4), D4 (4), D5 (2), D6 (4), D7 (2) and X1 (15): none names D1, D11 or D13. The five J items:
  `kind_of` is gone (X1 and earlier); stop words and `mime.types` are read by the preload (test: `test_stop_words_and_mime_types_are_read_once_at_startup` reads them, deletes the files and looks them up; mutants **M11** and **M12**, a preload that reads nothing, fail);
  the census is `compiled_out`; strata's spinning `published_position()` has no caller in Xapiand, strata or prospect (grep), so its row is `allow` with that reason (the callers use `try_published_position()`).
- [x] **K4. C10 and C14.**
  - C10: `test_pool.cc` `test_a_chore_resumes_its_awaiter_with_post` counts `post` and `post_demoted` on a spy scheduler while five chores finish on a worker: 0 demoted, at least 6 posts. **M14** (`s.post_demoted(h)` in `ChoreAwaiter`) fails it.
  - C14: the bound is `XapiandManager::shutdown_write_bound`, the longer of `DB_TIMEOUT` (30 s) and the reactors' grace (5 s), plus 5 s: 35 s. `tests/test_control_plane.cc`: a stop that returns is not abandoned; one that does not return is reported at the bound with
    the lines `describe()` gives, and ends later when it can. `tests/test_node.cc` `test_a_shutdown_abandons_what_it_cannot_finish_within_its_bound`: a child node (bound 1.5 s) whose PUT waits for an index turn held by a thread of its own is told to shut down; it logs
    `Shutdown abandoned 2 unfinished items after waiting 1508 ms` with `1 request in flight on http0` and `'xapiand://nodetest/abandon' (turn held, 2 references)`, closes the registry for at most another bound, finishes the WAL writers and the fsynchers,
    and exits 75 (Release, Debug, ASan and TSan, where `test_node` is one of the 11). `test_the_registry_lists_what_still_uses_an_index` covers `DatabasePool::busy()`. Mutants: **M13** the wait ignores its bound, **M15** the report ignores a held turn, **M16** the exit status is 0 instead of 75: killed.
  - What C14 asked beyond the bound is X6/L3's (D16's order, cancelling unfinished body reads, `Reactor::track` O(1)): `HttpAsioService::stop()` already cancels reads at their next checkpoint and closes stalled sockets after the grace.
- [x] **K5. Proof** (below).

## Proof

Release, Debug and ASan+UBSan (`detect_leaks=0`) and TSan are separate builds of the branch head. `Xapiand:7150e3cca` carries the table; `Xapiand:1a6e3e140` changes one branch of `join()` (the abandoned path) and was rebuilt in all four and re-run
through ctest: Release, Debug and ASan 29 of 29, TSan 11 of 11 with 0 reports.

| at `Xapiand:7150e3cca` | Release | Debug | ASan+UBSan |
|---|---|---|---|
| ctest | 29 of 29 | 29 of 29 | 29 of 29 |
| docs E2E (newman 6.2.3, `harness/engine_e2e.sh`): 371 requests | 641 of 641 | 641 of 641 | 641 of 641 |
| `graceful_shutdown_check.sh` (scenarios 1 to 4) | PASS | PASS | PASS |
| `wal_recovery_check.sh` (50 of 50 recovered) | PASS | PASS | PASS |
| `cli_roundtrip_check.sh` (`--dump` and `--restore` exit 0, same sha256) | PASS | PASS | PASS |

- No "blocks on database work" line and no sanitizer line in any E2E node log.
- **TSan**: the concurrency targets (`pool`, `engine_host`, `engine_error`, `blocking_guard`, `wal_replay`, `engine_strata`, `engine_search_strata`, `debounce_or_run`, `reactor_requests`, and the new `control_plane` and `node`): 11 of 11, 0 reports.
  `harness/engine_mixed_load.py --engine strata --secs 120` (126 s): 11,809 documents, the final dump equals the model, 0 failures, 0 sanitizer reports, the node exits 0.
- **Mutants**, one per rule, on the Release build (`.scratch/mut.py`: apply, build one target, run one test, restore): M1 to M9 and M11 to M17 killed as listed above (M3 and M4 in the way the K1 and K2 entries say); **M17** `shutdown_wait::wait` without
  `check_may_block` (killed by `test_blocking_guard`); **M10** the signal reader never spawned (killed by `graceful_shutdown_check.sh`: 6 scenarios time out, the node never sees SIGINT or SIGTERM).
  Not killed or not run: none outstanding. Not covered by a test: the 60 s period itself (the tests run the task at 100 ms), the wiring of `schedule_autocommit` from every write path (the slow-commit test uses `Shard::add_document`; the other write paths call the
  same `finish_write`).
- Every new decoder or parser: none. The stop-word and `mime.types` loaders are unchanged readers moved to startup, so no fuzz target was added.
- The tests added: `test_control_plane` (19 cases), `test_node` (8 cases: a node in the process on port 18781, a child node on 18782), two cases in `test_pool`, one in `test_blocking_guard`.

## Findings for the lead

1. **The control loop now runs `database_pool->cleanup()` and `housekeeping()` every minute** (D13 moves them from the cleanup thread). `cleanup()` trims the registry's LRU under the pool lock and `housekeeping()` copies a referenced pointer per entry, so each is O(entries) on the loop. T13's
   registry scan cost (C9) should be read against the control loop's lag at 10,000 open indexes; at that size either belongs in chunks.
2. **The control loop's probe sits at its 100 ms idle interval** unless something calls `note_activity()` on it; `Tasks::spawn` does not. The tests call it from a pulse thread. T4's reading of the control loop (the S1 finding) is unchanged by X5.
3. `--committers` no longer sizes anything (D15 removes the option); the `xapiand_committers_*` gauges report watch tasks and waiting keys.
4. The exit status 75 on an abandoned shutdown is new (`EX_TEMPFAIL`); `graceful_shutdown_check.sh` never reaches it.

## Review fixes (docs/reviews/phase7c-x5-code.md)

Commit `Xapiand:e6c18eada`. Release build; the sanitizer results are in the proof table below.

- **X5-B1** `setup_node_task` and `shutdown_checks` catch and call `XapiandManager::control_task_failed`, which logs and takes the `shutdown_sig(sig < 0)` path (status 70, or the `SystemExit` code). Per control task:
  1. node setup: fatal (a half-set-up node serves nothing);
  2. shutdown checks: fatal (a half-done shutdown);
  3. signal reader: a signal that fails to run is logged and the next is read (before, one throw ended the reader and the node could no longer be told to stop);
  4. housekeeping tick: logged, the next tick runs (`Periodic`);
  5. debounced autocommit watch, autocommit, idle close, housekeeping per entry: logged, the next write or tick tries again; the log and the close commit what is pending.
  Probe `bind_conflict.sh`: the second node exits with status 70 in under 1 s, logging `The node setup failed: ending the process with status 70` and `bind: Address already in use` (review: stayed alive). Test `test_a_node_on_a_taken_port_ends_the_process` (a child node on a port a listener holds: exit 70 within 15 s, both lines logged). Mutant: the setup swallows again: killed (status not 70, 3 checks).
  Not tested: a failure in `shutdown_checks` (no seam to make it throw).
- **X5-N1** `DatabasePool::housekeeping(slice = 256)` runs a pass as a detached task: 256 entries per turn under the pool lock, a task only for an entry that owes work (`ShardEndpoint::housekeeping_due`), `task::yield()` between turns, one pass at a time. The review's probe at 10,000 open entries, a 1 ms ticker on the loop, three ticks, under `quiet strict`:

  | | `housekeeping()` call on the loop | until its tasks drained | max ticker gap |
  |---|---|---|---|
  | before (the reviewed code) | 5.6 to 8.1 ms | 14.5 to 41 ms | 14.9 to 41.1 ms |
  | after | 0.001 to 0.006 ms | 3.0 to 4.7 ms | 1.2 to 1.5 ms |

  (The ticker is 1 ms, so 1.1 ms is its floor.) `endpoints()` still copies under the pool lock for `busy()`, `dump_databases()` and the SIGUSR1 dump (0.1 to 0.3 ms at 10,000); the housekeeping no longer calls it. Test `test_pool.cc test_the_housekeeping_pass_works_in_slices`: 30 quiet indexes in slices of 4 are 8 turns and no task, and an index that owes a released view is found and freed. Mutants: no slicing (turns 1, not 8), no filter (30 tasks): both killed.
- **X5-N4** a SIGINT or SIGTERM counted from the start of `join()` (`shutdown_signals`, incremented by the handler) ends `shutdown_wait::wait` (polled every 50 ms) and the registry's extra wait. The documented worst case is the graceful wait for clients plus 70 s (the bound twice; the 60 s drains only if a task never ends): `docs/src/content/docs/options.md` ("Stopping the node"), the changelog and the note. I kept the second bound; making the whole stop one bound is a choice for the lead. Test `test_a_second_signal_ends_the_shutdowns_wait` (bound 60 s, a second SIGTERM 1 s in: exit 75 in under 20 s, the log says `stopped by a second signal`). Mutant: the interrupt ignored: killed.
- **X5-N5** `test_a_shutdown_abandons...` now acknowledges 5 PUTs (2xx) to another index while a write waits on a held turn, shuts down at once (before the 200 ms autocommit), and a second child restarts the data directory and reads all 5 back. The property holds on the code as it is. Sensitivity: a mutant that exits at the abandonment and drops the WAL append loses the writes and the restart check fails with 5 missing; on its own the early exit does not lose them (the append is before the acknowledgement), so only its log-line check dies.
- **Proof of the fixes** (the head, separate builds): Release ctest 29 of 29; ASan+UBSan ctest 29 of 29; TSan on the 11 concurrency targets (with `test_node`, whose forked children are the new tests) 11 of 11, 0 reports; docs E2E 641 of 641 on Release and ASan; `graceful_shutdown_check.sh`, `wal_recovery_check.sh` and `cli_roundtrip_check.sh` PASS on both. The restart child of the abandon test failed once in 10 ASan runs with an empty first response (the listener not yet accepting when the state turned READY); `connect_to` now retries for 5 s, and 14 runs after were clean.
- **X5-N3** left for X6: `--committers` (`src/opts.cc:166`), the `num_committers` stats key (`src/server/search_views.cc:756`), the stale thread diagram in `ARCHITECTURE.md:96`.
- Other findings (N2, N6, N7) are notes without a fix owed; N6(a) (`SingleFlight::run` precondition: a suspended joiner's frame must not be destroyed) is still to be recorded beside the code when a step starts destroying request frames.

## Done (commits)

- `Xapiand:17bcf2f1b` K1, K3, and the building blocks: `SingleFlight`, `control_plane.h`, `shutdown_wait.h`, the stop-word and mime preloads, and their tests.
- `Xapiand:ebdd26b19` K4, C10: `DatabasePool::busy()`, the chore-resume test, the `TASK_ASIO` comment.
- `Xapiand:f337d1e05` K2, K4: the control plane as tasks, the shutdown bound in `join()`, `tests/test_node.cc`, the docs, the allowlist and this ledger.
- `Xapiand:f7543779f` an abandoned shutdown exits without destroying what its threads use (found by the Debug build); the housekeeping period and tick count; the startup-read test.
- `Xapiand:451d88b80` the resolver's lookup as a control-loop task; `Xapiand:32ee5feb2` the changelog line for the gauges.
- `Xapiand:7150e3cca` the allowlist rows for the new headers (the strict check had 13 unclassified) and the guard on the bounded wait.
- `Xapiand:1a6e3e140` the autocommit scheduler stays up after an abandoned stop (a late write would otherwise commit on a reactor thread).
- `Xapiand:e6c18eada` the review's blocker and notes X5-B1, N1, N4, N5 (above).

## Next

- Review (the proof auditor's grade, then the code reviewer). Sensitive areas touched: shutdown (`join()`), the autocommit path.
- X6: D15's options (`--committers`, `--http-servers`), the destructor asserts, D16's order with `--strict --owed`.
- The lead: the T4/T13 reading above; delete `.scratch` build directories when disk is needed.

## Blocked on

Nothing.

## Log

### 2026-11-02

- Read the design (D1, D11, D13, D14, D16, X5), the design review (C10, C14), the X2, X2b and X3 ledgers, `manager.*`, `schemas_lru.*`, `index_resolver_lru.*`, `cleanup.*`, the debouncer and the control host.
- The first node test hung with every thread idle. A trace of the flight keys showed a join on a flight the same call led and a resolver lead with a garbage key; the garbage was the dangling `Endpoint` temporary (above). Both went with that fix. The Debug build found
  a second bug: the registry's destructor asserts on the references an abandoned write holds, so an abandoned shutdown now exits before it destroys anything.
- The mutants run through `.scratch/mut.py` (apply, build one target, run one test, restore) on the Release build, while no other build ran; the sanitizer builds of the head were made before and after, never during. The scripts and logs are kept in
  `~/code/Kronuz/Xapiand/.scratch/x5-tools/`.
