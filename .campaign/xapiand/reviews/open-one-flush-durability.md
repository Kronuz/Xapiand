# open-one-flush review: durability

Commit: strata:ab0e26a60a7aded5e1aedf6bbf3323ce14c7d8ad (code as at c35ce7e0de996348e57970339b3f20e7015a5e29)
Verdict: APPROVE
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The branch head f8747be0 adds only docs and `bench/open_sync/` over c35ce7e0; the code reviewed is
c35ce7e0's one hunk in `include/strata/core/index.hh:5260-5267`.

## Evidence I relied on

- 261112-022003-37891b: `index_test` passes at c35ce7e0 (T4 cases 9 and 10).
- 261112-022007-45e212: strata suite passes at c35ce7e0, Release, on this Mac (real PosixIO opens).
- 261112-022005-2de6e1: mutant strata:65fd2afd (sync only on `rollover_owed_`) fails case 9 at
  `index_test.cc:137`. Its log ends after 31 failures (`kv_test.hh:23` exits past 30), so it never
  reached case 10; my probe covers that.
- CI 43199523054 at c35ce7e0: Linux (gcc, clang, arm64, ASan+UBSan, TSan), FreeBSD and
  macos-x86_64 pass; macos-arm64 cancelled (no runner). macos-x86_64 runs the `__APPLE__`
  `sync_staged` path functionally.

## What I checked myself

1. Completion at strata's pin (completion:e49f288, `include/completion/posix.h`), per platform:
   - macOS: `sync_files_and_directory` is `sync_staged(durable)`: `fsync(log)`, `fsync(dir)`, then
     one `F_FULLFSYNC` per distinct device (`files_sync_devices`, posix.h:202-243). The log and its
     directory share a device, and `stable_sort` keeps files first, so the flush goes through the
     log's descriptor. Before: `fsync(dir)` plus `F_FULLFSYNC` through owner.lock, then
     `F_FULLFSYNC(log)`. The old directory sync already relied on a device flush through another
     descriptor, so the new path rests on the same assumption: both are staged before the one
     device-wide flush.
   - Linux: `sync_files` (a `sync_file_range` hint, then `fsync(log)`, stronger than the old
     `fdatasync`) followed by `sync_directory` (`fsync(dir)`).
   - FreeBSD/elsewhere: `fsync(log)` then `fsync(dir)`, the same calls as before (`file_sync_data`
     is `fsync` off Linux), in the other order.
   - Order: the open writes nothing before the sync (the scan and replay only read; orphans are
     removed after it), so a crash part way through can only expose a state the earlier crash
     already could. Nothing is reported until both are durable. The swap from directory-first to
     log-first adds no state.
   - Production wrapper: `counted_io::sync_files_and_directory` unwraps files to the inner PosixIO
     (`counted_io.hh:207-210`), so the `dynamic_cast<PosixFile*>` holds and the shared flush is
     real. A file that failed the cast would fall back to `File::sync`, which is still durable.
2. `unlinked_ = 0`: the IO contract (`io.h:237-243`) makes "creations, replacements and removals
   so far" durable, as `sync_directory` does, so zeroing is as correct as `sync_dir()` was. In
   `recover` nothing calls `remove_file`/`forget_file` before the sync (`index.hh:5189-5267`),
   and the `.tmp` orphan removals come after it, so their bytes stay counted. If the sync throws,
   the open fails and the object is discarded.
3. faultfs (strata main): `faultfs::io` does not override `sync_files_and_directory`, so it runs
   IO's default, `file->sync()` then `sync_directory()`. That is the contract's own definition, and
   both steps are fault-injectable crash points. T4 therefore proves the strata-level property
   against the contract. It does not exercise completion's macOS single-flush implementation,
   which point 1 covers by reading.
4. Probe 261112-025427-ce1bb2 (light, fresh checkout at c35ce7e0; script
   `.scratch/review-open-one-flush/mutants.sh` in the home checkout) builds two mutants of the one
   sync line. Both are caught, so each half of the fold has its own failing test:
   - M1, log only (`log_->sync()`, no directory sync): fails case 10 at `index_test.cc:182` in all
     four kind×post runs, with case 9 passing.
   - M2, directory only (`io_->sync_directory()`): fails case 9 at `index_test.cc:137` (31
     failures).

   Case 10 needs no mutant of its own for A1 (this is not a fix), but M1 shows it guards the
   directory half.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| N1 | no | nit | The closing sentence of `docs/perf/open-sync.md` at f8747be0 says Linux issues "the same two `fsync`s as before". That contradicts the doc's own earlier paragraph and journal ruling 3: before, the log got `fdatasync`. | `docs/perf/open-sync.md`, last line | Say the log's `fdatasync` became `fsync`, as the "One device flush" section does. |
| N2 | no | gap | The rollover-mutant job evidences case 9 only, and 2de6e1 stops before case 10. A1's "both crash properties" rests on probe ce1bb2 (M1 and M2) for the directory half. | jobs 261112-022005-2de6e1, 261112-025427-ce1bb2 | Cite ce1bb2 in open-sync.md's evidence table, or tag a rerun of it `--for A1 --fails-without`. |
| N3 | no | nit | open-sync.md still names `~/.scratch/open-one-flush/loaded.sh` for the loaded run, while f8747be0 commits `bench/open_sync/loaded.sh`. | `docs/perf/open-sync.md` harness paragraph | Point at the committed script. |

## Not run

- No crash test on real storage: faultfs models the contract, not APFS or the drive cache. The
  macOS single-flush equivalence rests on reading completion:e49f288. No timings.
- macos-arm64 CI (cancelled, no runner). The local suite job ran on darwin arm64.
- The fallback branch (`to` below the newest generation, `commit_files`) was unchanged and not
  re-examined.

## Re-check (strata:ab0e26a6)

Verdict stays APPROVE. `git diff c35ce7e0..ab0e26a6` touches only `bench/open_sync/{build,loaded,mutant,run}.sh`
and `docs/perf/open-sync.md`: no engine or test change. The commits the new jobs ran at (8499e781,
69b76e9c) are ancestors of ab0e26a6, with no diff to it in `include`, `src`, `test` or the four
scripts. The scripts do what open-sync.md says:
- `build.sh` builds `idle_index` (a target at main d8ae10b8 too) at `$JOB_REF_MAIN` and at the
  checkout, then builds 1,000 indexes of 100 documents.
- `run.sh` warms up each side, then runs interleaved AB/BA pairs, capped at 8 minutes.
- `loaded.sh` adds a 4 MiB write-and-`fsync` loop with no `F_FULLFSYNC`.
- `mutant.sh` is my M1/M2, keyed to `index_test.cc:182` and `:137`. Jobs 261112-025813-0362d3
  and 261112-025815-cc2c50 are cited.

N1 to N3 are fixed: the Linux wording, the mutant rows in the evidence table, and the harness path.
