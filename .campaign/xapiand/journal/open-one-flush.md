# open-one-flush: journal

State: waiting on jobs 261112-025326-d8e7e1 261112-025328-896ded
Quest: .campaign/xapiand/quests/open-one-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A2 rerun from the checkout (lead, after the ruling): bench/open_sync/{build,run,loaded}.sh
committed at strata:f8747be0; build 261112-025326-d8e7e1 (light, `--ref MAIN=d8ae10b8`), then
strict 261112-025328-896ded (`sh bench/open_sync/loaded.sh ~/.scratch/open-one-flush/bench 1000 6`,
`--for A2`).

## Next

1. Read 896ded; fill its row in strata docs/perf/open-sync.md (Evidence and the loaded table) and
   A2 below; set ready for review.
2. Fold in the durability review's findings by id.

## Blocked on

Nothing. The lead ruled A2 (option 3): the loaded run is A2; the quiet run is reported as no
regression.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-022003-37891b (index_test passes at strata:c35ce7e0, T4 cases 9 and 10 included); mutant 261112-022005-2de6e1 at strata:65fd2afd fails case 9 (`index_test.cc:137`, the reopened generation lost after the power loss, 31 failures then exit) |
| A2 | pending: 261112-025328-896ded (strict, bench/open_sync/loaded.sh from the checkout at strata:f8747be0). Earlier probe 261112-023556-0291d4 (same harness from ~/.scratch, not rerunnable): p99 lower after in 5 of 6 pairs, p50 45.1-46.0 vs 23.0-25.9 ms. No regression: quiet run 261112-022029-add889, both sides ~0.1 ms per open, p99 lower in 3 of 6 pairs (tagged A2 by its submission; cited only as no regression) |
| A3 | suite 261112-022007-45e212 passes at strata:c35ce7e0; CI 43199523054 at strata:c35ce7e0: linux-x86_64 gcc and clang, linux-arm64-gcc, linux-asan-ubsan, linux-tsan, freebsd-x86_64, macos-x86_64 pass; macos-arm64 cancelled with no runner assigned (15 min queued), rerun requested |

## Result

strata:c35ce7e0: `basic_index::open` syncs the log and the directory with one
`sync_files_and_directory` (one F_FULLFSYNC on macOS) instead of `sync_dir()` then
`log_->sync_data()`; manifest-log D6.6 wording updated. Effect, strata docs/perf/open-sync.md: no
difference on a quiet device; under a dirty device cache the median open drops from about 45 ms to
about 23 ms (about 1.9x faster), and 1,000 opens take 46% less time. Open: Linux unmeasured; the
harness lives at strata:54f9e0af (bench/ is outside this file set).

## Rulings I made

1. Re-derived the prototype on main: the open's `sync_dir(); log_->sync_data();` becomes
   `io_->sync_files_and_directory({&log, 1})`, keeping `sync_dir()`'s `unlinked_ = 0` (a directory
   sync frees removals' bytes), which the prototype dropped (0 at that point today; kept for
   equivalence).
2. The harness `bench/open_sync/` (now in the file set) is taken from strata:54f9e0af unchanged,
   plus `loaded.sh`.
3. Linux wording in open-sync.md corrected: the log's `fdatasync` becomes `fsync` after a
   `sync_file_range` hint (completion posix.h `sync_files`), not "the same fsyncs as before".
