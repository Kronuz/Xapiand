# sync-at-first-commit review: durability (design)

Commit: strata:633e39b07a80449e20dfd0c9082d68d0791bf36d
Verdict: CHANGES
Date: 2026-11-12

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

None for A2-A4: no job exists yet (design stage). A1 is the note itself, read against
`include/strata/core/index.hh` (open `:5189-5277`, commit `:4877-4997`, `retain` `:5024-5068`,
`reclaim`/`drain` `:974-990`, `:3729-3866`, chores `:2905-2916`, merge publication `:3421-3428`,
close `:649-670`), `include/strata/core/faultfs.hh` (crash `:213-287`, stage `:518-535`,
ordering `:659-673`), completion `e49f288f` `io.h:237-249` (the IO contract), T4 cases 9-11
(`test/index_test.cc:102-222`), PLAN D60c and the quest journal's rulings.

## What I checked myself

- Probe job 261112-081821-99fd49 (`.scratch/review-sync-at-first-commit/run.sh`, builds
  `staged_model.cc` against this commit's faultfs.hh): durable A, unsynced B, ordering point,
  write C, crash over 512 seeds. C survives without B in 28 of 512 seeds: the model gap the note
  names (ruling 3) is real, and it would fail the correct design's own A3 test. With a second
  ordering point (stage, C, stage, D) the same 28 of 512: see F3.
- Q1 by reading. Append while owed: the ordering point stages the inherited log (faultfs
  `stage()` takes `flushed()`, which holds g) and the directory, so g is durable (Linux) or
  barrier-ordered (macOS) before g+1's `pwrite`; one record in flight, as manifest-log D4
  assumes. Necessary: without the log in it, g and g+1 share one `sync_data` and a torn g under a
  whole g+1 reads as damage; without the directory, a no-file commit never orders case 10's
  rename and an acknowledged edit to N can revert to M (T4 case 10's shape). Rollover: its
  ordering point syncs the directory before its rename and its snapshot holds the whole state,
  so the inherited tail stops mattering; a crash before the rename leaves N with at most a torn
  tail, read as a rollover owed, never damage. Open at `to` that falls back: an explicit forced
  rollover (`:5251-5257`) whose `sync_dir` makes K and the inherited renames durable; nothing is
  owed after it, and retention judged against K is safe. Sufficient for every crash point under
  power loss; the one EIO hole is F1.
- Q2 by reading: every removal of a file a log named goes through `retain()`'s queue (callers:
  commit `:964`, `reclaim()` `:977`, `drain()` `:3853`, chores `:2913`, merge publication
  `:3427`, install `:1696`), so a gate inside `retain()` covers reclaim, merge, sweep and drain.
  The only other unlinks are the open's `.tmp` orphans (never named by a head) and a handle's own
  partial outputs (`:2863`, `:3360`, `:4565`, `:4670`). Close runs no retain.
- Q3 by reading: `close()` (`:649-670`) writes nothing; a never-committing handle leaves the
  inherited bytes and names as found, minus `.tmp` orphans (unsynced unlinks), plus any of its
  own flushed or merged segments (orphans with ids at or above the old mark, which the next
  open's mark covers). The next open sees the same inherited state and handles it the same way.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | D6.2 claims an inherited writeback error fences the first commit, but two directory syncs that can run before it swallow their error: `drain()`'s `sync_dir` returns false on any system_error (`index.hh:3858-3861`), and it runs after an open that unlinked a `.tmp` orphan (`unlinked_` > 0, which D4's last paragraph keeps on purpose); a parallel flush's ENOSPC cleanup runs `try { sync_dir(); } catch (...) {}` (`:4670`). On Linux a directory fsync reports a writeback error once (errseq), so the swallowed EIO is the inherited rename's, the first commit's `fsync(dir)` then succeeds, the edit to N is acknowledged, and a power loss reverts N to M: an acknowledged commit lost. Before this design the open's sync took that error and failed the open; this path is introduced. | `index.hh:3851-3866`, `:4664-4671`, `:5274`; note D4 last paragraph | While owed, a directory sync outside the commit is the inherited one: `sync_files_and_directory({log})`, fencing on failure and clearing owed (or the best-effort syncs skip while owed and leave `unlinked_` counted). Test: open over a `.tmp` orphan, `reclaim_for_space()` with a `sync_directory` EIO fault, then `commit()` throws `index_fenced`. |
| F2 | no | gap | A3's mutant "inherited sync dropped" in case 9 with a no-file commit fails only when the crash tears g under a whole g+1: about 1 seed in 16 at the one crash point between the edit's write and sync (the probe's rate, 28/512), and never if g and g+1 share a 512-byte sector. Case 10 catches it at 1 in 2. | probe 261112-081821-99fd49 | Name the seed count (128 or more per point gives a miss below 0.03%) or enumerate the sector outcomes, and make g span a sector boundary (case 11's 9000-byte metadata does). |
| F3 | no | gap | The corrected faultfs model keeps one ordering mark: a second ordering point re-stages a node over the first epoch (`stage()` `:532`, `order_mark_` `:671`), so a chain (process crash after a first commit's edit write, reopen, commit, power loss) can show g+1 without g, which a real barrier or Linux fsync forbids. The note's chain paragraph claims the chain; A3's plan does not test one, so nothing fails today, but a chain test would false-fail. | probe 261112-081821-99fd49, two-epoch line | Either say A3 covers no chain and the model holds one epoch, or let a re-stage settle the earlier epoch's staged bytes as ordered ahead (one epoch list). |
| F4 | no | gap | Ruling 2's EIO fence at the removal gate has no test in the Evidence section, and faultfs cannot model a writeback error the crashed process left unseen. | note "Evidence", A3 | Add: a `sync` fault at the gate's sync during `reclaim()` before any commit fences the handle and queues nothing. |
| F5 | no | nit | D4 does not say whether its sync clears the owed state (it can: the log and directory are durable after it), nor where the gate gets the log after a demotion closed `log_` (`reopen_log()` `:5000` returns early only for the appender). | note D4; `index.hh:4999-5004` | One sentence each. |
| F6 | no | gap | A2's "and across reads: zero" holds only with no inherited garbage: a read's view free runs `retain(true)` from the chores (`:2913`), which under D4 pays the sync. A previous writer's unfinished reclaim queue is common, not only after crashes, and Xapiand's housekeeping calls `reclaim()` (`src/engine/strata/shard.cc:1391`). | `index.hh:2905-2916` | A2 states its states hold no garbage and adds one that does, counting exactly one sync at the first removal; A4 reports how often opens in the loaded harness meet garbage. |
| F7 | no | nit | "Xapiand records no open's generation as durable": Xapiand stores `manifest().revision` after a reopen as `committed_revision` (`src/engine/strata/shard.cc:214,255`) and the WAL keys its header and replay range on it (`src/database/wal.cc:83,161`). Whether that treats an unacknowledged revision as durable is xapiand-strata-bump's question; route it there. | as cited | Lead: add to xapiand-strata-bump's risks. |

## Not run

No build, test or timing of the design (nothing implemented). Linux and macOS backends'
`order_files_and_directory` were not read; I took the note's description of them, which matches
what today's commits with a new segment already rely on. The macOS claim that `fsync` plus
`F_BARRIERFSYNC` orders a dead process's dirty pages ahead of the new edit is reasoned, not
measured. GPL sources not read.
