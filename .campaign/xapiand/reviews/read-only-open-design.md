# read-only-open review: design (durability and concurrency)

Commit: strata:6f83a4fffde88ff6a3ab7fc3a8b859261a0df69d
Verdict: CHANGES (re-check at 6f83a4ff: R1 and R2 block; the first round's findings were at strata:70aba57d)
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

## Re-check (D60a)

Commit: strata:6f83a4fffde88ff6a3ab7fc3a8b859261a0df69d. Read: `docs/design/read-only-open.md` (D1,
D4, D6), `index.hh`'s open, `own()`, `shared_again`, `retain()`, `reclaim_files()` (saved as
`.scratch/review-read-only-open-design/idx6f.hh`; line numbers below are that file's), faultfs's
shared lock, `test/index_test.cc` (saved as `index_test6f.cc` beside it).

### Evidence I relied on

The contributor's mutant jobs at this code: M6 261112-051635-82a433 done (fails at
index_test.cc:459), M3 261112-051625-7b4853 **failed** (index_test passes under M3: "index: ok").
M1, M2, M4, M5, N1, N2 done at their lines. The suite job 261112-051608-dce758 was still running.
My one probe, 261112-053202-71b02e (`.scratch/review-read-only-open-design/m3_probe.sh`): M3 plus
the stale test's fence messages, from fixture states 2 and 0.

### Q1: F1 and F2 under D60a

Closed on every path where the handle holds the shared lock. The search open takes the shared lock
before the scan (`lock_directory(opts, search_only_)` at idx6f.hh:609, ahead of `recover`), and
faultfs refuses the exclusive lock to anyone while readers hold it (`acquire_directory_owner`), so
no commit lands between scan and log read, and no reclaim removes or truncates a file the handle
maps. F2 needs a writer's append in flight during the open's scan, which the shared lock excludes;
in the upgrade's window the check sees every writer change (a commit grows the head log or adds a
new name that retention never removes, a failed append returns to the size it found). The check is
sound. One path stays open: a handle with no lock (R1). A lock the filesystem does not support
runs the search handle unlocked under `nonlocal`'s warn, exactly as the plain open runs (existing
behavior; N5).

### Q2: the lockless loser

Yes, it is F1 again, on three paths, not one: `shared_again` (idx6f.hh:5320-5326) releases what is
held and swallows a refused shared lock after a refused exclusive lock (:5330), after a failed scan
(:5340, releasing the exclusive lock first), and after the fence (:5344, likewise). A plain open
that lands between the release and the retake leaves the handle with no lock, and its reads go on
(D4.1; the stale test reads `get("Qa")` after the fence). The writer's commit then retains and
reclaims (`commit()` runs `retain(); reclaim_files(reclaim_slice)`, :981-982): a lazy map of a
removed segment marks it failed for good, and a truncating reclaim (files over `reclaim_step`) of a
segment the handle maps raises SIGBUS. The design's mitigation cannot work as written: D6 says "the
host reopens on the busy error rather than reading on", but a loser holding the shared lock gets
the same busy error and D4.1 tells it to read on; the host cannot tell the two apart. D6's "on one
host this needs a second process" is also not so: CT-R15's two catalog keys on one directory are
two search handles in one process, and D6 itself moves that collision to "the first write on either
key", which is exactly two upgrades at once.

The candidates:

1. **Upgrade-intent lock (recommended).** A second lock file in the directory, taken exclusive and
   non-blocking by every exclusive acquirer (the plain open and the upgrade) before the directory
   lock, and released once the directory lock is settled. While it is held nobody can take the
   directory exclusive, so the upgrade's drop-shared/take-exclusive loses only to shared holders,
   and the shared retake on every one of the three paths cannot lose. A search open does not take
   it. It closes R1 entirely. Cost: one more open and flock pair per plain open and per upgrade
   (D7's job measures the flock pair at 1.7 µs with its release); a completion call for a named
   lock file beside `acquire_directory_reader()` (still one directory lock per IO) and faultfs's
   model of it; one extra empty file per index directory, created by `create` or by the first
   exclusive acquirer of an older directory (a mutation, never by a search open). That last one is
   the real price under the many-indexes workload: one inode per index. `parse_name` already keeps
   the name out of `own()`'s check. Its tests: two search handles upgrading in the interleaving
   (A drops, B drops, A wins) leave B holding the shared lock, and a plain open in the window fails
   busy.
2. **Unusable on loss.** When the shared retake fails, mark the handle lockless: `reader()` and lazy
   maps throw, and the host reopens. Cheap (a few lines), but it narrows and does not close: views
   already held keep their mappings, and a truncating reclaim of a segment a merge replaced raises
   SIGBUS in them; and the reopen fails busy until the writer closes. Acceptable only if the owner
   accepts that residual, stated in D6.
3. A blocking shared retake waits for the writer to close, which may be never. Rejected.

### Q3: the reworked tests

M6: killed. Moving the plain-open check after the other search handle closes (index_test.cc:459)
makes it see the loser's own shared lock; job 261112-051635-82a433.

M3: **not killed**, in either fixture state. Without the check the stale handle fences anyway,
inside `guarded()`, on its own first collision with the writer's changes, and every check of the
test passes: from state 2 the writer's commit reclaims the old log (`retained_predecessors` is 0,
so `retain()` queues it and `reclaim_files` removes it), and `sync_head` fails "faultfs open: No
such file or directory"; from state 0 the stale flush creates the segment id the writer already
used, "faultfs create: File exists" (probe 261112-053202-71b02e, both `hold` values). The fence
from `guarded()` leaves the exclusive lock held, which line 508's `acquire_directory_owner` cannot
tell from the shared one. The commit's "without the check the stale handle's rollover would
replace the writer's log" needs the old log kept and no shared segment id; neither holds here. The
check is still load-bearing in production (a writer whose commit writes no segment, or kept
predecessors), so the test must reach that case: R2.

### Findings (re-check)

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| R1 | yes | defect | F1's hazards return for a search handle that loses a lock race: after a refused exclusive lock, a failed scan, or the fence, `shared_again` swallows a refused shared retake and the handle reads on with no lock while the winner commits and reclaims (permanent `corrupt_segment` on a lazy map, SIGBUS on a truncated mapping). Reachable in one process (CT-R15's two keys upgrading at once) as well as across processes. D6's "the host reopens on the busy error" is not actionable: the busy error is the same with or without the shared lock back. | idx6f.hh:5317-5345 (`own()`, `shared_again`), :981-982 (commit reclaims), :3751-3770 (truncating reclaim); design D4.1, D6 | Option 1 above (intent lock), with its two tests. If the owner declines the per-index file: option 2, plus D6 stating that views held at the loss can still take SIGBUS. Owner's call between them. |
| R2 | yes | gap | The stale-upgrade test does not kill M3 (no names-and-sizes check): the mutant fences through `guarded()` on ENOENT (state 2) or a segment-id collision (state 0), and the test accepts any fence. So A3's evidence for D4 step 2 is missing. | jobs 261112-051625-7b4853 (M3 survives), 261112-053202-71b02e (fence messages); index_test.cc:476-512 | Make the writer's commit write no segment (`erase("Qa")` only), keep the old log (`o.retained_predecessors = 1`, state 2, so the stale rollover's rename would replace the writer's log), and after the fence assert `!throws([&] { (void)io.acquire_directory_reader(); })` (the check's fence holds the shared lock again; a fence inside `guarded()` holds the exclusive one). Rerun the M3 job; it must fail at the fence and at the final read of the writer's commit. |
| N4 | no | nit | `own()`'s check opens every file to size it (`open_existing(*n)->size()`), one open per name at each upgrade. | idx6f.hh:5333-5338 | A size from the scan (a stat) where the IO offers one. |
| N5 | no | gap | A filesystem without flock support leaves a search handle unlocked under `nonlocal` warn, as the plain open is today; D6 does not say so. | idx6f.hh:5174-5179 | One clause in D6. |

Prior findings: F1 closed except R1; F2 closed; N1 closed (`settle_storage`, idx6f.hh:3893, mutant
N1 killed); N2 closed (D3's sentence, mutant N2 killed at :449); N3 closed (A2 counts the lock calls).

### Not run

The suite at this commit (its job was still running), Linux's flock behavior, and the A4 timing.
