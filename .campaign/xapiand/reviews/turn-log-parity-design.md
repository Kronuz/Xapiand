# turn-log-parity review: design (durability lens), strata docs/design/rocksdb-parity.md

Commit: strata:0d93765c
Verdict: CHANGES
Date: 2026-11-20

The owner's architecture holds (PLAN D148): one log and one flush per device, idle groups leaving
the log, an opt-in unversioned upsert. The note's numbers match their probe logs. What is missing
is the protocol text that the irreversible tasks need. That covers task 10's cross-loop handoff,
task 5's ack=none window with more than one group, and the damage and parking rules for tasks 9
and 10. Two items also need text before they build: the forward copy (task 2/3) and the
checkpoint position under background flush (task 6). Every blocking row is a text fix to the
note plus the test it names. None of them overturns D148.

## Evidence I relied on

- Probe B, job 261120-072003-b88d0c (log and per-cell `wbytes.tsv` under
  `~/.scratch/xapiand/turn-log-parity-design/b-072657/`). The ops/s means in "Group commit" and
  "How many logs" match the log: K=4 cohort 180, K=8 cohort 327, K logs today 72-124, RocksDB
  146/175. So do the ratios 1.23×, 1.87×, 2.5× and 2.6×.
- Probe E, job 261120-074947-257624. The table matches: shared T=8 368.5, per-log 110.7,
  device 252.9 (31% under shared), and without the cohort wait 187.2 at 4.05 per flush.
- Probe D, job 261120-074846-356a48. The re-homing table matches the log, and so do the
  extrapolations: 300k re-partition 2.063 − 0.776 s = 4.3 µs per group, so 4.3 s at 1M, plus
  2.6 s to recover. That makes about 7 s and 550 MiB, stated in the note.
- Code read at journal `cc95ba6` (strata's pin) and strata `d956c0f4` (index.hh extracted to the
  scratch directory below).

## What I checked myself

- `.scratch/xapiand/review-turn-log-parity-design/d7_model.py`, run under
  `quiet light review-d7` (`python3 -I d7_model.py`). It models journal's `seal_turn`
  (turn_driver.h:451-508) with D7's repeated sealer calls. Results:
  1. Window. With N groups, each bounded at one turn's room and all saturating, a record waits
     unsealed for up to N−1 turns: 0, 1, 3, 7 and 31 turns at N = 1, 2, 4, 8 and 32.
  2. Fairness. A group bounded at 2× the room starves another group completely under the literal
     resume rule (turn_driver.h:492, `resume_at = group`): 0 of 2,000 records sealed. With the
     bound at or below the room, rotation is fair, with a wait of at most 1 turn.
- Recovery and re-partition by reading code: turn_log.h:428-610 (recover), host.h:420-525 (open,
  mark_lost, repartition), turn_driver.h (threading contract), turn_seal.hh (standalone loop),
  index.hh (commit, checkpoint, freeze, read masks).

## Answers to the lead's questions

1. **One log per device.**
   - **Ordering and the barrier.** Sound as far as the note goes. A turn is still one write plus
     one barrier. Records from every loop ride it, and each group's sequence is checked at
     `TurnLog::stage`.
   - **Torn tail.** Unchanged. The newest epoch's incomplete last turn is dropped whole
     (turn_log.h:530), whichever loops its records came from.
   - **Partitioning at recovery.** By group id, which works because placement is a pure function
     of the id (host.h:81-87).
   - **One damaged block.** The loss set widens (F2), and parked groups would all be lost (F3).
   - **Single writer.** The note does not specify the handoff that would preserve it (F1).
2. **A changed loop count.**
   - **Crash safety.** Today's protocol is safe. Generation-tagged logs and the layout replace
     are the single commit point, and `mark_lost` precedes it and is idempotent. A crash at any
     step either redoes the work or removes leftovers (host.h:430-446, 481-520). Nothing
     acknowledged is lost or duplicated.
   - **With D6.** No re-partition happens on a loop-count change.
   - **Cost.** Stated (about 7 s and 550 MiB at 1M idle groups) but extrapolated past the
     largest measured 300k, and bounded only linearly. Parking is the bound, after A6 (N2).
3. **The cohort wait.**
   - **Starvation.** None: a turn seals every open record that fits, and rotation carries the
     rest.
   - **Latency.** Each turn adds at most 1/16 of the last barrier. If "last barrier" includes
     the wait, the turn converges to 16/15 of the barrier, so latency stays bounded.
   - Non-blocking N3 and N4 below.
4. **The packed turn and the checkpoint.**
   - **D7.** It weakens the ack=none window for more than one group per log (F4, modelled). It
     also delays ack=sync records behind other groups' backlog by the same number of turns.
   - **D2 item 2.** By itself it changes only a trigger. `commit()` still waits for the log
     through the applied position, then flushes, then makes the edit durable, then stages the
     checkpoint (index.hh:1256-1273). Neither ack level weakens. It does weaken once D4 makes
     the flush background (F6).
5. **The unversioned upsert.**
   - **Point reads and key scans** see the new copy, because the newest source wins
     (index.hh:2486-2490).
   - **Searches** see the old copy until the flush (F7).
   - **Recovery** is fine. The log replays the key-only upsert, and resolution commits with the
     flush's segment in one `commit_files`, so nothing is lost or resurrected.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | gap | D6 names no handoff protocol. The note says only "a loop hands its sealed records to the owner's open turn", but every TurnDriver mutation is loop-confined: `seal` (On the loop), `stage` (pushes `pending_` unlocked), `link`/`unlink` and `forget`; Host table updates run "on the loop's thread". Every loop appending therefore needs either marshaling or a lock, and the note picks neither. Also unspecified: completion and failure fan-out (one failed barrier fences every index on the device), cohort counting across loops, and which mutex strata's standalone loop collapses to (`turn_mu_`, one per loop today). The probes measure D6's shape only as every index on one loop (probe B L=1) and as a C floor behind one mutex (`devbarrier.c` `group_flush`). The cross-loop handoff, one wake per loop per turn, is unmeasured, and on an NVMe flush of tens of µs it is the same order as the barrier. | turn_driver.h:164, :222-234, :403-422, :139-153; host.h:199; turn_seal.hh:325; probe-b.sh (L=1); devbarrier.c:56-95 | Before task 10, specify the handoff in a D6 subsection: per-loop SPSC queues drained by the owner at `seal_turn` (or `post_call` marshaling), waiters resumed on their home loop (`Wait` already posts to `waiter.home`), fail-all fan-out, and the cohort count. State that apply stays loop-local, with no lock on the write path. Task 10's Linux floor includes the handoff. Test: K loops on one device log under TSan, with a power cut at every operation, every acknowledged record recovered. |
| F2 | yes | gap | One damaged block with no readable manifest costs "every group that wrote nothing after it", and on damage every open group without records is lost. Per loop, that set is one loop's groups; with one log per device it becomes every group on the device, L× wider. The note does not say so, and A2's mechanism must hold on the shared log. | turn_log.h:473-474, :604 | State the wider blast radius in D6 (and in Q1's trade-off). A2's tolerant-open test runs on one log holding groups from at least 2 loops: a damaged unmanifested block opens every covered group and fails only uncovered ones. |
| F3 | yes | defect | Parking (D10 item 3) collides with recovery's damage rule. A parked group stays `open` in the table and has no log records, so any damage anywhere in the log marks it lost (`if (damaged) for known.open ... lost`). With one log per device and millions of parked indexes, one bad block fails every idle index on the device. The other half is the reverse case: a group that writes after parking has records only in the log, and the table cannot tell "wrote nothing since parking" from "its post-park record sat in the damaged block". | turn_log.h:604; host.h:405; note lines 423-427 | Task 9's journal record of item 3 defines a durable `parked` table state. Park works as `close_group` does: the table write is durable before `forget`. Unpark is a table transition in or before the turn carrying the group's first record (D10 item 5 makes it ride that turn). Recovery exempts parked groups from the damage rule. Test: damage an unmanifested block; parked groups open, and an unparked group whose only post-park record was in that block reports lost. |
| F4 | yes | defect | D7 bounds each group's unsealed bytes at one turn's room, but the room is per turn, shared by every group. With N groups writing ack=none, the oldest unsealed record waits N−1 turns, so the crash-removable window is about N barriers, not "one barrier plus one turn" (D107). Under D6, N counts every group on the device. An ack=sync record behind them waits the same turns. | d7_model.py window rows (N=8: 7 turns; N=32: 31); turn_driver.h:454-478; note line 291-297 | Bound the unsealed bytes per log, not per group: one turn's room shared across groups, with the group's share or a log-wide admission counter (under D6, one atomic per device: say so in F1's protocol). Otherwise restate the window as ⌈Σ unsealed / room⌉ turns and take it to the owner. Test: 8 groups saturating ack=none; every record seals within 2 turns of its join. |
| F5 | yes | gap | The forward copy's removal is ambiguous. D3 says "forward copies go (D2)", but `reclaim()` frees only segments with no live records, and an idle group's checkpoint stays live until it closes. Without the forward copy, and before parking (deferred past A6), any idle group pins its segment forever. The log then grows until ENOSPC fails it and every index on it fences. | turn_log.h:321, :352-354; note lines 167-168, 132-133 | State that the forward copy stays as the fallback for records outside a checkpoint's reach until parking lands. D2's "nothing is copied" then means for busy groups only. Test (tasks 2/3): one idle group's checkpoint in the oldest segment, 10× retain turns of another group; segment count stays at or below retain + spares. |
| F6 | yes | gap | D4 does not say what position the checkpoint covers. Under background flush, `stage_checkpoint` reads `cur_.applied` when it stages, and by then the writer has moved past the freeze into the new active buffer. A checkpoint naming that position would let the log drop acknowledged records held only in the active buffer. | index.hh:3503, :1256-1273; note lines 173-181 | D4 states that the checkpoint names the frozen buffer's applied position, captured at `freeze()`, and is staged after that segment's `commit_files` is durable. Test (task 6): an ack=sync write lands in the active buffer during a background build, power is cut after the install, and the write recovers. |
| F7 | yes | gap | D11 claims "reads already see the buffer first", which is true for key lookups and key scans only. Searches read `segments()`, which masks superseded segment copies only through the buffer's pending (segment, ordinal) deletes, and those are the per-write lookup D11 removes. Between the write and the flush, a search matches the old version (and counts it), alongside the new one. | index.hh:1495-1515, :2471-2476, :3114 | This defines user-visible semantics, so it is the owner's call. Options: (a) resolve the buffer's unresolved keys once per view at `segments()` (one sorted pass per view, not per write); (b) document that searches may see the old copy until the next flush or refresh; (c) check each segment hit's key against the buffer. Recommendation: (a). The test asserts that a search sees exactly one copy after an unversioned upsert. |
| N1 | no | nit | Probe table names D's job as 261120-072006-46c758, which was cancelled (exit 143); the numbers are from 261120-074846-356a48. | `campaign job show` | Fix the row. |
| N2 | no | gap | Re-partition cost is linear in groups, and its map (gap 1) is bounded by all logs' live bytes: up to L × retain × segment = 8 GiB at 32 loops of 64 MiB, so an open can OOM. The note gives only the idle-group cost. D6 removes it on a loop-count change; it remains if Q1 is not built. | host.h:482-494 | State the live-bytes worst case beside the 1M row. |
| N3 | no | gap | D5 counts arrivals by `wake()`, which fires only when a record opens. Writers sharing one group never reach n, so the loop always waits the full bound. That is bounded, but it is a 6% tax at K writers per index. | index.hh:3541; cohort.py | Count joins (writers), not opened records; add a K-writers-one-index case to task 1's test. |
| N4 | no | nit | "Last barrier" is undefined. If it includes the cohort wait, the bound feeds back (converging at 16/15 of the barrier). | note line 209 | Measure write plus barrier only. |
| N5 | no | gap | Fairness under D7 depends on each group's bound staying at or below the turn's room. Above it, the literal resume rule starves later groups (model: 0 of 2,000 sealed). | d7_model.py fairness rows; turn_driver.h:492 | Resume after a group that a repeated call drained, or keep the bound at or below the room. Test with one saturating group and one sync group. |
| N6 | no | nit | probe-b.sh prints 0 F_FULLFSYNC for every row, because the phase marks never reach wbytes (all rows are phase `start`). The note's syncs per op come from whole-process totals (466/1024 = 0.46; 394/1536 = 0.26), open-time syncs included. | b-072657/*.wbytes.tsv | Fix the phase filter or say "whole process". |
| N7 | no | nit | "Device" is not defined. Two Host directories on one device still split flushes. | note line 257 | Say one log per Host directory, one Host per device. |

## Not run

- No build or test: tasks 1-9 have no landed code at `b112db88`, and F1-F7 are design text.
- No probe of D6's cross-loop handoff cost (F1 asks task 10 to measure it on Linux).
- D1 ladder recovery (variable segment sizes vs `scan_segment`'s `min(size, segment_bytes)`):
  read, no issue found, not probed.
- VM and Linux floors (task 12, task 10's precondition).
