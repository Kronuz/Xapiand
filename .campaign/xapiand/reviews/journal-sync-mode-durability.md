# journal-sync-mode review: durability

Commit: journal:fe050d1fdfa59512a3455eb1e1c51e4808da6167 (quest/xapiand/journal-sync-mode; pins completion:b4600a8, faultlab:83fbde7)
Verdict: APPROVE
Date: 2026-11-24

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- 261124-122708-2f04ab (fe050d1, `journal_sync_mode_test`, exit 0, 56 checks). On this Mac, full
  records commit `[F_FULLFSYNC]`, run on the native queue `[F_FULLFSYNC]`, and a checkpoint
  publication of `fsync F_FULLFSYNC` pairs ending in `F_FULLFSYNC`. Plain records only `fsync`. The
  refusal case fails the refused turn with `ENOTSUP` and records nothing after the refused call.
  The table case matches each site's last call to faultlab's `call_for` with the same `persists`.
  This is enough for the turn barrier on both paths and for one publication; my probes cover the
  rest of Q1, Q2 and Q6.
- 261124-122713-058d9e (fe050d1, T9 full and plain, exit 0). Full: 5,600 crash points (1,861 with a
  second loss during recovery) and 0 failures. That means no damage, no lost group, every
  acknowledged record kept, and the in-flight turn either whole or absent. Plain: 0 failures, and
  each process crash passes the full checks. On power losses, 5,238 lost an acknowledged record,
  2,673 reported damage, and 1 (block 1024) kept part of a turn.
- 261124-122716-1b2259 (mutant journal:d7f0a7d, T9 opens full as plain, `--fails-without`). Exit 1
  with 10,539 failures across all four geometries. The crash model fails full→plain.
- 261124-122720-bcba79 (mutant journal:d56e937, sync_mode_test opens full as plain,
  `--fails-without`). Exit 1 with 5 failures: `full mode flushes the drive after every fsync` and
  `a turn barrier is one call, the one the mode names`.
- 261124-122735-d94c86 (strict A/B, 7 rounds). Full makes 10.3 appends/s against main's 9.9 at one
  operation per batch, and 2,633 against 2,403 at 256. Plain makes 731.9 appends/s. Full costs what
  main costs, which corroborates that it makes main's flushes.
- CI 45480924916 at fe050d1. Green on Linux (x86_64 gcc and clang, arm64 gcc, ASan/UBSan, TSan),
  FreeBSD x86_64 and macOS x86_64. `journal_sync_mode_host`, `_refusal` and `_table` passed on
  each. Off macOS, `host` asserts `off macOS both modes make the same calls`
  (test/sync_mode_test.cc:239-243). macOS arm64 was still queued when I wrote this.

## What I checked myself

Scripts are in `.scratch/xapiand/review-journal-sync-mode-durability/`: `job.sh` and
`job-lifecycle.sh` (the job commands), `CMakeLists.txt` (built against `$JOB_SRC`), `recorder.cc`,
`probe_t9.cc` and `probe_lifecycle.cc`.

- Static checks:
  - `include/` makes no sync system call, constructs no `PosixIO` and no `PosixIOOptions`, and
    defines no `File`. Its only `IO` is `PrefixIO`, which forwards every sync and open to the
    host's `IO` (host.h:343-347).
  - Every journal sync is one of the following `IO` calls: `sync_files`, `sync_data_files`,
    `sync_directory` and `order_files_and_directory`. primitives.h:52-54 wraps the first two.
  - The only call that bypasses an `IO` is the queue worker's `File::sync_data`. On macOS it runs
    on the file's copied mode, because the AIO backend issues a native fsync only on FreeBSD
    (completion bsd_completion.h:105-108).
  - The quest changes nothing under `include/`, so recovery is the code main ships.
- 261124-130552-389d38 (fe050d1, light; it exits 1 because of the three lifecycle coverage checks
  described below):
  - `probe_t9 sweep 8` replays T9's sweep (the same seeds, cuts, second losses and crash seeds)
    in both modes and records what each crash left. It reproduces T9's tallies exactly: 5,600
    points; plain 5,238 lost, 2,673 damaged, 1 torn.
    - Plain: 2,565 of the 5,238 losses are silent (no damage, no lost group). 2,460 bounded
      groups each keep a state's records past their bound (0 bad), and nothing is invented.
      `TurnLog::read` and the open agree at every point.
    - Full: 0 damage and 0 lost. 1,242 points restore a segment name (a release whose directory
      sync had not run), and none of those names' replacements had turns written.
  - `probe_t9 one 1 76 1024 plain|full` decodes T9's torn point in each mode (see Q5).
  - `probe_lifecycle` runs on real files, recording through sync_mode_test's recorder:
    - Refusal matrix: in full with every `F_FULLFSYNC` refused, 12 durable operations each fail
      with `ENOTSUP` (45), and their calls end at the refused `F_FULLFSYNC`. The operations are:
      host open, host reopen, table commit, a turn by `commit()`, a turn by `run()` on the native
      queue, artifact finish, checkpoint publication, `close_group`, `delete_group`, `Journal`
      create, `Journal` `append_batch`, and the turn log's spare preallocation.
    - Reopen in the other mode follows the new `IO`. Plain→full records `fsync F_FULLFSYNC fsync
      F_FULLFSYNC F_FULLFSYNC F_FULLFSYNC`; full→plain records 4 `fsync`.
    - Two hosts in one process, one full and one plain, each recorded by its own seam: the full
      host makes 6 `fsync` and 15 `F_FULLFSYNC`, the plain host 15 `fsync` and nothing else.
    - Lifecycle: a host's life in each mode keeps its mode's shape (full: 62 `fsync`, 119
      `F_FULLFSYNC`, no barrier; plain: 123 `fsync` only). But segments were removed 7 times and
      never recycled, and plain made no `F_BARRIERFSYNC`. My workload prepared a spare after
      every turn, so every released segment was removed. No reclaim released two segments, and
      turn_log.h:839 is the journal's only ordering call. These are coverage misses, not product
      failures; round 2 changes the workload.
  - `rec_*`: the journal's own POSIX tests and demo, linked with `recorder.cc`, which replaces
    completion's default seam (`sync_seams.entries[0]`) and checks each thread's calls at exit.
    The tests are journal, completion, turn_log, crc_fixture, inventory, sync_seam, host
    `loop_slots` and `loop_views`, turn_seal `supersede`, and the demo in full and in plain.
    - All in full: 784 `fsync` and 2,084 `F_FULLFSYNC` on up to 3 threads, 0 `F_BARRIERFSYNC`, and
      every `fsync` run ends in an `F_FULLFSYNC` (0 broken). Every test passed.
    - The plain demo made no `F_FULLFSYNC`.
- 261124-142654-0f4410 (round 2, `job-lifecycle.sh`) reruns `probe_lifecycle` with a workload
  that recycles a segment, writes into the recycled file, and releases several segments in one
  reclaim. It was still building when I handed this review back: its result is not in this
  review. If it fails a shape check (rather than a coverage check), Q6 reopens as a blocking
  finding.

## Answers

1. No path reaches a plain sync in full mode.
   - A `PosixIO` can't exist without a mode: `FlushMode` has no default
     (completion posix_sync.h:49-54, posix.h:61 and 78).
   - The journal adds no sync of its own (static checks above).
   - Recorded calls agree: 2f04ab covers the turn barrier on both paths and a publication. The
     `rec_*` tests and the lifecycle, reopen and two-host probes cover directory barriers, table
     and artifact syncs, and reopen (recycled segments by construction; round 2 records them), with no bare `fsync` or
     `F_BARRIERFSYNC` reached in full mode.
2. Yes, shown by a run: the refusal matrix covers every durable entry point I found, and each
   fails with the refused call's errno with nothing weaker after it. A refused turn leaves the
   turn log failed on both the `commit()` and `run()` paths.
3. Yes.
   - faultfs drops plain-synced writes: sectors and names that were only plain-synced are drawn
     at a power cut (Q5's decoded point), and 5,238 of 5,600 plain points lose an acknowledged
     record.
   - The full→plain mutants fail: 1b2259 with 10,539 failures in the crash model, bcba79 with 5
     in the call table.
4. Yes for what it claims: off macOS both modes make the same calls.
   - `sync_call` names one call per site in both modes off macOS (posix_sync.h:75-92).
   - io_uring's barrier is `IORING_OP_FSYNC` (`DATASYNC` for data) in either mode
     (linux_completion.h:279-290).
   - FreeBSD AIO uses `aio_fsync(O_DSYNC|O_SYNC)` in either mode (bsd_completion.h:133).
   - CI's `journal_sync_mode_host` asserts the equality on Linux and FreeBSD.
   - "As the filesystem's fsync" defers to the filesystem and is not tested by anything here (nor
     can it be).
5. What happened at T9's torn point (seed 1, cut 76, block 1024), and the answers:
   - Layout: turns 1-9 were in segment 1, 10-17 in segment 2, 18-24 in segment 3, and 25 and the
     in-flight 27 in segment 4.
   - Before the cut, segment 1 was removed, not recycled: both 1 and spare 5 exist after the crash.
   - In plain mode the run had no ordering point. Directory syncs are plain `fsync`s, which
     persist nothing in faultfs, and no reclaim released two segments. So every name and sector
     was still in the drive's cache.
   - The crash undid segment 1's removal, dropped segment 2's never-durable name (taking its 8
     acknowledged turns), and kept segment 1's sectors 0, 1 and 7.
   - Recovery read turn 1 whole and turn 2 up to its first record (1:1), then complete turns 19
     and 20 in segment 3. Turn 2 is incomplete with complete turns after it, so recovery reported
     damage and kept 1:1 (turn_log.h:1046-1056).
   - Every kept record was written and is in its place. Each group keeps a prefix, never a
     reorder within the group. That is exactly what faultfs's plain model allows.
   - Full mode cannot reach this. Its directory sync ends in `F_FULLFSYNC`, and so does every turn
     barrier, so no acknowledged turn sits behind a non-durable name or sector. The same seed and
     cut in full recovers every group at its acknowledged state with the in-flight turn whole and
     no damage; the full sweep is clean.
   - "Reported damage" is right. Turns 19-20 survived after missing 3-18, so this is not a
     shorter log, and cutting at the first hole would drop surviving acknowledged turns. Recovery
     also can't know the mode the bytes were written in: a directory written plain reopens full
     (the reopen probe), so a plain-only "clean" verdict would hide the same pattern as real
     damage in full mode.
   - For Detent: in plain mode, neither the damage flag nor a group's lost status says whether
     that group lost acknowledged records (D2, D4).
6. No.
   - The journal opens no `PosixIO` and no file except through its `IO` or `PrefixIO`.
   - A recycled segment is reopened with `io_.open_existing` after its rename
     (turn_log.h:843-845), so it has the same `IO`'s mode: each `PosixFile` copies its `IO`'s mode
     at open, and no API opens a file under another. Round 2 (0f4410, pending) records one written
     in each mode.
   - A reopen and a second host each follow their own `IO` (probes above).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D1 | no | gap | T9 plain skips lost groups, so a bounded group, whose records after its bound the host keeps and serves (host.h:891), is never checked in plain mode. | test/turn_log_power_test.cc:109, 116. My sweep checked them: 2,460 bounded groups, 0 not a state's records past the bound (389d38). | Check bounded groups in T9 plain as the probe does (`bounded_ok`); optional. |
| D2 | no | nit | README.md:111-112 says plain "recovery still reports damage instead of inventing records", which reads as if plain losses are reported. 2,565 of 5,238 losing points are silent, and a damaged recovery can keep a rewound group unreported (D4). ARCHITECTURE.md:287-289 ("reports what it cannot prove") and sync-mode.md's table are right. | 389d38 `t9-sweep`; README.md:111-112 | "can lose acknowledged records, silently or with damage reported; it never invents a record". |
| D3 | no | nit | docs/mechanisms/sync-mode.md:62 attributes the torn point to "a recycled segment's name restored by the crash". Segment 1 was removed, not recycled, and the turns were lost with segment 2's name, which a plain directory sync never made durable. sync-mode.md:29's row "Turn log segment change" names `order_files_and_directory`, which the turn log calls only between two releases in one reclaim (turn_log.h:839), not at a segment change. | 389d38 `t9-one-plain`, `lifecycle` (plain: 0 `F_BARRIERFSYNC` over 7 releases) | Reword both. |
| D4 | no | gap | Pre-existing and outside this diff. Recovery charges a missing-turn range to the gaps it finds in it and charges a segment's tail only when no gap is found (`missing_between`, turn_log.h:1034-1042). A vanished segment leaves nothing to charge. At T9's torn point, turns 3-17 held 1:2-1:5 and 5:1-5:2, and no manifest names their blocks, yet only groups 2-4 (named by segment 3's gaps) are lost. Group 1 keeps 1:1, which is 6 acknowledged records behind 7c, unreported. That is narrower than the rule "where no manifest reads back, it costs every group" (turn_log.h:982-985). Plain promises only a per-group prefix, so this is allowed there, and full mode never reaches it by a crash (0 damage in 5,600 points). It is reachable by real damage that loses a segment file. | 389d38 `t9-one-plain` | The owner's call: keep it, or treat missing turns that a vanished segment or an uncharged tail may have held as unknown (every group lost). A fails-without test: this T9 point at the TurnLog level. |
| D5 | no | nit | sync-mode.md:31 says "FreeBSD makes `fsync` everywhere", but its AIO run barrier is `aio_fsync(O_DSYNC)` for data. | completion bsd_completion.h:133 | Name it. |

## Notes

- I ran as a subagent of the lead, not as a session titled
  `xapiand · reviewer journal-sync-mode durability`.

## Not run

- Round 2 (0f4410) had not finished: recycled segments rest on the code (turn_log.h:843-845) and
  on round 1's shape checks, which covered every other call.
- No probe ran on Linux or FreeBSD. Those platforms rest on CI's `journal_sync_mode_*` and the code
  references in Q4.
- No power cut on real hardware. Both modes are checked only through faultfs's model and the
  recorded calls.
- No host-level plain crash sweep. T9 and my sweep recover a `TurnLog` without a host table, so a
  host's table (open groups, bases) may report more groups lost than D4's point shows.
- Neither T9 nor my sweep checks cross-group consistency in plain mode. The decoded point keeps
  turns 19-20 after losing 3-18.
- The A/B numbers (d94c86) are corroboration only.
