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

## Re-check

Commit: strata:6ab3983bc3deef58a1f26206b48e62d3d036f87f
Verdict: CHANGES
Date: 2026-11-10

Scope: `git log e0508e9..6ab3983b` for F1 (strata:c78f8702), F2 (strata:f9fdcca3) and the post-commit
fence R39 (strata:8b532c81, strata:6ab3983b). F1 and F2 are closed. R39 closes the unfenced-but-durable
window, and exercising it with a live reader found a use-after-free in the publication step it now
fences (F3 below, blocking).

### Evidence relied on

- 261110-092610-3fe121 and 261110-092610-707186 (build-test family at 6ab3983b: the v1 rename probe, the
  ENOSPC probe), 261110-092610-0da306 (build-test, Release, ctest), 261110-092610-9b9471 (mutants,
  including "the open syncs the log only" and "a failed barrier not truncated back"). Not rebuilt.
- No sanitizer job existed at 6ab3983b (CI's `linux-asan-ubsan` is not at this head; build-test is
  Release). My ASan+UBSan job below is the first.

### What I checked myself

Scripts in `/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-manifest-log-durability-r2/`. Rerun:
`campaign job submit --quest manifest-log --commit <sha> --level light --minutes 90 -- sh
.../run.sh rel` (and `asan`), and `... -- python3 .../mutants.py`.

- `alloc_crash_probe.cc` (jobs 261110-103338-1f8973 Release, 261110-103339-84a301 ASan+UBSan). An
  allocation fails at every point of a commit (alone, and with the next one failing too) in three
  scenarios (rollover owed, an edit that writes a segment, a metadata-only edit), each without and with
  a live reader holding the current view across the commit. Whatever the outcome the host reopens (no
  power loss), commits a metadata-only edit (acknowledged), then six power-loss seeds. Per point it
  checks: the reopen and post-crash open succeed; the acknowledged edit survives; a commit that threw
  and is visible at the reopen is still there after the power loss (D6.6); a commit that threw and is
  durable left the writer fenced; a fenced writer refuses commits. Release result: 12 scenario-burst
  combinations, 12 to 145 allocation points each, 0 violations of any of those checks, including 12 to
  23 fenced-and-durable points per scenario. The only failure is the held-view read (F3).
- `sync_probe.cc` (same jobs). The log's `sync_data` fails before or after its effect, pages dropped or
  kept clean (Linux), edits of 10, 3,000 and 9,000 bytes at three offsets in the log, 16 power-loss
  seeds each: 36 of 36 single-fault cases fence, leave the log byte-identical to before the edit
  (206 to 206, 628 to 628, 4,365 to 4,365 bytes, 4,364 for "after, kept"), reopen at the head before the
  edit, and lose no acknowledged commit. Also with the undo's own sync failing: the same, all 36.
- `mutants.py` (job 261110-104118-99fd52, exit 1 by design: 2 of 4 killed). Details under R39.
- apply_atomic under ASan+UBSan at 6ab3983b (same ASan job): ok, 153 allocation points, 20 fenced
  (17 after the edit was durable), no report. No reader reads after the failure in that test (F3).

### Per finding

| ID | Blocking | Kind | Finding | Evidence | Status / fix |
|---|---|---|---|---|---|
| F1 | no | defect | Closed. The open runs `sync_dir()` before opening and syncing the log (`index.hh:5246`); the `to` fallback goes through `commit_files`, whose rollover syncs the directory (`index.hh:4943-4950`). Every stop point the review named ends in one state (new name visible, directory entry unsynced, writer fenced or dead) and the reopen now makes it durable before it reports a generation: a process kill, EIO from `replace` after its effect, EIO from the directory sync, and an allocation failing at `files_[name]` (inside the rollover's `durable()` block, `index.hh:4947`). | T4 case 10 (`index_test.cc:138-179`) plus my sweep: "rollover owed" with every allocation failing in turn (32 and 42 points, burst 0 and 1), reopen, metadata-only commit, power loss at 6 seeds: 0 lost, 0 failed opens; v1 probe 261110-092610-3fe121 (exit 0). Mutant "the open syncs the log only" killed in 261110-092610-9b9471. | none |
| F2 | no | defect | Closed for what the review named. A failed `sync_data` undoes (truncate to E, `sync_data`) before the appender breaks and the index fences (`manifest_log.hh:1267-1277`, `undo` at `:1289-1296`, which swallows its own failure and sets `broken_`, so the original error propagates and the fence follows). The log is byte-identical to before the edit on all 36 single-fault cases and when the undo's sync fails too. See N1 for the double fault. | `sync_probe` in 261110-103338-1f8973 and 261110-103339-84a301; T4 case 11, faultfs_test's `keep` model; mutant "a failed barrier not truncated back" killed. | none |
| F3 | yes | defect | Use-after-free of a view a reader holds, when an allocation fails in `publish_view`'s `retire` (`index.hh:3036`). `publish_view` stores the new view's pointer into the registry (`reader_registry::publish(rs.current, next.get())`) and then calls `retired.retire(std::move(pub_->current))`, whose `list_.push_back` can throw; the by-value `unique_ptr` parameter then frees the old view while a reader holds it, and `next`, a local, is freed while `rs.current` names it, so the next `acquire()` reads freed memory. `pub_->current` is left null. The code is unchanged since before e0508e9 (`git diff e0508e9..6ab3983b` touches neither `publish_view` nor `hazard.hh`), so it is not introduced by R39; R39 routes this failure through `durable()`, which fences the writer, and fencing neither stops readers nor repairs the pointers. It is reachable from `commit()` (`index.hh:961-962`) and install (`:1693-1696`) with any live reader, and the commit's own durability is intact (the edit is in the log). | Release probe 261110-103338-1f8973: for each scenario with a reader, exactly one allocation point (n=34 rollover, n=139 segment edit, n=17 metadata-only; 6 of 6 seeds) leaves the held view reading wrong ("the held view reads wrong"). ASan job 261110-103339-84a301: `heap-use-after-free`, read at `review_alloc_crash_probe.cc:102` (the held view's `get`), freed by `publish_view` at `index.hh:3036` via `publish_point` (`:2944`) inside `commit()`'s `durable` block (`:961`), allocated by `build_view` at an earlier `reader()` (`:2949`). `apply_atomic`'s sweep never reads through a reader after the failure, so it cannot see this. | Make `publish_view` unable to throw after the pointer is published and before ownership settles: reserve the retired list's room before `reader_registry::publish` (or retire first, then publish), so `retire` cannot fail. Add a read through a view held across the commit, and through a fresh `acquire()`, to the commit sweep with a reader (my probe's shape), and keep `alloc_crash_probe` as the rerun: it must report 0 BAD under ASan at the fixed commit. If the lead rules this outside A5's scope (pre-existing, memory safety rather than durability), it is a hunt row and the verdict on the three fixes stands; I would not land with a known reader UAF under allocation failure. |
| N1 | no | gap | The undo's own truncate failing after a failed sync leaves the edit in the log. When the pages were kept clean (Linux) and the edit spans pages, a same-boot reopen reports the failed edit's generation (head 2, reopened at 3), the next acknowledged edit follows it, and a power loss leaves "manifest log: damaged at offset 206, generation 3" at 16 of 16 seeds (open at 9,000-byte edits; 3,000-byte at some offsets; small edits within the last page survive). The fix as the review proposed it ("a failed undo breaks the appender as today") accepts this; it is a double fault (failed fsync, then failed ftruncate), the writer is fenced, and no writer-side fix exists. | `sync_probe` "+ undo truncate fails" rows, 261110-103338-1f8973. | Name it in D4 beside the process-kill case in the journal's follow-ups. Optional: a fenced writer that cannot truncate could refuse to `close()` quietly and say the log needs `open(..., to)`. |
| N2 | no | gap | R39's claim is held for `commit()` only through its first three steps. Mutant M1 (commit's `publish_point`, `free_views`, `retain`, `reclaim_files` outside `durable()`) is killed by `apply_atomic_test`; M2 (install's three steps outside `durable()`) and M3 (`reclaim_files` alone outside it) survive, since no test fails an allocation inside install's tail or with a non-empty reclaim queue. The code is correct by reading (`index.hh:961-966`, `:1693-1697`). | 261110-104118-99fd52: `killed: M1`, `SURVIVED: M2`, `SURVIVED: M3`, `killed: M4` (undo without its sync: `manifest_log_test`). | An allocation sweep over install, and a commit sweep with queued deletes, each with the mutant that fails it. |
| N3 | no | gap | The open's new directory sync is unmeasured. D6.6 still quotes "0.3 to 2 us" for the log's sync alone; one more `fsync(dir)` per writer open on a many-indexes host is a different cost (journal R35 leaves it to the lead's open-cost row). | `index.hh:5246`; journal R35. | Measure in the open-cost row and amend D6.6's sentence. |
| N4 | no | note | 261110-092610-08117d (A9 at 6ab3983b) exited 8 and 261110-102344-5abe4a is queued; the criterion is outside this lens. | `campaign job list --quest manifest-log`. | none |

### Risk by risk

1. F1 holds for kill, EIO from `replace`, EIO from the directory sync and an allocation failing at
   `files_[name]`: all leave the same on-disk state, and the open now orders the directory before the log.
2. F2: byte-identical to before the edit on every single-fault sync failure (before or after its effect,
   pages dropped or kept, undo's sync failing too). When the truncate itself fails the appender breaks, the
   original error stands, the writer fences, and the log keeps the edit (N1).
3. R39: after the log moves, `commit()` and install now run everything under `durable()`; in `commit_files`
   the head's replay is fenced, `roll_over`'s post-block statements are non-throwing, and `append_edit`'s
   post-barrier section is inside its own fence, so no step is left outside. Fencing there cannot double-free
   or leak: the sweep (153 points, ASan+UBSan) and my probe (all counts of fenced-and-durable points) found
   no leak report and no double free. The one memory error is F3, in the step being fenced, and predates it.
4. An acknowledged commit means the same to a reader: `commit()` returns only after the edit is durable and
   the view published. A commit that throws and is durable now always leaves the writer fenced (0 violations
   at every allocation point), where before a retention or view failure could throw from an unfenced writer on
   a durable edit; a reopen shows the new generation. Retention running out of memory now costs a reopen
   (journal R39 states this).

### Not run

- No real-filesystem or dm-flakey crash test; Linux page-cache behavior in F2 is modeled by faultfs's `keep`.
- No macOS or Linux CI matrix result at 6ab3983b (CI's sanitizer job is not at this head); my ASan run is
  macOS x86_64, the probes' Release run the same.
- The security review's findings and the A9 rerun are outside this re-check.

Tokens: ~160k

## Re-check 2

Commit: strata:1ebeff876de93af2442045d89757f91076117520
Verdict: APPROVE
Date: 2026-11-10

Scope: `git show f39abc3e` (F3's fix) and `git show 1ebeff87` (the N2 sweep) against F3. No new probe job;
I used the jobs below and read the code at the head.

### Evidence used

- 261110-114551-711f81 (ASan+UBSan, assertions on: the script overrides `CMAKE_CXX_FLAGS_RELWITHDEBINFO`, so
  no `NDEBUG`), `apply_atomic_test` and `hazard_test` at 1ebeff87, exit 0. The ten sweeps print: commit 153
  points, commit with a held view 161, reclaiming commit 150, install 98, install with a held view 109, each
  at burst 0 and 1, no report; hazard: ok.
- 261110-114602-96a9cd (ASan) and 261110-114604-0667d8 (Release) run at 3b672c41. `git diff 1ebeff87
  3b672c41` is exactly the fix reverted (`reserve()`, the `noexcept`/assert, the two `reserve()` calls) and
  no test file, so they are the same test against the old code. ASan: `heap-use-after-free` read in
  `view_state` (`apply_atomic_test.cc:204`), freed by `publish_view`, from `durable_sweep`
  (`apply_atomic_test.cc:326`), exit 134. Release: segmentation fault, exit 139.

### F3: closed

`basic_retired::reserve()` (`hazard.hh:379-381`) grows `list_` before the publish and `retire()` is
`noexcept` on that room (`hazard.hh:384-387`). `publish_view` reserves first (`index.hh:3028`) and retires
after the stores (`:3038`); `retire_current` reserves first (`:3046`).

1. Every caller. `retire(` has two call sites in the library (`index.hh:3038`, `:3048`) and `reserve()` two
   (`:3028`, `:3046`), each ahead of its own publish with nothing but atomic stores, the test-only
   `rs.probe` and a move in between. `publish_view` has two callers, `publish_point` (`:2944`) and
   `publish_cover` (`:2993`); `publish_point` is reached from `commit()` (`:962`), install (`:1694`),
   `refresh` (`:881`), the merge installs (`:1077`, `:1102`), `:2911` and `:3422`; `retire_current` from
   `close()` (`:665`) and `~basic_index` (`:631`). All go through the two reserved functions, so install
   and the other paths are covered. In the destructor a throwing `reserve()` terminates, as the old
   `push_back` did; in `close()` it throws before anything is published, so readers are untouched.
   `hazard_test`'s three owner loops reserve before their publish.
2. Concurrency. `list_` has one owner: `retire`, `reclaim` (including its `erase_if`), `for_each`, `leak`,
   `size` and `empty` are all called from the writer under its lease (`writer_scope` in `try_finish_close`,
   `commit`, `close`, the destructor; `free_views`, `retire_current`, `publish_view` under the callers
   above). Readers touch the registry's records, never `list_`; `reclaim` reads the registry, not the
   reverse. `reserve()` is called by the same owner as `retire()`; it moves the elements (a noexcept move
   of `unique_ptr`) before adopting the new buffer, so a failed `reserve` leaves the list as it was, and
   `reclaim`/`leak` keep the capacity. Nothing a scan could read mid-way. `retire()` with no room, in a
   build without assertions, terminates (noexcept) instead of freeing a held pointer.
3. The sweep. `durable_attempt` reads the held view after every step, whether or not it threw
   (`apply_atomic_test.cc:266-270`, `view_state(*held) != seen` is `wrong`), then a fresh `acquire()`.
   With the fix reverted the same test fails in ASan and Release (jobs above), and the held case adds
   points (161 against 153, 109 against 98), so the reader changes what is allocated. The failure shows
   at the held read, so it is a real gate.

### N2: M2 closed, M3 not

The new install cases kill M2 (install's post-log steps outside `durable()`): reviewer's `mutants.py` at
1ebeff87, job 261110-114559-823541 (`killed: M1`, `killed: M2`, `SURVIVED: M3`, `killed: M4`; 3 of 4), and
the contributor's M2 mutant, job 261110-114607-d6584b (failed, as it must). M3 (`reclaim_files` alone
outside `durable()` in `commit()`) still survives the "commit reclaiming a merge's sources" case: the same
job, and the contributor's 261110-114609-102757 at 4bb2e08 (the diff is that move alone) exit 0. So the
sweep does not yet fail an allocation inside `reclaim_files` after a durable commit; either the queue is
empty at that point or the allocation is absorbed. The code is correct by reading (`index.hh:961-966`).

### Findings

| ID | Blocking | Kind | Finding | Evidence | Status / fix |
|---|---|---|---|---|---|
| F3 | no | defect | Closed: the room is reserved before the publish in both functions that publish then retire (`index.hh:3028`, `:3046`), `retire` is `noexcept` (`hazard.hh:384`), no other call site exists, and the retired list has a single owner. The held-view sweep reads the held view and a fresh acquire after every failure (`apply_atomic_test.cc:266-276`) and fails with the fix reverted. | 261110-114551-711f81 (pass); 261110-114602-96a9cd (ASan, 134, use-after-free at `view_state`), 261110-114604-0667d8 (Release, segfault) at 3b672c41 = the fix reverted | none |
| N2 | no | gap | Half closed: M2 killed, M3 (`reclaim_files` outside `durable()` in `commit()`) survives the reclaiming-commit case, so the commit message's "or reclaim_files alone" is not yet true. | 261110-114559-823541 (`SURVIVED: M3`); 261110-114609-102757 exit 0 at 4bb2e08 | Make that case's reclaim queue non-empty at the commit's `reclaim_files` and fail an allocation in it, or note the queue as untested. Not blocking: correct by reading. |
| N5 | no | note | `retire()`'s precondition is an `assert`, so in a build without assertions a caller that forgets `reserve()` terminates on allocation failure instead of misbehaving: safe, and the ASan job runs with assertions on. | `hazard.hh:384-387` | none |

### Not run

- No new probe; no `alloc_crash_probe` rerun at 1ebeff87 (the sweep and the mutant pair cover F3; my probe's
  scenario with a reader is the same shape). No TSan or Linux run at this head.
