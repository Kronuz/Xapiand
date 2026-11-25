# rocksdb-parity: journal

State: working
Quest: .campaign/xapiand/quests/rocksdb-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Launch 1 (2026-11-24): first commit, gap table, one strata-only task.

1. Done: strata:7823c51a adds the lost bound's first-open limit to the design's A2 (item 3), closed
   by detent:journal-bound-first-open (journal:870382d, on journal main through journal:fc5c59b);
   strata's journal pin (journal:9c7e31c) predates it.
2. Done: the gap table below.
3. Task 7 (D9 sorted runs): prototype in `index_merge.hh` `run_merges` passes each mapped
   segment's key range to the policy, which already settles disjoint runs (strata:1d50e0ab) but
   never received a range from the index. Probing base against it on tenx1 rows (light).

## Next

1. Read the light probe; if it wins, test (identity and settled-run behavior), then the strict
   smoke and one exclusive job for the rows it moves.

## Gap table (launch 1)

Source: document-gate's report on strata main (`docs/gates/document-profile.md`, strata:d956c0f4,
rows at strata:dd02c63d). No exclusive row has rerun since (turn-log-parity's journal, "No A6
exclusive rows yet"), so these are the gaps at main; landed since: task 1 (cohort wait, light smoke
only), task 4's checkpoint window (`checkpoint_floor_bytes`), task 8, and D9's policy half
(strata:1d50e0ab, not wired: see task 7). Ratios strata over RocksDB.

| Row (axis) | Short by | Task |
|---|---|---|
| Disk: sync/1 fillrandom; sync/10; sync/100 fillrandom; `wal1` fillseq, fillrandom; `wal1` overwrite; sync/100 overwrite; VM sync/1 | 39.5×; 7.9×; 3.3×; 3.7×; 2.6×; 1.74×; 13.8× | 3 (journal: segment ladder, spare pool) |
| Peak memory: sync/1 YCSB A, F; sync/100 overwrite; `wal1` | 3.4×, 3.1×; 2.6×; 2.2-2.9× | 2 (D3, turn-path buffers; half journal) |
| Peak memory: `big1` fillrandom, `tenx1` fillseq, fillrandom; `tenx1` overwrite; other `big1`/`tenx1`; `fits1`; `twox1` | 2.3×; 2.1×; 1.23-1.87×; 1.11-1.73×; up to 1.69× | 6 (D4 background flush, smaller active buffer) |
| Bytes written: `tenx1` fillseq; `twox1` fillseq | 3.2× (wamp 2.05 vs 0.64); 2.5× (1.62 vs 0.65) | 7 (D9) |
| Bytes written: `wal1`; sync rows | 1.82-2.2×; 1.36-1.92× | 3 (zero-fill), 4 (forward copies; landed), 2 |
| CPU/op: `tenx1` fillseq | 1.97× (3.32 vs 1.68 µs) | 7 |
| CPU/op: `tenx1` overwrite | 1.42× (5.14 vs 3.63 µs) | 11 (read before upsert), then 6, 7 |
| CPU/op: sync/1 fillrandom | 1.15× | none: not reproduced (design, CPU item 4); A1's rerun decides |
| CPU/op: `wal1` in the VMs | 1.40-1.62× | 2, then 12 (VM profile) |
| p99.99 `wal` | 130-193× (9.5-17.2 ms vs 73-92 µs) | 5 (journal side: one bound per log), 3 (preallocation off the turn) |
| `wal1` throughput in the VMs; p99 | 1.15-1.38× slower; 1.42-1.87× | 5, 2, 3, 12 |
| Group commit K=4 on 4 logs; K=8 on 8 logs | 2.1×, 1.9× slower | 10 (journal: one log per device) |
| Group commit K=4 on 1 log | 1.39× slower | 1 (landed; light smoke 181 vs 131 ops/s, exclusive row owed) |
| 1 KiB fillrandom, tier none | 0.78× | 6 |
| p99 tier none: `tenx1` fillseq, fillrandom, overwrite; `twox1` overwrite | 1.19-2.08×; 1.64× | 6 (inline flush); 7 for fillseq's merges |
| p99 sync/100 fillrandom, overwrite | 1.30×, 1.13× | none named (both engines vary 2× between trials); 6 |
| Time to durable: `fits1` overwrite; YCSB A | 1.14× (0.79-0.96× over 4 trials); tie 0.93× | 6 |
| sync/1 fillrandom, YCSB F throughput | 1-2% | none: A1's rerun decides |

Task choice (strata-only: 1, 2, 4, 6, 7, 11): 1 and 4 have landed. The largest gaps a strata-only
change closes whole are task 7's: bytes written 3.2× and CPU 1.97× on `tenx1` fillseq (2.5× bytes
on `twox1`). Task 2's peak memory (3.4×) is larger, but half its megabyte buffers are journal's
(`logical()`, the emitter's string, forward copies), so strata's half alone cannot be shown to close
it; task 6 needs journal's `covers`; task 11 adds a public API (1.42×).

## Tasks that change journal (not started)

What each needs before it starts, per the quest's contract (Detent's lead sees each diff, with its
journal test, before it lands):

- 3: journal `turn_log.h`, `turn_driver.h`, `framing.h` (segment ladder, room following the
  segment, off-turn spares, copy flag, per-fragment checksum); a record-format change, so
  `format-freeze` review and a `--fails-without` crash test; journal test "the room follows the
  segment" (no turn throws, every admitted record seals next turn); copy-flag tests (a group
  rewritten after double damage recovers, one not rewritten reports the loss).
- 5: strata `log_state::join`/`seal` and `turn_seal.hh` (one admission counter per log), journal's
  packed-turn half already on main (6187a32): needs strata's pin moved to journal main first;
  test: 8 saturating `ack=none` groups and one `ack=sync`, every record seals in the turn after
  admission, mutant with the per-group bound fails (probe G).
- 9: journal `host.h`, `turn_log.h`: refuse a current-generation log with loop >= L and a
  `block_bytes` neither a multiple nor a divisor of 512; tests for both refusals.
- 10: journal `host.h`, `turn_driver.h` plus strata loop wiring; needs `probe-e.sh` and `probe-f.sh`
  floors on bare-metal Linux first (no such host: only VMs), D6's handoff protocol and its test
  (a wake on a non-owner loop, power cut at every operation).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |

## Result

<When ready for review.>

## Rulings I made

- Task 7 chosen for launch 1 (reasons under the gap table).
- Task 7's first step passes key ranges only for mapped segments: an unmapped candidate stays
  unranged, so nothing settles until every candidate is mapped (the policy's own rule), and the
  merge scheduler never maps a cold segment just to decide.
