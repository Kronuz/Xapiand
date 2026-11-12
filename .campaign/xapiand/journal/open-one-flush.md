# open-one-flush: journal

State: waiting on jobs 261112-023556-0291d4
Quest: .campaign/xapiand/quests/open-one-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

A2 probe 261112-023556-0291d4 (strict, not evidence): run.sh with a concurrent writer
(~/.scratch/open-one-flush/loaded.sh: 4 MiB written and fsync'd in a loop) to put the device in
the dirty-cache mode the prototype's run saw.

## Next

1. Read the probe; bring A2's outcome to the lead (Blocked on); fill open-one-flush timing in docs.
2. CI run 43199523054 (queued at 14 min).

## Blocked on

Pending the probe: A2 as written is not met on a quiet device (below).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-022003-37891b (index_test passes at strata:c35ce7e0, T4 cases 9 and 10 included); mutant 261112-022005-2de6e1 at strata:65fd2afd fails case 9 (`index_test.cc:137`, the reopened generation lost after the power loss, 31 failures then exit) |
| A2 | 261112-022029-add889 (strict, after build 261112-022027-c52fb1): quiet device, both sides 0.13-0.18 s per 1,000 opens, p50 102-117 us; p99 lower after in 3 of 6 pairs (pairs before/after: 250/251, 300/247, 187/231, 239/254, 215/213, 245/204 us); max ~21 ms on both. Not met as written. |
| A3 | suite 261112-022007-45e212 passes at strata:c35ce7e0; CI 43199523054 pending |

## Result

Pending.

## Rulings I made

1. Re-derived the prototype on main: the open's `sync_dir(); log_->sync_data();` becomes
   `io_->sync_files_and_directory({&log, 1})`, keeping `sync_dir()`'s `unlinked_ = 0` (a directory
   sync frees removals' bytes), which the prototype dropped (0 at that point today; kept for
   equivalence).
2. The harness `bench/open_sync/{build,run}.sh` is outside the file set: the A2 jobs run it from
   strata:54f9e0af via `--ref HARNESS`, unmodified; docs cite that commit.
3. Linux wording in open-sync.md corrected: the log's `fdatasync` becomes `fsync` after a
   `sync_file_range` hint (completion posix.h `sync_files`), not "the same fsyncs as before".
