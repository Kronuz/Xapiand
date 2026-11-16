# xapiand-strata-bump review: correctness

Commit: Xapiand:956f4b6ea104bf7e75e67e000c67b9753ceec514 (with prospect:61e09a60e0bf, strata:86fe7c476c64)
Verdict: APPROVE
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261112-101724-de5872 (build-test at Xapiand:c3fec9b40, exit 0): 39/39 ctest, async_check clean,
  removed_options_check. c3fec9b40..956f4b6ea changes only the journal, so it stands for 956f4b6ea.
- 261112-101724-6a3348 (A3 test at c3fec9b40, exit 0): 144 checks pass.
- 261112-105157-135ef5 (mutant at 956f4b6ea, exit 0): with `corrupt_segment` mapped to
  `errc::database` the test fails at test_engine_error.cc:240 and :244, its own kind checks. So the
  damage reaches Xapiand as `corrupt_segment`, not as `corrupt_index` or a system error.
- 261112-105156-3f234f (pins_check at 956f4b6ea, exit 0): strata 86fe7c47 and prospect 61e09a6
  have no other pin to lose to; completion resolves to e49f288 (strata's pin), with journal's
  9acfba3 listed as a stale pin.
- Xapiand CI 43278343734 at 956f4b6ea (gh): ASan+UBSan, TSan, unit under ASan+UBSan and under TSan,
  and e2e all pass. "Build + unit + smoke" fails only in test_loop_yield.cc:383
  (`loop_ms < process_ms * 0.45`), the known flake; test_engine_error passes there.

## What I checked myself

Scripts are in `.scratch/review-xapiand-strata-bump-correctness/` in the home checkout.

- Q1 pins: prospect:61e09a6's CMakeLists.txt:11 pins strata 86fe7c476c64, the same commit as
  Xapiand CMakeLists.txt:770. prospect:61e09a6 is a fast-forward of prospect origin/main a938de6.
  b20e7c5..61e09a6 changes no prospect library header, only tests, tools and docs.
- Q1 drift: pins_check.py:132-139 fails when any resolved library pins a newer commit than Xapiand,
  or a commit that diverges from it. That is the drift this quest repairs. A library pinning an
  older commit than Xapiand passes as a "stale pin" unless `--strict` is given. Nothing runs it
  automatically (F2).
- Q2 IO: Xapiand has no `kronuz::journal::IO` subclass in src/ or tests/. Its IOs are
  `PosixIO{true, true}` (pool.cc:571, shard.cc:451, test_reactor_requests.cc:495), and
  `PosixIOOptions`' first two fields are the same at completion e2c6474 and e49f288. Positional
  init still sets follow_root_symlink and allow_shared_directory; barrier_warning stays null and
  full_flush_ordering stays false. No barrier warning is emitted. That matches D16: the
  `barrier-warning` quest owns the warning.
- Q2 laziness: strata index.hh:603 and :5105 at 86fe7c47 force `eager_segments` under `defer_io`.
  Every production open passes `host = true`, which sets defer_io (pool.cc:577,
  database/shard.cc:466, shard.cc:171). `engine::open_local` and `engine::check` have no
  production caller outside engine/open.cc. The hostless path is lazy, but `verify_fields()`
  (shard.cc:336-341) maps every segment at open, so it surfaces damage at open too.
- Q2 error mapping: the only new strata exception type in the range is `generation_not_retained`.
  It is raised only by `open(..., to)`, which Xapiand never calls. The `corrupt_segment` and
  `corrupt_index` translations (shard.cc:114-117, pool.cc:538-541) were already in place.
- Q2 unpin-after-kick: the last unpin's shutdown close is posted to the catalog's executor
  (catalog.hh:2401-2405 at 86fe7c47), not run on the unpinner's thread. Xapiand's pins are not
  held across that close. CI's TSan jobs at this commit are the evidence for the catalog race the
  journal attributed to strata.
- Q3 probe job 261112-113044-cc7874 (`probe_old_manifest.sh probe_old_manifest.cc`): it opens,
  through the pool as a node does, an index directory holding a pre-manifest-log manifest
  (`STRMAN01`, the format strata 5d742b2 wrote). Read and write pins both answer HTTP 500 with
  "strata: unsupported index: a manifest file of another build". The exception is not an
  `engine::Error`, and the write pin creates nothing (only `manifest-1` remains). See F1. Note:
  the first run of the script left tests/test_engine_error.cc modified in the quest's job slot
  because its restore trap ran from build/. I restored the file (`git -C <slot> checkout`) and
  fixed the trap.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | User-visible and not named in the quest file: the bump moves Xapiand from strata 5d742b2 (pre-manifest-log) to the manifest log (strata:f2089f9f), so every index written by a current master build fails to open. Reads and writes answer HTTP 500 "strata: unsupported index: a manifest file of another build" as a raw `std::runtime_error`; `rethrow_pin_failure` (pool.cc:518-542) and `rethrow_translated` (shard.cc:96-127) have no case for `strata::unsupported_index`, so it never becomes an `engine::Error`. No data is overwritten. The owner decides; formats stay open until release. | job 261112-113044-cc7874; strata manifest_log.hh:1184 | Owner's call. Options: 1. Keep it as is and name the break in the quest or upgrade notes. 2. Also map `unsupported_index` to a named errc and status, so the client sees "unsupported index" rather than an internal error. 3. Add a migration path (unlikely to be worth it before release). |
| F2 | no | gap | A1 reads "pins_check in the build-test check", but the build-test check (state.json) and CI never run harness/pins_check.py; only the one-off job 261112-105156-3f234f did. Drift like the one this quest fixes (prospect pinning a newer strata) is caught only when someone runs it. This predates the quest. | state.json build-test command; no pins_check in .github/workflows | Add `python3 harness/pins_check.py --deps .deps` to the build-test check or a CI step (lead's call whether here or in a follow-up). |
| F3 | no | nit | Restated A3 says "the error surfaces at open", but the test's request (test_engine_error.cc:229-237) pins and reads in one lambda, so it does not show which step fails. It still holds if nodes become lazy (hunt row 102), which may be the better property. | test_engine_error.cc:229-237 | Optional: assert the throw comes from `pin_index`, or reword A3. |
| F4 | no | nit | Xapiand pins prospect 61e09a6, a branch commit. Xapiand must not land before prospect main fast-forwards to it, or the pin names a commit off main. | prospect origin/main is a938de6 | Lead's sequencing (already stated). |
| F5 | no | nit | The journal's Result section still names the earlier pins (strata ab0e26a6, prospect fcd9f3e). The CMakeLists comment on the prospect pin says "damaged segments at first map", but Xapiand nodes stay eager. | journal Result; CMakeLists.txt:771 | Refresh the text. |

## Not run

- No local sanitizer runs: CI's ASan+UBSan and TSan jobs at 956f4b6ea are the evidence.
- No review of strata's own changes in 5d742b2..86fe7c47 (manifest log, catalog, open-sync),
  which strata's quests reviewed. I covered only Xapiand's call sites against them.
- Per-open cost of the new open (directory and log sync on every open, read-only too): this is
  hunt rows 66 and 102.
- The master merge the brief asks for (10 files since the branch point); the landed commit should
  be rechecked by the suite after it.
- macOS barrier behaviour on non-Apple storage: unchanged, and deferred to the `barrier-warning`
  quest.
