> Status: CURRENT (2026-11-04) the X-POOL developer's ledger (phase 7c: Xapiand adopts strata's cold-tier catalog); local branch `phase7c/x-pool` from master `Xapiand:88892b4cb`, not pushed.

# phase7c X-POOL (build): the registry becomes the catalog's payload

Branch `phase7c/x-pool` in `Xapiand-wt/x-pool`. Spec: `strata/docs/design/cold-tier.md` revision 7
(`design/cold-tier`, strata:5723f3d), "Steps", "Xapiand adopts it". Design: `docs/design/x-pool.md`
revision 2, with the architect's host-interface rulings (`strata/docs/reviews/cold-tier-xpool-host.md`).
Step 2 builds against strata item 5b (`phase/cold-tier-step2-item5b`) with
`-DFETCHCONTENT_SOURCE_DIR_STRATA=<a strata worktree>`. Step 1 uses master's strata pin.

Host rule (the owner, 2026-11-04): no AWS; Linux sanitizer batches run on the local QEMU VM, through strata's
`tools/vm/aws-linux.sh ssh` with `STRATA_AWS_VM_ALLOW` unset (its light hold), in task-named `~/xpool3`.

## State

Step 3 built on `phase7c/x-pool-step3` (Xapiand-wt/x-pool-step3), stacked on step 2's review commit
`Xapiand:b6b9718c2`, against strata item 5b `strata:c39e716`: the autocommit settles merges and re-arms (D8), the
sweep's times (`IndexSweep`), XP3 and XP4 tests, CR4, CR5's decisions (design D13). Proof in "Proof (step 3)". Waits
on CR1 (strata) for the pin's cancel token, CR3's property test and CR4's test.

Step 2 (the registry as the catalog's `H`, pins with intents) built against strata item 5b (`strata:c39e716`,
`-DFETCHCONTENT_SOURCE_DIR_STRATA=<strata-wt/cold-tier-step2-item5b>`); it also carries the step 3 pieces the lead put
in step 2's scope (autocommit and FLUSH on resident pins, C24, C25, the `map_asked_` note). proof in "Proof (step 2)": macOS and Linux (local VM), plain, ASan+UBSan and
TSan, all green with 0 sanitizer reports. Repin to strata main when items 5 and 5b land.

## Done criteria

- [x] `docs/design/x-pool.md`: decisions, gate rows, the rulings on F1 to F6 (revision 2, L2, XH8).
- [x] Step 1, the seam: `IndexPin` over today's `refs`, `lock_shard` awaited (`co_await lk_shard.pin()`)
  at all 28 construction sites plus `dump_page`'s borrowed one, the pin-before-view check (XP1), the
  autocommit's resident pin.
- [x] Step 1 proof on Linux: see Proof.
- [x] Step 2, the registry as `H` (against item 5b): XP1, XP2; with C24, C25, the autocommit's and FLUSH's resident
  pins, and restore's create pin.
- [x] Step 3, commit chore, idle close and housekeeping gone, shutdown, FLUSH by snapshot: XP3, XP4 (the autocommit's
  re-arm; see Design vs code 11 for XP4's sweep row).
- [ ] Step 2 review: CR1 (strata), then `pin`/`pin_chore` pass the request's token, CR3's pool-level property test, CR4's
  test; CR4's fix and CR5's decisions are in; CR6 owed.
- [ ] Step 4, `map_owed`, the `unleased_maps()` assertion, the budget and CT-R39's clamp: XP5.
- [ ] After wide-search lands: its `search_batch` pins through `IndexPin`, K clamped to `soft_open`.

## Proof (step 3, head `Xapiand:7d614930f`)

macOS, Apple clang RelWithDebInfo `ASSERTS=ON`, LTO off (`.scratch/build-x3`):
- `xapiand_test_pool` 264 checks, `xapiand_test_node` 108 (after dropping the manager C24 test, 112 with it), all pass.
- `graceful_shutdown_check.sh` all scenarios pass; `path_boundary_check.sh` 0 failures; `wide_search_check.py check`
  PASS (nested and glob paths); docs E2E (`npx newman@6.2.3`) 396 requests, 666 of 666, 0 uncovered, asserts on.
- `async_check --strict --owed --extern strata=<item 5b>`: 250 hits, owed 0, unclassified 0; `test_gate_tools.py` OK.
- Mutants (`.scratch/mutants3/mutants.py`, one string each, rebuilt, filtered test run, restored):
  | Mutant | Killed by |
  |---|---|
  | s1 `settle_merges` never pending | `test_the_autocommit_comes_back_while_a_merge_is_pending` (first call false) |
  | s2 no sync of a merge's output in the settle | the same (never settles; output unsynced) |
  | s3 busy not pending | `test_a_busy_autocommit_comes_back` (pool), `test_a_busy_autocommit_comes_back_for_its_write` (node) |
  | s4 the committer does not re-arm | `test_a_busy_autocommit_comes_back_for_its_write` |
  | s6 `IndexSweep` ignored | `test_the_sweep_closes_only_an_unpinned_index` (the idle index stays open) |
  | s7 a use drops its pin | the same (the held index closes): XP3's "unpin early" |
  | s8 the default commit chore (`index.commit()`) | `test_the_shutdown_commits_and_logs_an_uncommitted_write` (0 COMMIT lines): XP4's mutant |
  | s9 the commit chore without the merge-only sync | `test_the_sweeps_close_syncs_a_merges_output` (never closes) |
  | s5 no check-in in the settle | survives: equivalent, the turn's release checks in, one more round settles |
  | s10 C24 reorder reversed | killed on macOS by the manager-level test, which then failed unmutated on the Linux VM (see Findings); dropped |
- Revert checks: each new test fails against its mutant, which is the change reverted (s1, s3, s4, s6, s8, s9).
- The merge window is forced, not timed: `GatedExecutor` holds the index's merges at their first checkpoint.
- No XP3 code path left: `begin_idle_close`, `finish_idle_close`, `housekeeping_pass`, `renew_time` have no hit in
  `src/`; the manager's minute housekeeping is the schema cache's cleanup only.
- Sanitizers, macOS (Homebrew LLVM 23, separate builds `.scratch/build-asan`, `.scratch/build-tsan`, runner
  `.scratch/san-tests.sh`): ASan+UBSan `test_pool` 264, `test_reactor_requests` 59, `test_engine_host` 95,
  `test_node` 108, `test_wide_search` 22301, `test_blocking_guard` 14, all pass, 0 reports.
  Graceful all pass, path 0 failures, wide search PASS, docs E2E 666 of 666 with 0 reports in the node log. TSan
  (`die_after_fork=0`; `report_thread_leaks=0` for `test_node`, as in step 2): the same six suites and four harness
  checks pass, 0 reports.
- Linux, the local QEMU VM (clang-18, `~/xpool3/{tsan,asan}`, `.scratch/linux3/ship-run.sh` through `aws-linux.sh ssh`):
  - ASan+UBSan: the six suites pass (pool 264, node 108), 0 reports; graceful all pass; path 0 failures; docs E2E 666
    of 666, 0 reports.
  - TSan: pool 264, reactor_requests 59, engine_host 95, wide_search 22301, blocking_guard 14 pass, 0 reports; node
    passes all but the manager C24 test, since dropped; graceful all pass; path 0 failures; docs E2E 666 of 666, 0 reports.

## Proof (step 2, head `Xapiand:1089c251b`)

macOS, Apple clang RelWithDebInfo `ASSERTS=ON` (`.scratch/build-x2`):
- ctest 36 of 36 (`test_text` excluded, as on master): `xapiand_test_pool` 229 checks (plus the two new ones below),
  `xapiand_test_reactor_requests` 59, `xapiand_test_node` 104 plus `test_a_search_batch_skips_a_missing_index`.
- Docs E2E (`npx newman@6.2.3`, `e2e_capture.sh` + `e2e_assert.py`): GREEN, 396 requests, 666 of 666 assertions,
  0 uncovered, on an asserts build (the pin-before-view check live). No assertion or `owner_break` in the node log.
- `graceful_shutdown_check.sh`: all scenarios pass. `path_boundary_check.sh`: 0 failures.
  `wide_search_check.py check`: PASS (batched equals unbatched, by list and pattern; nesting `/**/x/`, globs).
- `async_check --strict --owed --extern strata=<item 5b>`: 250 hits, owed 0, unclassified 0; `test_gate_tools.py` OK.
- Sanitizers on macOS, Homebrew LLVM 23 (`-DCMAKE_OSX_SYSROOT=.../MacOSX.sdk`, see Findings), RelWithDebInfo
  `ASSERTS=ON`, separate builds `.scratch/build-asan` (ASan+UBSan, UBSan recoverable with rapidjson's and LZ4's
  pointer-overflow suppressed, `.scratch/ubsan.supp`) and `.scratch/build-tsan`; runner `.scratch/san-tests.sh`:
  - ASan+UBSan: `test_pool` 231, `test_reactor_requests` 59, `test_engine_host` 95, `test_node` 106,
    `test_wide_search` 22301, `test_blocking_guard` 14 checks, all pass, 0 reports; docs E2E GREEN (666 of 666) with
    0 reports in the node log; `graceful_shutdown_check.sh` all pass.
  - TSan: the same suites pass with 0 race reports, with `die_after_fork=0` (the at-end tests fork a threaded
    process, which TSan otherwise kills) and, for `test_node`, `report_thread_leaks=0`: the abandon children `_Exit`
    with the node's thread unjoined (`test_node.cc`, the same on master), a leak report and not a race. Docs E2E GREEN
    with 0 reports; `graceful_shutdown_check.sh` all pass.
- Linux, the local QEMU VM (8 cores, Ubuntu 24.04, clang-18; AWS off), driven by `.scratch/linux2/ship-run.sh` with
  the tree in `~/xpool2/src` and builds in `~/xpool2/{tsan,asan}` (task-named, not the shared `~/work`); script
  `.scratch/linux2/build.sh`, logs `.scratch/linux2/out-{tsan,asan}/`:
  - TSan (`die_after_fork=0`): `test_pool` 231, `test_reactor_requests` 59, `test_engine_host` 95, `test_node` 106,
    `test_wide_search` 22301, `test_blocking_guard` 14 checks, all pass, 0 reports; `graceful_shutdown_check.sh` all
    pass; `path_boundary_check.sh` 0 failures; docs E2E 666 of 666, 0 uncovered, 0 reports in the node log.
  - ASan+UBSan (recoverable, the two third-party suppressions): the same, all pass, 0 reports.
  Step 1's Linux failures do not recur here: `BlockingScheduler`'s race (fixed on master since), the at-end test's 5
  failures and the intermittent "IndexChores ended with work outstanding" (the idle close and housekeeping are gone),
  and `path_boundary_check.sh`'s 14 relative-path failures.
- Mutants (`.scratch/mutants2/mutants.py`, each a one-string change, rebuilt, its test run, the source restored):
  | Mutant | Killed by |
  |---|---|
  | m1 no WAL replay at open | `test_a_cold_pin_reads_what_the_log_replays`, `test_a_fenced_index_is_replaced_and_replays_its_log` (doc_not_found) |
  | m2 a stale entry's use keeps its pin | `test_a_fenced_index_is_replaced_and_replays_its_log` (still fenced) |
  | m3 a failed commit does not mark the entry | the same |
  | m4 no N3 check in `pin` once finished | `test_a_finishing_pool_serves_an_open_index` |
  | m5 `pin_chore` asks `open`, not `resident` | `test_a_chore_pin_is_resident` |
  | m6 no create intent from the flags | `test_a_create_pin_passes_a_missing_index` |
  | m7 `~DatabasePool` does not wait for the shutdown | `test_the_pool_waits_for_a_pin_released_during_its_end` (abort: catalog destroyed with a pin held) |
  | m9 no view check in `Shard::reopen` | `test_reactor_requests` view rule, "an open" |
  | m10 the turn's release does not check in | `test_the_turns_end_publishes_a_finished_merge` |
  | m12 `lock_shard::pin` raises a missing index's not_found | `test_a_search_batch_skips_a_missing_index` |
  | m8 `do_commit` without its "revision unchanged" discard; m11 `commit_blocking` without its nothing-applied branch | survive: equivalent. `do_commit` returns before the engine when nothing is modified, and strata bumps a revision only when something was applied (`index.hh:4631`), so C25 holds at three layers; `test_the_commit_chore_logs_nothing_when_nothing_was_applied` pins the result (one COMMIT line, no revision) |
- Revert checks: each new test above fails against its mutant, which is the change reverted. FLUSH's `when_all` (before
  the one-at-a-time loop) aborted `test_flush_commits_every_index` with "a task holds one index's turn at most".
- C24 at the manager: the control loop's suspended roots are destroyed before stop 3 awaits the catalog's shutdown. No
  test kills a reversal of that move: such a root outlives the drain only when the drain gave up at the deadline, and
  then the stop exits 75 either way (see Findings).

## Proof (step 1, head `Xapiand:158a3b14a`)

macOS (Apple clang, RelWithDebInfo, `ASSERTS=ON`, `.scratch/build-mac`):
- `xapiand_test_pool` 231 of 231 checks; `xapiand_test_reactor_requests` 342 of 342.
- Mutants on `src/database/pool.cc` (`.scratch/mutants/run.sh`), each killed, the source restored:
  - M1, no `check_no_view_held` in `pin`: `test_reactor_requests.cc:434` (the view rule's "a pin") fails.
  - M2, `pin` uses `get`: `test_a_pin_keeps_its_index_open` fails (empty pin; then a crash, rc 139).
  - M3, `pin_chore` uses `spawn`: `test_a_chore_pin_opens_nothing` fails (an entry is made).
  - M4, `pin_chore` not resident: the same test fails twice (a made and a closed index give a pin).
- The one ordering choice, pins before views, is enforced by the check (M1). No new shared state: a pin is
  today's `refs` count, so no thread mutant applies to step 1.
- `async_check --strict --owed --extern strata=<strata at the pin 3f564dc>`: 245 hits, owed 0, unclassified
  0; `test_gate_tools.py` OK. `lock_shard::pin()` is named so the checker does not read it as a mutex; the
  stale `lk_shard\.lock` row is gone.

Linux (AWS lease, 16 cores, `~/xpool`; scripts `.scratch/linux-batch*.sh`). No result differs from master
(`88892b4cb`) built the same way:
- clang-18 TSan, asserts on: docs E2E GREEN, `graceful_shutdown_check.sh` all scenarios pass. ctest 30 of
  34: `test_engine_host`, `test_pool`, `test_node` race in `BlockingScheduler::~BlockingScheduler`
  (`host.h:214`), which `phase7c/linux-gate-fixes` fixes in Xapiand:8b4541e63 (not on master); with that
  commit applied, `test_engine_host` (99), `test_node` (85) and `test_reactor_requests` (342) pass, and
  `test_pool` stops at a race on an exception object two `Running` threads share in
  `test_a_failed_commit_reaches_only_the_joined_waiters` (test code this branch does not touch). Master
  with the fix aborts `test_pool` earlier, in `test_smoke` ("IndexChores ended with work outstanding").
  `test_text` runs out of TSan memory.
- clang-18 ASan+UBSan, UBSan non-recoverable: every JSON response aborts in rapidjson (`stack.h:117`, null
  plus offset) and LZ4 (`lz4.c:1069`); six suites abort identically on master. Not a usable gate.
- clang-18 ASan+UBSan, UBSan recoverable with those two third-party reports suppressed, asserts on: docs
  E2E GREEN (the view check live, so every request path pins before its views), `graceful_shutdown_check.sh`
  all pass. `test_reactor_requests` 342 of 342 five of five (master 341). `test_pool`: 5 of 231 checks fail
  in `test_a_registry_that_ends_with_work_outstanding_aborts` (`test_pool.cc:1622`), five of five, as on
  master (5 of 214); one run in five aborts with "IndexChores ended with work outstanding" (master 0 of 5
  here, 1 of 1 under TSan above). `test_text` ASan out of memory. `path_boundary_check.sh`: the same 14
  failures on master and branch (relative `./path_boundary.*/out` index paths accepted).
- gcc-13 with asserts and tests: an internal compiler error on `src/database/schemas_lru.cc:176` and
  `fuzz_body.cc` not compiling; the CI's gcc config builds neither asserts nor tests.

## Design vs code (step 3, branch `phase7c/x-pool-step3`)

Decisions recorded from the step 2 review (CR5, design D13): the sweep's times are strata's (1 s, 30 s, 600 s); every
open replays the WAL (the restore's skip is gone); `FLUSH` commits and no longer closes; a 404 leaves a negative entry
per name until the sweep erases it. None narrows a user-visible answer; `FLUSH` not closing is the closest call (the
reference and changelog describe it as a commit only).

Owed (CR6): the hostless path (`engine::open_local`, `Core::owned`, tests and the CLI) keeps the adapter's own
registry, against D2's one way to hold an index. Not built in step 3.

11. XP4's "committed by the sweep's close": the sweep closes only a committed index (a hot one demotes first, which
    needs `warm_quiescent`; a cold one needs `close_ready`), so no sweep close commits a write. Its close commits only a
    finished merge's output, through the entry's chore (S14); a write left uncommitted is committed by the shutdown, the
    replace close, or the bound close of an owed slot whose wakes failed. The tests cover the shutdown's commit of a write
    and the sweep's sync of a merge.
12. XP4's "blobs synced" has no test of its own: the entry's chore runs `do_commit`, whose `storage_commit` precedes the
    engine's commit, and no test observes the fsync.
13. D8's re-arm also covers a busy autocommit: one whose turn did not come (`busy`) arms itself again, so its write is
    committed later; before, it was logged and left to the next write or the close.
14. `IndexSweep` (the sweep's period and idle times; zero keeps strata's) is new; a node passes none. D8 names
    no times.
15. `settle_merges` checks the index in inside its chore, though the turn's release checks in too: one call then
    publishes and syncs, where the release's check-in alone takes two (mutant s5 survives, equivalent but slower).

## Design vs code (step 2; note `docs/mechanisms/index-registry-on-catalog.md`)

1. D2's hostless private catalog is not built: `engine::open_local` (tests, the CLI) keeps the adapter's owned-index
   registry and its in-place reopen. The borrowed Core raises on reopen and close.
2. D2's `corrupt_segment` runs `heal()` under the turn: not built; a corrupt segment marks the entry stale and the
   catalog replaces the index like a fence.
3. D6's restore: the open replays a WAL that exists even when the opener's flags disable the WAL (the old open
   skipped it); the restore still deletes the log of what it replaced at its end.
4. N3 (not in the design): a finishing pool serves only open or owed indexes, through a resident pin taken before the
   request pin; the resident pin applies nothing, so it meets no debt (C25's third clause).
5. D8's FLUSH commits one key at a time (a task holds one turn), not all at once.
6. A failed write or commit marks the entry stale (`note_failure`), and every use hands its pin back at its end; D2
   says the call that throws hands it back. Same effect, one place.
7. `lock_shard::pin` defers a missing index's `not_found` to the use (the Shard stays unpinned), so a search batch
   skips it as before; the design names no such case.
8. The shutdown's abandon report names `pinned` where the old one named the held turn (`test_node` expects
   `pinned`): the catalog sees pins, and a turn is held only under one.
9. D8's "a key closed in between closed committed": an idle close takes only a committed index (`warm_quiescent`),
   so the sweep never commits; the bound close, the replace close and the shutdown do, through the entry.
10. `DatabasePool::set_index_io` (tests only) puts the index files on another IO; strata's faultfs fences in tests.

## Findings for the lead

- (step 3) The C24 reorder cannot show at the manager: a root that holds a pin past the drains leaves the HTTP stop 1 ms,
   and that stop counts the pinned index as unfinished work, abandons, and so skips destroying the control roots in
   either order (Linux VM, TSan: "Shutdown abandoned 1 unfinished item ... 'root' (open, 1 pin)"). CR3's pool-level test
   is the one to keep.
- (step 3) Before the re-arm, an autocommit whose turn timed out (`busy`) logged and left its write uncommitted until
   another write or the close; it now comes back.

- (step 2) Strata's `resource_budget.hh` includes `<sys/sysctl.h>` outside Linux, which defines `MIN` and `MAX`;
   every host that includes the catalog inherits them (Xapiand's `DatabaseWALType::MAX` broke). `pool.h` undefines
   them; the generic fix is strata's (keep the include in a source file or undefine after it).
- (step 2) Homebrew LLVM on macOS needs `-DCMAKE_OSX_SYSROOT=/Library/Developer/CommandLineTools/SDKs/MacOSX.sdk`
   for Xapiand: otherwise a dependency's `-isystem <SDK>/usr/include` is not recognised as implicit (CMake sees
   `MacOSX15.sdk`) and comes before libc++, which breaks `std::isfinite` in msgpack's rapidjson glue and CLI11.
- (step 2) C24 at the manager: a control root that holds a pin past the drains exists only when a drain hit the
   deadline, and the stop then has no time left for the catalog's shutdown, so it exits 75 whatever the order. The
   order is now right (roots before the shutdown); a clean exit in that case needs the drains to leave the
   catalog part of the bound.

1. clang 18 and later UBSan abort Xapiand on every JSON response (rapidjson `stack.h:117`, null plus
   offset; also LZ4 `lz4.c:1069`); clang 20 does not compile rapidjson (`document.h:319`). gcc-13 with
   `ASSERTS=ON` hits an internal compiler error on `schemas_lru.cc:176`. All pre-existing.
2. On Linux, master fails `test_pool`'s `test_a_registry_that_ends_with_work_outstanding_aborts` under
   ASan, `path_boundary_check.sh` 14 times (relative paths), and aborts `test_pool` intermittently with
   "IndexChores ended with work outstanding"; the TSan races in `BlockingScheduler` wait on
   `phase7c/linux-gate-fixes`.

## Done

- Step 3: `Xapiand:00a29c2ce` the autocommit's settle and re-arm, `IndexSweep`, tests for XP3, XP4 and the re-arm;
  `Xapiand:72528f7fb` CR4, cancellation through `pin_chore`; `Xapiand:2d5ff8ef9` the notes, D13 (CR5), CR6 owed;
  `Xapiand:7d614930f` the manager C24 test dropped; this ledger.

- Step 2: `Xapiand:d43d00046` the registry as the catalog's payload (the stopped attempt's work, kept: sound on review);
  `Xapiand:ae6490771` build and test fixes, N3, FLUSH one key at a time, failed writes hand pins back, control roots
  before the shutdown (C24), tests for C24 and C25; `Xapiand:b4d86cb9c` the faultfs tests (XP2, C23);
  `Xapiand:45725be28` a search batch skips a missing index, the turn's check-in test; `Xapiand:1089c251b` the
  async_check allowlist.

- `Xapiand:6108858e7` design note revision 1; `Xapiand:a037c083e` the Linux host rule.
- `Xapiand:0d4c7e629` the seam: `IndexPin`, `DatabasePool::pin`/`pin_chore`, `lock_shard` awaited, the
  view check, the autocommit by endpoint; tests `test_a_pin_keeps_its_index_open`,
  `test_a_chore_pin_opens_nothing`, and the view rule's "a pin" cases.
- `Xapiand:24c0e2f10` `lock_shard::pin()` and the allowlist row.
- `Xapiand:158a3b14a` the resident chore pin; design revision 2 with the rulings; mechanism notes.

## Next

1. When CR1 lands on strata's item 5b: `DatabasePool::pin` and `pin_chore` pass the request's cancel token; CR3's
   property test (a destroyed root that held a pin lets `join()` return true); CR4's test (a cancelled resident pin
   throws `task::cancelled`, and FLUSH ends).
2. Step 3 to review with step 2's fixes. Step 4: `map_owed`, the gate assertion, the budget (D13's numbers).
3. Repin strata to main when items 5 and 5b land.

## Blocked on

- CR1 (strata, the destroy-safe pin awaiter and the pin's cancel token): CR3's test, CR4's test and passing the token.
