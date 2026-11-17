# document-gate review: design (durability), criterion A1

Commit: strata:9b958322c572b23a8979075d7c0bd8c5ff50d506 (`docs/design/standalone-turn-log.md`)
Verdict: CHANGES
Date: 2026-11-16

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The shape is right: the turn log as the write-ahead log, apply first, one record per index per
turn, the manifest log as the checkpoint, Track C's torn-tail rule. Six paths in D2 to D6 and its
test plan, as written, lose acknowledged writes, expose unsynced ones, or fail an open on a correct
log; each is a text fix plus a named test, so the design goes back once, not to re-plan. This is the one text-fix round.

## Evidence I relied on

No job at 9b958322 proves A1 (design only; `campaign status document-gate`). I read the design, its
cited sources at their pins (journal `5bbeedc` `include/journal/turn_log.h`, strata `index.hh`,
`faultfs.hh`, `docs/design/track-c.md` D11, `docs/design/manifest-log.md` D9, cpp-libraries
`docs/research/sync-cost.md`), and the quest file's 2026-11-16 decision on Q1/Q2.

## What I checked myself

Scripts under Xapiand `.scratch/review-document-gate-design/` (`run.sh` builds and runs both; rerun
with `campaign job submit --quest document-gate --level light --minutes 10 -- bash
/Users/kronuz/code/Kronuz/Xapiand/.scratch/review-document-gate-design/run.sh`).

- P1 `supersede.cc`, job `261116-203118-487c1b` (the first run, `261116-201125-359287`, had a probe bug: a second
  owner lock): the real `turn_log.h` at journal `5bbeedc`. Group 1 appends batch seq 1 (X), batch seq
  2 (acknowledged at its barrier), then a checkpoint naming X. A checkpoint at seq 1 or 2 is refused
  ("turn record out of its group's sequence"); at seq 3 it is accepted, and recovery returns only
  the checkpoint: the acknowledged seq 2 is gone.
- P2 `fullfsync.c`, same job: 1, 2, 4 and 8 threads, each appending 4 KiB to its own file and
  issuing `F_FULLFSYNC`, 3 s per cell, on this Mac's APFS SSD under a light hold (contended, a shape,
  not a benchmark). Both runs agree; the second (`261116-203118-487c1b`):

  | Threads (logs) | Calls/s, all | p50 ms | p99 ms |
  |---:|---:|---:|---:|
  | 1 | 46.3 | 21.0 | 37.1 |
  | 2 | 47.3 | 42.0 | 58.8 |
  | 4 | 94.0 | 42.0 | 63.9 |
  | 8 | 185.0 | 42.0 | 63.2 |

  Concurrent calls coalesce: from two callers up, each waits for the flush in progress and then
  one shared flush, so latency is 2.0x one caller's and flat to 8, while the rate grows 4.0x.

## Answers to the lead's questions

1. **D2, per tier.** `none` and `ack=none` lose what their rows say and nothing else. On the
   synchronous tiers, apply first is sound for verdicts (D2's three reasons hold against
   `index.hh:82-88,790-793`), and an unacknowledged write can reappear only as the usual indeterminate
   outcome (a failed barrier whose bytes landed; N7). Acknowledged writes can still be lost or made
   unopenable through D5 and D6, not D2: F1, F2, F3.
2. **D5.** I1 and I2 hold at every crash point between flush, edit and checkpoint record, and M-a
   and M-b name the right mutants, if the writer lease is held from step 1 until the checkpoint
   record is staged. The design does not say so, and its own alternative assumes the opposite (F1).
   Held that way, the stall is not one or two barriers: it is those barriers plus the flush plus
   `commit_files()` (about five `F_FULLFSYNC`, 100 ms on this Mac, sync-cost §2), and every ack of
   the index waits behind it because `turn_synced` is a lease chore (`index.hh:918-938`).
3. **D10.** Sound for a group of one under the crash model: faultfs keeps each synced sector, so the
   newest epoch's last turn can be torn only if its barrier never returned, that is, never
   acknowledged. The classifier argument is right too: reclamation recycles segments by rename with
   their old bytes (`turn_log.h:302-305`). The residual, media damage to that one synced turn above
   the head, is stated in Limits; D6.5 bounds it to the writes since the last checkpoint.
4. **D11 on macOS.** Per core is right; Q3's premise is not. N per-core logs do not cost N flushes
   per turn period: P2 shows concurrent `F_FULLFSYNC` coalescing, 4.0x the flushes served at 8
   logs with p50 flat at 42 ms. The cost of more than one log is a barrier about 2x one log's
   (21 ms to 42 ms), whatever N, which adaptive batching absorbs in throughput and the ack latency
   pays. A per-device log would keep 21 ms but serializes every core on one mutex and one turn in
   flight. Single-index gate rows run one log, so A4 is unaffected; N2 asks for the row.
5. **Tests and parity.** T1 to T9 with M-a to M-f do not yet prove D2 to D6: M-b and M-f need cases
   T1 and T7 do not have (F6), and none of F1 to F4's paths is exercised. Flush-bound parity is
   honest for throughput and median latency by arithmetic (one `F_FULLFSYNC` per write group for
   both, `core-vs-rocksdb-f14.md:20-22`); it is not honest for A4's tail percentiles while the
   checkpoint stalls the writer and its acks inline and RocksDB's memtable flush does not (F1, N4).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | D5 does not say how long the writer lease is held, and both readings break something. If the writer resumes after step 1 (D5's "background", manifest-log D9's model, and D5's "checkpointing a past generation would avoid that stall"), every batch record staged between X and the checkpoint record is superseded by it: RFTW0001 accepts a checkpoint only at the group's last sequence plus one and drops all earlier records of the group, so acknowledged writes above X are gone after the record's barrier, crash or not. A past-generation checkpoint has the same flaw. If the lease is held through step 3, the stall is wait plus flush plus `commit_files()`, not one or two barriers, and acks of already-synced turns wait for it too, since `turn_synced` runs as a chore that a lease holder defers; if step 1 waits on V it deadlocks on itself. | P1; `turn_log.h:209-211,263-269,513-519`; `index.hh:918-938`; design D5 "Order", I1, Limits | State that the lease is held from step 1's seal until the checkpoint record is staged (or re-stage the records above X after the record, which costs their bytes again); step 1 waits on the driver's completion, not on V; the holder runs `turn_synced` while it waits. Restate the stall and M5 as writer wait and ack latency p50/p99/p99.9 across a checkpoint. Test: T1 with applies racing the checkpoint, and mutant M-g (lease released after step 1) failing it. |
| F2 | yes | defect | D2's replay rule fails the open with `replay_diverged` on every non-`accepted` verdict, but `flush_owed`, `drain_owed` and `map_owed` say only that the host owes a chore and must apply again (`index.hh:85-88`); under `defer_io` (the catalog), a reopen whose buffer share is smaller than at runtime (D17's share split across a host replaying many indexes at once, D6.6) turns a correct log into an unopenable index. Without `defer_io`, apply flushes inline (`index.hh:800-801`), and under D5 trigger 1 that flush is a checkpoint whose record, staged mid-replay before `driver.release`, supersedes records not yet replayed (F1's mechanism). | `index.hh:85-88,800-801`; design D2, D5 trigger 1, D6.4 | Replay services owed verdicts (runs the chore, applies again) and flushes as `refresh()` with no checkpoint record until replay ends and the group is released. Test: T7b, replay into a smaller buffer budget under `defer_io`, then a power-off sweep across that replay. |
| F3 | yes | gap | The position {s, j} and the open record are separate steps around stage 1, and D3 names one mutex but not the interleaving. If the sealer seals record s between an apply taking {s, j} and the transaction joining the record, it lands in s+1 while memory says {s, j}: a checkpoint at X ≥ {s, j} waits only for record s (I1 broken), and a replay of s+1 applies it twice, which reads as `replay_diverged` on a correct log. | design D3 "The open record", "Sealing"; `index.hh:803-806,880-881` | Take the position when the accepted transaction joins the record, in one critical section with the append (or hold the record's mutex across stage 1). Test: a deterministic interleaving forcing a seal between stage 1 and the append, and the TSan job over it. |
| F4 | yes | defect | D5 says the log adds no cap beyond what apply admits, but a turn must fit one segment (`turn_log.h:337-340`, 64 MiB by default), and apply admits any transaction the lane holds (`index.hh:4534-4549`). A larger transaction throws `length_error` at stage, inside a turn shared by every index on the core. | `turn_log.h:337-340`; `index.hh:4534-4549`; design D5 "An oversized transaction" | Define the path above the segment size: an `invalid` verdict (narrows behavior: owner's call), a segment sized to the largest admitted transaction, or a direct checkpoint of that transaction with its position covered by the record. T8 adds a transaction past `segment_bytes`. |
| F5 | yes | gap | D4 keeps readers at V, but a `refresh()` outside D5 (a host's refresh for search, a flush that is not a checkpoint) writes a segment holding transactions above V and publishes it, so readers see writes a crash removes. | `index.hh:957-964`; design D4 "Visibility waits for the barrier" | Every flush on a logged index waits for the log through the applied position first, as D5 step 1. T4 adds a `refresh()` between apply and the barrier, and M-c covers it. |
| F6 | yes | gap | Two mutants are not caught by the tests named for them. M-b shows only after the reopened index writes again (new positions reuse those at or below X and are skipped); a single crash and reopen passes the prefix oracle. M-f shows only when a transaction refused at runtime (`no_space`, a stage-1 throw) is replayed; T7 refuses at replay. | design "Crash model and tests", "Mutants" | T1 writes after each recovery and checks again after a second crash. A T10 refuses transactions at runtime (faultfs capacity; a read fault in stage 1), crashes, and checks they are absent; M-f fails T10. |
| N1 | no | gap | The quest file's 2026-11-16 decision (Detent's lead) settles Q1 and Q2 against the design's recommendations: retirement lives in the host's group table, not a `Retire` kind; a failed turn fences and reopen cuts the log at the failed range, zeroing only if appending continues without a reopen. D7, D9, T3 and Limits should follow it, and the `turn_group` attribute should not become a second record of membership. | quest file "Decisions" | Rewrite Q1, Q2, D7 and T3 to the decision. |
| N2 | no | gap | Q3 states N per-core logs cost N flushes per turn period; P2 measures coalescing instead (2x barrier latency, flat from 2 to 8 logs), and M3's group-commit row, one index on one log, never runs N logs. | P2; design Q3, M3 | Correct Q3's rationale to P2's numbers; add a row with N logs on one device, macOS and Linux. |
| N3 | no | nit | `ack=none` acknowledges at apply but shows the write only at its barrier, so a client misses its own acknowledged write for up to a barrier; RocksDB's WAL-unsynced row reads its own writes. Owner's call: keep (state it in the row), make V follow apply on `ack=none`, or read-your-writes for the writer. | design D4 table | Owner's choice. |
| N4 | no | nit | Parity covers throughput; A4 also grades tail percentiles, where the checkpoint's inline stall (F1) has no RocksDB counterpart. | design Summary, Cost | Claim throughput parity; let M5 and M3 report p99.9. |
| N5 | no | nit | One damaged block whose manifest does not read back loses every idle group on the core's log (`turn_log.h:416-418,511`), so a many-index host fails every such index's open with `log_lost`, even one whose head covers its last record. Owner's call whether a tolerant open at H with a reported possible loss belongs here, as RocksDB's WAL recovery modes do. | `turn_log.h:416-418,511`; design D6.4 | Owner's choice; A1 needs one index. |
| N6 | no | nit | A group id reused by another index after a leave fails that index's join-crash case on D6.4's UUID check instead of opening at H. | design D6.4, D7 | Group ids are never reused, or D6.4 treats a foreign UUID with no join record as absent. |
| N7 | no | nit | A failed barrier fails every staged operation, yet its bytes may be durable and replay keeps a whole last turn: those clients should get an outcome-unknown error, not a failure. | `turn_log.h:249-250,473-474`; design D9 | Name the error. |

## Not run

- No code exists; nothing of A1 was built or tested.
- P2 ran on one Mac (Intel, APFS, internal SSD) under a light hold; Linux `fdatasync` on separate
  preallocated files, FreeBSD and arm64 not measured.
- I did not check the command codec's bytes (D3's table) against a fuzzer or fixtures, nor the
  `E::encode_part` hook's fit with every layout.
- Whether consensus's Worker meets F1's supersede rule on the replicated path is not checked; it is
  the same `turn_log.h` rule and worth a look by Detent's lead.
