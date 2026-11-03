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

Step 1 (the seam) built and proved on macOS; Linux sanitizer proof in progress (see Proof). Step 2
waits for item 5b.

## Done criteria

- [x] `docs/design/x-pool.md`: decisions, gate rows, the rulings on F1 to F6 (revision 2, L2, XH8).
- [x] Step 1, the seam: `IndexPin` over today's `refs`, `lock_shard` awaited (`co_await lk_shard.pin()`)
  at all 28 construction sites plus `dump_page`'s borrowed one, the pin-before-view check (XP1), the
  autocommit's resident pin.
- [ ] Step 1 proof on Linux: see Proof.
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

Linux (AWS lease, 16 cores):
- clang-18 TSan, asserts on (`~/xpool/tsan`): ctest 30 of 34; the four are `test_engine_host`, `test_pool`,
  `test_node` (each a data race in `BlockingScheduler::~BlockingScheduler`, `host.h:214`, the race
  `linux-gate-fixes` fixes in Xapiand:8b4541e63, not on master) and `test_text` (TSan out of memory). Docs E2E
  GREEN, `graceful_shutdown_check.sh` all scenarios pass, `path_boundary_check.sh` all ok.
- clang-18 ASan+UBSan (`-fno-sanitize-recover=undefined`), asserts on: unusable as a gate. rapidjson's
  `Stack::Reserve` (`internal/stack.h:117`) applies an offset to a null pointer on the first JSON response
  (`MsgPack::to_string` → `Writer::StartObject`), which aborts the node on `GET /` and six tests; third-party
  code this branch does not touch. Docs E2E reported GREEN.
- Attribution batch (`.scratch/linux-batch2.sh`, branch against master `88892b4cb` on the same ASan config,
  and the branch plus 8b4541e63 on TSan): running.
- gcc-13 ASan+UBSan, the CI's config (`-DASAN=ON -DUBSAN=ON`) with asserts and tests
  (`.scratch/linux-batch3.sh`): running.

## Findings for the lead

1. clang 18 and later UBSan abort Xapiand on every JSON response (rapidjson `stack.h:117`, null plus
   offset); clang 20 does not compile rapidjson at all (`document.h:319`, assignment to a const member).
   The CI's gcc-13 config does not flag the first. Pre-existing, outside this branch.
2. The TSan races in `BlockingScheduler`'s destructor fail three suites on master's code; the fix waits on
   `phase7c/linux-gate-fixes`.

## Done

- `Xapiand:6108858e7` design note revision 1; `Xapiand:a037c083e` the Linux host rule.
- `Xapiand:0d4c7e629` the seam: `IndexPin`, `DatabasePool::pin`/`pin_chore`, `lock_shard` awaited, the
  view check, the autocommit by endpoint; tests `test_a_pin_keeps_its_index_open`,
  `test_a_chore_pin_opens_nothing`, and the view rule's "a pin" cases.
- `Xapiand:24c0e2f10` `lock_shard::pin()` and the allowlist row.
- `Xapiand:158a3b14a` the resident chore pin; design revision 2 with the rulings; mechanism notes.

## Next

1. Close step 1's Linux proof (batches 2 and 3), then hand step 1 to review.
2. Step 2 against item 5b once its branch has the host interface: `IndexEntry` by `open_payload`,
   `StrataReaders` and the adapter's registry gone, create intent from the flags, `index_fenced` to
   `reopen`.

## Blocked on

- Step 2: strata item 5b (the host interface: intents, `open_payload`, `reopen`, `snapshot`).
