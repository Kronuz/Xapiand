> Status: CURRENT (2026-11-02) the X-POOL developer's ledger (phase 7c: Xapiand adopts strata's cold-tier catalog); local branch `phase7c/x-pool` from master `Xapiand:88892b4cb`, not pushed.

# phase7c X-POOL (build): the registry becomes the catalog's payload

Branch `phase7c/x-pool` in `Xapiand-wt/x-pool`. Spec: `strata/docs/design/cold-tier.md` revision 7
(`design/cold-tier`, strata:5723f3d), "Steps", "Xapiand adopts it". Design: `docs/design/x-pool.md`
revision 2, with the architect's host-interface rulings (`strata/docs/reviews/cold-tier-xpool-host.md`).
Step 2 builds against strata item 5b (`phase/cold-tier-step2-item5b`) with
`-DFETCHCONTENT_SOURCE_DIR_STRATA=<a strata worktree>`. Step 1 uses master's strata pin.

Host rule (the lead, 2026-11-02): sanitizer batches run on Linux through strata's `tools/vm/aws-linux.sh`
(the branch shipped as a git bundle into `~/xpool` on the lease); only macOS builds run locally.

## State

Step 1 (the seam) built and proved: macOS tests and mutants, Linux TSan and ASan+UBSan with no result
different from master (see Proof). Ready for review. Step 2 waits for item 5b.

## Done criteria

- [x] `docs/design/x-pool.md`: decisions, gate rows, the rulings on F1 to F6 (revision 2, L2, XH8).
- [x] Step 1, the seam: `IndexPin` over today's `refs`, `lock_shard` awaited (`co_await lk_shard.pin()`)
  at all 28 construction sites plus `dump_page`'s borrowed one, the pin-before-view check (XP1), the
  autocommit's resident pin.
- [x] Step 1 proof on Linux: see Proof.
- [ ] Step 2, the registry as `H` (against item 5b): XP1, XP2.
- [ ] Step 3, commit chore, idle close and housekeeping gone, shutdown, FLUSH by snapshot: XP3, XP4.
- [ ] Step 4, `map_owed`, the `unleased_maps()` assertion, the budget and CT-R39's clamp: XP5.
- [ ] After wide-search lands: its `search_batch` pins through `IndexPin`, K clamped to `soft_open`.

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

## Findings for the lead

1. clang 18 and later UBSan abort Xapiand on every JSON response (rapidjson `stack.h:117`, null plus
   offset; also LZ4 `lz4.c:1069`); clang 20 does not compile rapidjson (`document.h:319`). gcc-13 with
   `ASSERTS=ON` hits an internal compiler error on `schemas_lru.cc:176`. All pre-existing.
2. On Linux, master fails `test_pool`'s `test_a_registry_that_ends_with_work_outstanding_aborts` under
   ASan, `path_boundary_check.sh` 14 times (relative paths), and aborts `test_pool` intermittently with
   "IndexChores ended with work outstanding"; the TSan races in `BlockingScheduler` wait on
   `phase7c/linux-gate-fixes`.

## Done

- `Xapiand:6108858e7` design note revision 1; `Xapiand:a037c083e` the Linux host rule.
- `Xapiand:0d4c7e629` the seam: `IndexPin`, `DatabasePool::pin`/`pin_chore`, `lock_shard` awaited, the
  view check, the autocommit by endpoint; tests `test_a_pin_keeps_its_index_open`,
  `test_a_chore_pin_opens_nothing`, and the view rule's "a pin" cases.
- `Xapiand:24c0e2f10` `lock_shard::pin()` and the allowlist row.
- `Xapiand:158a3b14a` the resident chore pin; design revision 2 with the rulings; mechanism notes.

## Next

1. Step 1 to review (the lead's triage).
2. Step 2 against item 5b once its branch has the host interface: `IndexEntry` by `open_payload`,
   `StrataReaders` and the adapter's registry gone, create intent from the flags, `index_fenced` to
   `reopen`.

## Blocked on

- Step 2: strata item 5b (the host interface: intents, `open_payload`, `reopen`, `snapshot`).
