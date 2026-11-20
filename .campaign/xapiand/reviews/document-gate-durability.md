# document-gate review: durability

Commit: strata:12f59669148d
Verdict: CHANGES
Date: 2026-11-17

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- At strata:12f59669: build-test 261117-075429-907b22 (T1 at 8 power-loss seeds per mutation,
  t1r, T2-T12, M1, served), T6 sweep + t6w + T5 261117-075441-a3837d, t6w TSan
  261117-075442-939866, served/T4/T12/T3/T8 TSan 261117-075430-571f74: all exit 0. These carry
  single-transaction atomicity and prefix recovery under every cut (T1's prefix check is per
  transaction, and its workload has two-op transactions), torn turns under power loss, and the
  close/reopen sequences.
- T1 at 200 seeds 261117-033250-f42f82 (9d7c7710, older code; the 8-seed run above is at the head).
- The kill set M-a..M-r as listed in the journal; I did not rerun any.
- Row jobs at 193e6e32 (sync1 261117-055245-3aecbb, 261117-055253-80fcf5, 261117-055301-8578af;
  wal1 261117-055310-7264db; A/A 261117-055320-5ca39d, 261117-055327-074989; M2
  261117-055334-a41ea3; M4 261117-034943-316356): I read their logs and checked the perf note's
  tables against them; the numbers match.

## What I checked myself

Probe job 261117-082815-50cdbe (exit 0) at strata:12f59669, `.scratch/review-document-gate-durability/run.sh split
wal_window` (probe.cc adds a target through a deferred CMake hook; no source edit):

- `wal_window`: kv_strata's own engine (`bench/kv/kv_strata.hh`, PosixIO, served loop) under
  `wal`, N puts of 16 B keys and 100 B values at full speed, sampling every 1000 puts how far the
  log's durable sequence trails apply. Light hold on a loaded host (four fuzz and coverage jobs),
  so its barriers ran slower than the row's 21 ms; the record counts are the host-independent part:

  | puts | ops/s | records | max lag (records) | lag at end | drain after the run | apply to durable p50 / p99 / max |
  |---|---|---|---|---|---|---|
  | 100k | 163,377 | 15 | 6 | 6 | 731 ms | 261 / 743 / 744 ms |
  | 1M | 35,322 | 142 | 34 | 26 | 5.5 s | 4.8 / 13.4 / 13.4 s |
  | 3M | 30,021 | 432 | 31 | 15 | 2.8 s | 8.5 / 12.0 / 12.1 s |
- `split`: kv_strata's sync batch of 16 x 100 KiB (past the 1 MiB command limit) on faultfs,
  20 rounds, counting the host's barriers per batch. 20 of 20 took two or more barriers (18 split into
  2 records, 2 into 3), so each first part was durable alone for at least one barrier.

Code read: `include/strata/core/turn_seal.hh` (await, close, reopen), `index.hh` `log_state`
(join, seal, durable_through), `attach_log`, `replay_log`, `leave`, `command.hh` (kind_of,
decode_batch, decode_checkpoint), journal cc95ba6 `turn_driver.h` (forget, retired, one sealer
call per turn), `bench/kv/kv_strata.hh`, `test/kv_strata_batch_test.cc`, t6w.

### The five questions

1. **sync1 durability, and the split batch.** An acknowledgement is `durable()` returning after
   `await_durable(s.last)`, the last applied position, so every split part before it is covered
   (one group's records go durable in sequence order). Acknowledged writes survive. A batch past
   the command limit is not atomic: its parts are separate records, and every split batch in the probe took two or more barriers, so a crash between them
   recovers the first part without the rest (T1's per-transaction prefix). No measured
   row reaches the limit (batch 100 x 100 B is about 20 KiB). F2.
2. **close/reopen with a waiter spanning it.** Correct. A waiter's sequence at or below the closed
   life's `retired` throws `group_closed` (index_closed) whether the group is gone or reopened,
   and the reopened life's records start past `retired` (`attach_log` restages at
   max(base, retired) + 1). t6w and its TSan run pass at the head. N1 is a latency nit.
3. **wal1's ack=none window.** F1.
4. **Fail closed.** Yes. Torn turns are journal's (T1/T6 sweep them under power loss at the head).
   An unknown kind or version throws `unsupported_index` in `command::kind_of`, reached from
   `replay_log` and both checkpoint decodes inside `attach_log`; nothing on the open path catches
   it. The codec test checks every truncation, a trailing byte and version 2. N4.
5. **The perf note's honesty.** The sync tables, the A/A and the group-commit gap match their
   jobs. The ack=none section does not (F1); N2 and N3 are smaller.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | The `ack=none` window is not "one barrier plus one write, about 21 ms": it is unbounded under a writer that outruns one record per turn. The driver calls each group's sealer once per turn (journal cc95ba6 `turn_driver.h:447`), `log_state::seal` hands out one record per call, and `log_state::closed` is an unbounded deque with no backpressure on `apply`. With kv_strata's 1 MiB command limit the wal rows produce records faster than one per barrier: the probe's lag reached 34 records and 13.4 s apply-to-durable, and in the row itself (924k ops/s, 15 records in 0.108 s, one drained per 21 ms turn) about 10 records, roughly 200 ms, are still in process memory at the run's end, which matches its settle of 0.565 s against RocksDB's 0.143 s. The backlog's memory (1 MiB per record) is unbounded too. The perf note's "The ack=none window", the design doc's D4 table and Limits, and D103/D104's premise ("up to one barrier") state a bound the code does not have. | probe 261117-082815-50cdbe `wal_window`; `index.hh` log_state seal/join; wal1 261117-055310-7264db phase times | Options, the lead's or owner's call: 1. Bound it: seal every closed record that fits the turn's room (a journal contract change, coordinated with Detent), or hold `apply` once closed records pass k, so the window is k+1 barriers. 2. Keep it, and state the measured window (records and ms, growing with the writer's lead) in the perf note, D4 and Limits, and reopen D103/D104 with it. Either way the test: a served-loop case on faultfs whose writer closes several records per turn, asserting the lag in records stays within the stated bound (option 1) or the docs' figure (option 2). |
| F2 | no | gap | A kv_strata sync batch past the 1 MiB command limit is not atomic: it splits into whole transactions in separate records and turns, so a crash between them keeps the first part, and other readers see the parts at different barriers. The ruling "both make a batch visible at once and atomically" and `kv_strata.hh`'s "as RocksDB's WriteBatch" hold only below the limit; RocksDB's WriteBatch has no such limit. Acknowledged writes stay durable. No measured row reaches the limit. | probe 261117-082815-50cdbe `split`: 20 of 20 batches took 2-4 barriers; `kv_strata.hh` next_op | State the limit in `kv_strata.hh` and the ruling, or have the driver fail a row whose batch passes it. |
| N1 | no | nit | `turn_sealer::await`'s wait predicate does not check `closed_`: a waiter that wakes only after a close and a reopen keeps waiting until the new life's first turn moves `durable` past its sequence, then throws `group_closed`. Correct, one turn late. | `turn_seal.hh` await, cv predicate | Add `sequence <= retired_locked(group)` to the predicate. |
| N2 | no | nit | The perf note's wal rows give p99 only; their p99.99 is 2.4-5.9x higher than RocksDB's (fillseq 428 vs 72 us, fillrandom 465 vs 106, overwrite 194 vs 81), on 100k ops. | wal1 261117-055310-7264db | State the tail beside the p99, or say p99.99 is 10 samples. |
| N3 | no | nit | "Synchronous durability: a tie on every row" while overwrite sync/100's second trial is 1.16x faster, outside the note's own 10% tie band and the A/A spread. | 261117-055253-80fcf5 | "a tie or better". |
| N4 | no | gap | No open-level test of a logged record of an unknown version: the codec test proves `kind_of` throws, not that open surfaces it. `replay_log` also skips non-Batch records past the front without decoding them. | `index.hh` replay_log; test codec() | A T9 case: stage a version-2 batch payload into the group, reopen, expect `unsupported_index`. |

## Not run

- A6 (Linux, FreeBSD, arm64) and A7's ASan/UBSan, fuzz CPU-hour and coverage: queued or owed,
  the lead's to track.
- No power-loss sweep of kv_strata's batch path itself; it is the core's apply and await, which
  T1 sweeps.
- No measurement of the ack=none window on Linux, or under a strict hold (the probe ran light;
  it reports counts of records as well as milliseconds).
- ENOSPC and EIO on the turn log beyond T3's failed barrier.
