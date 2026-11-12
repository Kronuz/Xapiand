# open-one-flush: journal

State: ready for review
Quest: .campaign/xapiand/quests/open-one-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Nothing. Durability review APPROVE; notes folded:
- N1: fixed at strata:8499e781 (open-sync.md: the log's `fdatasync` becomes `fsync` on Linux).
- N2: `bench/open_sync/mutant.sh M1|M2`; M1 (log only) 261112-025813-0362d3 fails case 10 at
  `index_test.cc:182`; M2 (directory only) 261112-025815-cc2c50 fails case 9 at `:137`.
- N3: fixed at strata:8744f5a3 (harness paragraph names `bench/open_sync/loaded.sh`).

## Next

1. Land (lead). macos-arm64's CI rerun is not needed for A3.

## Blocked on

Nothing. The lead ruled A2 (option 3): the loaded run is A2; the quiet run is reported as no
regression.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-022003-37891b (index_test passes at strata:c35ce7e0, T4 cases 9 and 10); mutants `--fails-without`: rollover_owed_-only 261112-022005-2de6e1 fails case 9 (`:137`); M1 log only 261112-025813-0362d3 fails case 10 (`:182`); M2 directory only 261112-025815-cc2c50 fails case 9 (`:137`) |
| A2 | 261112-025328-896ded (strict, `bench/open_sync/loaded.sh` from the checkout at strata:f8747be0, after build 261112-025326-d8e7e1): p99 lower after in 6 of 6 pairs (28-39 vs 60-76 ms), p50 45.3-45.8 vs 23.0-23.1 ms (about 2.0x faster), totals 277.9 vs 138.4 s. No regression: quiet run 261112-022029-add889, both sides ~0.1 ms per open, p99 lower in 3 of 6 pairs (cited only as no regression). Earlier probe 261112-023556-0291d4 agrees. |
| A3 | suite 261112-022007-45e212 passes at strata:c35ce7e0; CI 43199523054 at strata:c35ce7e0: linux-x86_64 gcc and clang, linux-arm64-gcc, linux-asan-ubsan, linux-tsan, freebsd-x86_64, macos-x86_64 pass; macos-arm64 cancelled with no runner assigned (15 min queued), rerun requested |

## Result

strata:c35ce7e0: `basic_index::open` syncs the log and the directory with one
`sync_files_and_directory` (one F_FULLFSYNC on macOS) instead of `sync_dir()` then
`log_->sync_data()`; manifest-log D6.6 wording updated. Effect, strata docs/perf/open-sync.md: no
difference on a quiet device; under a dirty device cache the median open drops from about 45 ms to
about 23 ms (about 2.0x faster), and 1,000 opens take 50% less time (job 261112-025328-896ded).
Harness and mutants in strata bench/open_sync/. Open: Linux unmeasured.

## Rulings I made

1. Re-derived the prototype on main: the open's `sync_dir(); log_->sync_data();` becomes
   `io_->sync_files_and_directory({&log, 1})`, keeping `sync_dir()`'s `unlinked_ = 0` (a directory
   sync frees removals' bytes), which the prototype dropped (0 at that point today; kept for
   equivalence).
2. The harness `bench/open_sync/` (now in the file set) is taken from strata:54f9e0af unchanged,
   plus `loaded.sh`.
3. Linux wording in open-sync.md corrected: the log's `fdatasync` becomes `fsync` after a
   `sync_file_range` hint (completion posix.h `sync_files`), not "the same fsyncs as before".
