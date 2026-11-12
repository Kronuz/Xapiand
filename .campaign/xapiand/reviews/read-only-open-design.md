# read-only-open review: design (durability and concurrency)

Commit: strata:70aba57df5abbbe177928370db22db95b0acb0f6
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

None: `campaign status read-only-open` shows no job for A1-A4 at 70aba57d (the suite at 69b76e9c
ignores the docs-only commit). This is the A1 design ruling, against the committed open path
(`git show 70aba57d:include/strata/core/index.hh`, saved as
`.scratch/review-read-only-open-design/idx70.hh`; line numbers below are that file's). The worktree's
uncommitted implementation was not reviewed.

## What I checked myself

Code reading, no jobs; every finding is settled by `file:line`.

1. Q1 (what a search handle observes): D2 is exact for the single-handle case. Commits order their
   segments before the log edit (`append_edit`, idx70.hh:4972 `order_files_and_directory` before
   `appender_->write`), so a head left in the page cache names segments whose bytes the same page
   cache holds; the only non-durable parts are the edit (case 9) and the rollover rename (case 10),
   as D2 says. The host rule (no recorded generation from a search handle) is the right boundary.
2. Q2 (upgrade order): lock, then names-and-sizes check, then the one
   `sync_files_and_directory({log})`, then appender, then orphans. Nothing on disk changes before
   step 3, step 3 only syncs, step 5 only unlinks `.tmp` orphans; no commit of the handle returns
   before step 3. Against a crash with no other writer, I find no point that loses an acknowledged
   commit or leaves the index unreadable. D5's table is right.
3. Q3 (concurrency): same-handle readers are fine (the upgrade publishes nothing). Two upgrading
   handles serialize on `flock` per open file description (completion `posix.h`
   `acquire_directory_owner`), so the second is busy, then stale. The cross-handle writer case is
   where the design is wrong: F1, F2.
4. Q4 (tests): faultfs counts syncs among `mutations()` (`faultfs.hh:189`), so A2's zero-mutation
   check covers fsync and F_FULLFSYNC; `power_off_after` plus `crash(seed)` gives A3 every crash
   point; the mutants name what each step guards. faultfs serves maps from memory, so no faultfs
   test can show F1's truncation hazard.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | D6's account of a writer on another handle is false, and D60 newly admits that case: today a second open fails busy; a search open now succeeds beside a writer in another process (or another handle in this one). Three outcomes D6 misses. 1. **At open:** the scan records sizes before the log read, so a commit landing between them gives a log naming a segment the scan missed, or one the scan caught half-written: `load_record` throws `corrupt_index("missing file" / "file size differs")`, and `read_checked`'s retry reuses the same stale map. A false corruption report. 2. **Lazy map after open:** ENOENT is not "the host's error": `open_body_of` turns it into `corrupt_index("missing file")`, `map_body` catches that and marks the record failed for good (`corrupt_segment`). A reclaim's truncated-but-not-yet-unlinked file gives "file size differs". 3. **Mapped segment:** reclaim truncates every file over `reclaim_step` (16 MiB) in steps before unlinking, and "a file a reader maps is never truncated" holds only for the reclaiming handle's own views. Another handle's or process's mapping of that file takes SIGBUS on the next read: a crash, not ENOENT. | idx70.hh:5226-5230 (log read after scan; lazy `load_files` against `files_`), :5372-5373 (`load_record`), :5390-5393 (`open_body_of` ENOENT to `corrupt_index`), :2554-2563 (`map_body` marks failed), :3740-3748 and :3618-3624 (reclaim and posted delete truncate before unlink); design D6 | Owner's call (D60 says "no lock"); options: **(a)** (recommended) a search handle holds a *shared* `flock`. That is one syscall and no flush, which leaves D60's motivation intact. Writers stay busy while any search handle is open, exactly as today's exclusive open makes them, and the upgrade converts to exclusive non-blocking. A conversion that is not atomic on BSD/macOS is covered by D4 step 2. F2 also goes away. **(b)** Keep no lock and state a host contract in D6: no other handle in any process writes the directory while a search handle is open. Name the consequences (false `corrupt_index`, SIGBUS) and say that tools must open plain. **(c)** Support it: rescan-and-retry at open, a host error instead of the failed marker for a missing or short file on a search handle, and no truncating reclaim while unknown mappers exist (not knowable across processes, so (c) fails part 3). Test for (a): a second process's (or handle's) plain open of a directory with a search handle open fails busy, and a search open beside a plain writer fails busy; for (b): the contract in D6 and Xapiand's catalog check. |
| F2 | yes (moot under F1 (a) or (b)) | defect | D4 step 2 rests on "a log only grows or rolls to a new name". A failed append truncates back (`append_edit`: "a failed write is truncated back first"). Suppose the scan caught k bytes of a write that then failed: the handle replays a dirty tail and owes a rollover. If the next commit by another handle appends exactly k bytes and that writer closes, the upgrade's names and sizes match. The handle then rolls over its own older state as the head, and the other writer's acknowledged commit is lost. Rare (a failed partial write inside the scan window plus an equal-length edit), but it breaks A3's guarantee if concurrent writers stay supported. | idx70.hh:4968-4984 (truncate-back), :5266 (`rollover_owed_` from size mismatch); design D4.2 | If F1 resolves as (c): also keep the log's bytes from the replayed end to the scanned size (at most one record) and compare them at step 2, or fence whenever the scanned log size differed from the replayed end and the log changed at all. Test: a faultfs write failure mid-append on handle B during A's search open, then B commits an equal-length edit and closes; A's upgrade must fence. |
| N1 | no | gap | A search handle's storage permit includes the commit allowance (`settle_storage` at open, a rollover snapshot when `rollover_owed_`), so millions of search handles under `storage` reserve bytes only an upgrade needs. | idx70.hh:3829, recover's `settle_storage()` | Hold no allowance until the upgrade, or state the reservation in D7. |
| N2 | no | gap | D3 does not say what `apply()` does when the flush it triggers runs an upgrade that is refused (busy or stale): is the record rejected or kept buffered? | design D3 | One sentence in D3, and the busy-refusal test applies through `apply()`. |
| N3 | no | nit | A2's lock evidence ("acquires the directory lock while the handle is open") shows that no lock is held, not that no lock call was made. | design Evidence | Also assert that faultfs's `acquire_directory_owner` is not called (a counter, or `calls()` across the open minus the expected reads). |

## Not run

No jobs: no probe was needed, since each finding is settled by the committed code above. I did not
review the worktree's uncommitted implementation, Linux sync timing, the A4 harness wiring for
`for_search` in bench/catalog_tiers, or the Xapiand-side host rule (left open by the note).
