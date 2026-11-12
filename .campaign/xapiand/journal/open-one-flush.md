# open-one-flush: journal

State: waiting on jobs 261112-022003-37891b 261112-022005-2de6e1 261112-022007-45e212 261112-022027-c52fb1 261112-022029-add889
Quest: .campaign/xapiand/quests/open-one-flush.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs queued at strata:c35ce7e0 (quest head); CI pushed (quest/xapiand/open-one-flush).

- A1 pass: 261112-022003-37891b (index_test at head). A1 mutant: 261112-022005-2de6e1 at
  strata:65fd2afd (branch mutant/open-one-flush-sync-owed; syncs only on `rollover_owed_`); exits 0
  only when index_test exits nonzero with a CHECK in case 9 (lines 105-140).
- A3 suite: 261112-022007-45e212. CI: GitHub Actions on the pushed branch.
- A2: build 261112-022027-c52fb1 (light), then strict 261112-022029-add889 (`run.sh`, 6 pairs,
  stops starting pairs at 8 min); out dir ~/.scratch/open-one-flush/bench.

## Next

1. Read the jobs; fill Evidence and the timing table in strata docs/perf/open-sync.md; check CI.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |

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
