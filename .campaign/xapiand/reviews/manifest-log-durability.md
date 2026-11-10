# manifest-log review: durability

Commit: strata:e0508e9805e612ac78b0475d6db6817a04c716c8
Verdict: CHANGES
Date: 2026-11-10

APPROVE: lands as is, at this commit's code. CHANGES: lands after each blocking finding is fixed
and confirmed by a job at the fixed commit. REJECT: the approach is wrong; the quest is re-planned.
An APPROVE lists no blocking finding; a CHANGES lists at least one.

## Evidence I relied on

- 261109-212058-c999f8 (build-test at e0508e9, full ctest): manifest_log_test's crash images,
  bit flips, caller rules and allocation failure; media_test (every byte of the log); lifecycle's
  T4 case 1 (power loss at every mutation of rollovers and lazy cleanups); index_test's case 9
  (process kill before the edit's sync, then power loss). `manifest_log.hh` is unchanged since
  strata:4d2a541, so A7's log-layer evidence holds at this commit through this job.
- 261109-212222-4d4b55 (CI's config: assertions, clang ctest, libstdc++ trio, the restored
  `noexcept` terminating apply_atomic) and 261109-212117-5b1128 (31 log and index mutants, the
  R29 and R32 mutants among them): enough for R29, R30 and R32 as written.
- 261109-212247-2389ea (A9 at e0508e9) and 261110-021407-5ffb02 (T6, done, exit 0).
- Not relied on: the queued reruns 9e3973, 23571b, fd1e2f, a9800f.

## What I checked myself

Probe job 261110-022246-d64be6 at e0508e9 (light), from
`.scratch/review-manifest-log-durability/run_probes.sh`, which builds `rename_probe.cc` and
`sync_error_probe.cc` against the job's checkout (log copied to `out/`). Rerun it at the fixed
commit with the same `campaign job submit` line that heads the script.

Ordering, by platform (completion pinned at e2c6474, `posix.h`):

- The commit's ordering point (`index.hh:4959` for an append, `:4935` for a rollover) covers new
  segments, merge outputs and the rollover's temporary. On Linux and FreeBSD it fsyncs each file
  and then the directory (`posix.h:597-613`). On macOS it fsyncs each file and the directory, then
  issues F_BARRIERFSYNC on the directory's device (`posix.h:186-224`, ordered mode). Install's
  copies are fsynced one by one in `write_new` (`index.hh:4988`), and the rollover's ordering
  point then fsyncs the directory entries.
- The edit: one `pwrite` at E (`manifest_log.hh:1166-1171`), then `sync_data`: fdatasync on
  Linux, F_FULLFSYNC on macOS (the barrier's second phase), fsync on FreeBSD (`posix.h:167-177`).
  A segment and its directory entry therefore reach the device before an edit that names them, on
  all three platforms.
- Rollover: the ordering point, `replace`, then `sync_directory`, which is fsync(dir) plus
  F_FULLFSYNC on macOS (`posix.h:614-628`, `index.hh:4934-4940`). The truncate-back is ftruncate
  then `sync_data` (`manifest_log.hh:1201-1208`).
- The open: one `sync_data` on the log (`index.hh:5244`) and no directory sync. That gap is F1.

Torn and corrupt tails (journal framing at 5bbeedc, `framing.h:148-175`, `:262-311`): the fragment
CRC covers its length, type, segment and epoch (`:173`); the block-header CRC covers the header.
A length that runs past the end of file is cut, which reads as damage. A length that stays inside
the file fails its CRC, and since the bytes are not zero that is also damage. A zero run at the
failure point reads as torn only when no whole record follows. Damage in the snapshot, or a stop
inside the first record, refuses the open (A6). Damage past `to` is reported, not accepted (R6).
I found no path that accepts corrupt metadata silently.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A rollover whose rename is visible but not yet directory-synced loses acknowledged commits. The writer stops between `replace` and `sync_directory`: a process kill there, an EIO from either call, or an allocation failing at `files_[name]` (`index.hh:4936-4939`). The host then reopens in the same boot and gets the new log's head. The open syncs only the log (`index.hh:5244`), and an append that writes no file orders nothing (`index.hh:4959`), so later delete-only or metadata-only commits are acknowledged while the rename can still be lost. A power loss then reverts to the old head, and the open unlinks the `.tmp` that holds the acknowledged edits (`index.hh:5251-5253`). If retention has already unlinked the old log lazily, the index reads as "no manifest log". `main` had no such window: every commit fsynced the directory. D6.6's own rule for edits applies here: "the open must not report that generation durable while a power loss can still take it back". The T4 crash matrix never covers a process crash followed by more commits before the power loss. | 261110-022246-d64be6, `rename_probe`. For each fault kind (replace failing after its effect, or the directory sync after it failing), a metadata-only or delete-only acknowledged commit is lost at 32 of 64 power-loss seeds; at 14 of those the open fails with "no manifest log". Control (a commit that writes a segment, so its ordering point fsyncs the directory): 0 of 64. | Option (a): the open fsyncs the directory before its log `sync_data` (on macOS the log's F_FULLFSYNC already flushes the device, so this adds one fsync(dir)). Option (b): the first append to a log this process did not roll over to orders the directory first; this is cheaper on read-only opens, but the open still reports a generation a power loss can revoke. I recommend (a), measured in the open-cost row. Add `rename_probe`'s shape as a test. |
| F2 | yes | defect | A failed `sync_data` on an edit fences but leaves the edit's bytes in the log (`manifest_log.hh:1180-1190`; only a failed write is undone). On Linux, pages that failed writeback stay in the page cache marked clean, and errseq reports the error once, to the failing call. A same-boot reopen therefore reads the edit, its own `sync_data` succeeds without writing the edit (contrary to D6.6's "a writeback that failed earlier included, fails the open"), and the reopen reports that generation. Later acknowledged edits that start on a fresh page then sit behind a zero run, so after a power loss the open refuses with damage. `open(..., head)` recovers only by dropping them. faultfs models a failed sync as dropping the unsynced bytes (`faultfs.hh:446-456`), so no test can see this. `main` never left a failed sync's bytes under a live name, because a manifest was renamed only after its sync. | 261110-022246-d64be6, `sync_error_probe`, with the power-loss image built by hand from those Linux semantics: the reopen reported generation 3 (the head was 2); generation 4 was acknowledged; after the image, "damaged at offset 206, generation 3"; `open(..., 2)` opens without the acknowledged b. | On a failed sync, undo as a failed write does (truncate to E, then `sync_data`) before fencing; a failed undo breaks the appender as today. Model the clean-but-not-durable page in faultfs (an option on the failed sync), so a test fails without the fix. |
| N1 | no | gap | A short count from `pwrite` is thrown as EIO (`manifest_log.hh:1169`), so a disk that fills partway through an edit fences instead of reporting `space_error`, contrary to D4's "ENOSPC leaves the index as it was". faultfs never writes part of a request at capacity (`faultfs.hh:406-409`), so this is untested. Data is safe: the write is truncated back. | code reading | Report a short count at the device's capacity as ENOSPC (or probe with `fstatvfs`), and let faultfs's capacity write a partial count. |
| N2 | no | nit | The append's ordering point (`index.hh:4959`) sits outside `durable()`: an allocation failing there escapes unfenced. This is safe, since nothing in the log changed and a retry fsyncs again. | code reading | None needed; a comment would help. |
| N3 | no | nit | On macOS, ordering relies on F_BARRIERFSYNC, which Apple guarantees only on its own SSDs; `full_flush_ordering` is off by default (`posix.h:418-422`). Unchanged from `main`. | code reading | Owner's call; leave as is. |

Risk 4 (the e0508e9 fixes) holds as written. R29 is covered by cold_segment and its mutant, R30 by
CI's config job, and R32 by the burst-1 sweep and its mutant. The sweep's fenced case reopens
without a power loss (`apply_atomic_test.cc` `commit_attempt`), so an allocation failure combined
with a crash is not proven. For the rollover it reaches F1's state.

Risk 1's recovery idempotence holds otherwise. The open writes only `.tmp` unlinks and one log
sync; `open(..., to)` rolls over above every generation on disk (R13); lifecycle counts crash
points inside recovery.

## Not run

- The queued reruns at e0508e9 (mutants 9e3973, T7 23571b, fuzz fd1e2f, A9 a9800f) and CI's
  sanitizer and Linux matrix, which are CI's.
- No real-filesystem crash test (dm-flakey, power cut). F1 is shown on faultfs's POSIX-strict
  namespace model. ext4 and XFS order a rename ahead of a later fdatasync's journal commit in
  practice; APFS, UFS and ZFS were not checked. F2 rests on Linux's documented writeback-error
  behavior, and its image was built by hand.
- No probe of install or `open(..., to)` under allocation failure, and none of D8's allowance
  under ENOSPC beyond the contributor's T4 case 7.

Tokens: ~135k
