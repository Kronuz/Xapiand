# open-one-flush: journal

State: ready for review
Quest: .campaign/xapiand/quests/open-one-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing. Durability review is the lead's (launched in parallel).

## Next

1. Fold in the durability review's findings by id.
2. Check macos-arm64's rerun of CI 43199523054 (not needed for A3).

## Blocked on

Nothing. The lead ruled A2 (option 3): the loaded run is A2; the quiet run is reported as no
regression.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-022003-37891b (index_test passes at strata:c35ce7e0, T4 cases 9 and 10 included); mutant 261112-022005-2de6e1 at strata:65fd2afd fails case 9 (`index_test.cc:137`, the reopened generation lost after the power loss, 31 failures then exit) |
| A2 | 261112-023556-0291d4 (strict, concurrent fsync writer, `--for A2`; the resubmit reused it: same command at code-identical strata:c35ce7e0, dd3c3a4a and later are docs-only): p99 lower after in 5 of 6 pairs, p50 45.1-46.0 ms before vs 23.0-25.9 ms after, totals 277.3 s vs 148.7 s. No regression: quiet run 261112-022029-add889, both sides ~0.1 ms per open, p99 lower in 3 of 6 pairs (still tagged A2 by its submission; cited only as the no-regression row) |
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
2. The harness `bench/open_sync/{build,run}.sh` is outside the file set: the A2 jobs run it from
   strata:54f9e0af via `--ref HARNESS`, unmodified; docs cite that commit.
3. Linux wording in open-sync.md corrected: the log's `fdatasync` becomes `fsync` after a
   `sync_file_range` hint (completion posix.h `sync_files`), not "the same fsyncs as before".
