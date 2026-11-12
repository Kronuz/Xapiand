# open-one-flush: journal

State: blocked on lead: A2 workload ruling
Quest: .campaign/xapiand/quests/open-one-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing running. CI 43199523054 green on Linux; macos-arm64's rerun queued (it never got a runner).

## Next

1. Lead rules on A2 (below); if the loaded run counts, it is retagged or rerun with `--for A2`.
2. Check macos-arm64's rerun of CI 43199523054 (not needed for A3).

## Blocked on

Lead: A2 workload. The quiet-device strict run (261112-022029-add889) does not meet A2 as written
(p99 lower in 3 of 6 pairs; both sides ~0.1 ms, a full flush of a clean cache costs nothing). The
strict probe with a concurrent fsync writer (261112-023556-0291d4, `run.sh` unchanged under
`~/.scratch/open-one-flush/loaded.sh`) meets it: p99 lower in 5 of 6 pairs, p50 45.5 to 23.0 ms,
totals 277.3 s to 148.7 s. Options: (1) accept the loaded run as A2's job (recommended: a host with
many indexes opens while others commit, the state the prototype's slow mode saw); (2) rerun the quiet
run (likely the same tie); (3) restate A2 as "no regression on a quiet device, median halves
under a dirty cache", which both jobs show.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-022003-37891b (index_test passes at strata:c35ce7e0, T4 cases 9 and 10 included); mutant 261112-022005-2de6e1 at strata:65fd2afd fails case 9 (`index_test.cc:137`, the reopened generation lost after the power loss, 31 failures then exit) |
| A2 | loaded probe 261112-023556-0291d4: p99 lower after in 5 of 6 pairs, p50 45.1-46.0 ms before vs 23.0-25.9 ms after (not tagged --for A2; ruling above). Quiet run 261112-022029-add889 (strict, after build 261112-022027-c52fb1): quiet device, both sides 0.13-0.18 s per 1,000 opens, p50 102-117 us; p99 lower after in 3 of 6 pairs (pairs before/after: 250/251, 300/247, 187/231, 239/254, 215/213, 245/204 us); max ~21 ms on both. Not met as written. |
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
