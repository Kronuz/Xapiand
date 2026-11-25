# rocksdb-parity: journal

State: waiting on jobs 261125-020618-9f6c9c 261125-020620-96fc88 261125-020622-cd1920 261125-020625-245609 261125-020725-4543a9 261125-020727-610311 261125-020751-9cf6ff 261125-013258-77d915 261124-231353-1ad1c6
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
3. Task 7 (D9 sorted runs), step 1 at strata:19ddcbd0 (doc strata:faff0726,
   `docs/mechanisms/sorted-runs.md`): `run_merges` maps each candidate (a map point, as
   `schedule()`) and passes its key range to the policy, which already settled disjoint runs
   (strata:1d50e0ab) but never received a range; merging or damaged segments count as occupied
   key space (`candidate::overlapped`). Tests: `background_merge` `sorted_runs` (sequential load
   merges nothing before and after a reopen; a segment across the runs merges them; contents equal
   a tiered twin's), `merge_policy` overlapped case. Mutant branch
   `mutant/rocksdb-parity/no-ranges` (strata:19ddcbd0 with base `index_merge.hh`).
   Light probe (shared host, 1 trial, bodied-only variant): tenx1 fillseq written 210.8 -> 134.2
   MiB (wamp 1.91 -> 1.21), CPU 4.19 -> 2.73 us/op, 275k -> 362k ops/s; random-key rows unmoved
   within noise. Identity (light): fillseq --after readall, get digest eb2a3ad93b05aafe, 1,000,000
   hits on both base and fix.

4. Task 7 step 2 (launch 2), strata:34791d51 (code, tests), strata:e2c18e42 (note), strata:54b4c043
   (probe `bench/kv/probes/rocksdb-parity/runs-readall.sh`):
   - Runs at any size: `run_bytes` is gone; a segment whose range overlaps no other is a run.
     Light probe (`run_bytes = 0` build, shared host): tenx1 fillseq wamp 1.17-1.30 -> 0.575,
     62 segments (one per flush, about 1 MiB each); RocksDB in the same session 0.642, 103 files.
   - Run cap: `merge_policy::max_runs` = 64; past it `cap_runs` merges the key-adjacent window (no
     other segment between, up to the merge factor) writing the fewest bytes per segment removed.
     Simulation, 200 sequential 12 MiB flushes: 60 segments, write amp 1.80 (none at cap 1000).
   - Range index: `key_ranges` (`include/strata/core/key_ranges.hh`): mapped runs by first key,
     binary-searched; the rest (overlapping, unmapped) probed by position. `lookup_key` (every
     upsert's read) and `get` use it; `index_ranges()` rebuilds it after every `segs_` change
     (flush x2, merge publish, checkpoint install, open, close, destroy, warm twins, heal) and after
     `run_merges` maps segments; a list of another length or an unfinished rebuild probes all.
     Debug build confirmed 62 runs of 62 segments after tenx1 fillseq (debug line not committed).
     Without it (the `run_bytes = 0` build) the fill ran 0.3 us/op slower on the writer: every
     upsert's `lookup_key` walked 62 segments.
   - Light probes (3 interleaved, shared host at load 50+): fill CPU 3.29 -> 1.97-2.08 us/op;
     readall after fillseq 369k -> 323-333k ops/s in one session, 390k -> 442k in another: noise;
     the exclusive job decides. Identity: get digest eb2a3ad93b05aafe, 1,000,000 hits, A and B.
   - Mutant branches (each one change on strata:34791d51): `mutant/rocksdb-parity/size-gate`
     (runs need 4 MiB, baeda3b4), `no-cap` (97d5d86b), `no-key-ranges` (find probes all, 6aaa31e3).

## Next

0. Read jobs: tests 261125-020618-9f6c9c (e2c18e42); mutants 261125-020620-96fc88 (size-gate,
   must fail only at merge_policy_test:387), 261125-020622-cd1920 (no-cap, only 345 and 416),
   261125-020625-245609 (no-key-ranges, only 437-441); strict smoke 261125-020725-4543a9; exclusive
   A/B with A/A 261125-020727-610311 (A = strata:31fcb7b3, B = 54b4c043; runs set 3 trials plus
   readall over runs 3 trials); full suite 261125-020751-9cf6ff (tests outside the file set may
   count on sequential keyed segments merging: a failure there is "Blocked on"). Keep or revert
   step 2 on the exclusive rows; put them in `sorted-runs.md` "Measured" and Result.

1. Read build-test 261125-013258-77d915 at strata:31fcb7b3 and the RocksDB rows
   261124-231353-1ad1c6 (tenx1, twox1 at strata:19ddcbd0); put the fillseq rows against RocksDB in
   `sorted-runs.md` and Result.
2. Task 7 step 2: the runs' range index for gets and the run-count cap (N13); then settle flushes
   under `run_bytes` (write amp 1.31 -> ~0.6 needs it; RocksDB's is 0.64).

## Jobs read (launch 1)

- 261124-231305-3c9f9c, tests at strata:19ddcbd0: merge_policy and background_merge pass (exit 0).
- 261124-231315-48aa2b, mutant `mutant/rocksdb-parity/no-ranges`: background_merge fails at
  sorted_runs' `segments().size() == 6` (lines 244 and 251), as required (exit 0 of the inverted
  check). A plain `--fails-without` needs `--for`, and no criterion fits a merge-policy test (A2 is
  log formats), so the job greps for the failing CHECK instead.
- 261124-231329-08a553, strict smoke beside 7 light holds, 1 trial, B/A: tenx1 fillseq 1.16x
  faster, CPU 5.78 -> 4.63 us/op, write amp 1.78 -> 1.32, peak RSS 160 -> 103 MiB; tenx1 fillrandom
  1.02x (tie); twox1 fillseq 0.94x, write amp 1.04 -> 1.63; twox1 fillrandom 1.04x; tenx1
  readrandom 0.95x. Noise band unknown until the exclusive A/A.
- 261124-231350-d66cf1, exclusive, A=strata:7823c51a B=strata:19ddcbd0, 3 trials, medians: tenx1
  fillseq write amp 2.08 -> 1.31, CPU 2.84 -> 2.10 us/op, peak RSS 134 -> 89 MiB, ops/s 1.07x
  [1.00, 1.24]; tenx1 fillrandom 0.97x tie (wamp 2.10/2.09); twox1 fillseq tie (wamp 1.62/1.62,
  CPU 2.05/1.88): the smoke's 1.04 -> 1.63 does not reproduce, the base reads 1.62 here too, so it
  was merge timing under load; twox1 fillrandom tie (wamp 1.71/1.22, timing); tenx1 readrandom
  0.96x tie. A/A spread: A 3.3-10.5%, B 4.5-37.5% (fillrandom). Identity: readall get digest
  eb2a3ad93b05aafe, 1,000,000 hits, on A and B. Verdict: land; no row loses past the A/A band.

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

Nothing blocks step 2. One question for the lead, default taken (64): the run cap trades search
cost for bytes written. An index of text documents keyed in order (auto-increment ids) now holds
up to 64 segments where the tiers held about 16, and each segment costs a query its setup
(query-gap H20: 16 segments cost 1.03-1.58x before P6). Options: (a) keep 64 (KV parity: tenx1
fillseq writes each byte once); (b) cap by layout, high for keys-and-stored layouts, the tier
budget where postings exist; (c) lower the cap to 16 and accept rewrites past it. Recommendation:
(a) now, (b) when a query row on a sequentially keyed text index is measured.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |

## Result

Task 7 step 1 (D9 wired), strata:19ddcbd0, note `docs/mechanisms/sorted-runs.md` (strata:31fcb7b3):
the index passes each segment's key range to the merge policy, so a sequential load's segments
settle as runs instead of merging by size. tenx1 fillseq: 37% fewer bytes written (write amp 2.08
-> 1.31), 1.35x less CPU per op (2.84 -> 2.10 us), 34% less peak RSS (134 -> 89 MiB); every other
row in the runs set ties (job 261124-231350-d66cf1). Against the gate's RocksDB (wamp 0.64, CPU
1.68 us): bytes written 3.2x -> about 2.0x, CPU 1.97x -> about 1.25x, pending the same-session
rows of 261124-231353-1ad1c6. Open: step 2 (range index, run cap, settling flushes under
`run_bytes`).

## Rulings I made

- Task 7 chosen for launch 1 (reasons under the gap table).
- Task 7's first step passes key ranges only for mapped segments: an unmapped candidate stays
  unranged, so nothing settles until every candidate is mapped (the policy's own rule), and the
  merge scheduler never maps a cold segment just to decide.
- Task 7 step 2: `run_bytes` removed rather than lowered: the per-segment costs it stood for are
  a count, so the cap counts runs (D9's "caps the run count by per-segment fixed costs").
- `max_runs` = 64: about twice the segments the tiers hold at their largest (8 a tier, about four
  tiers to the 2.5 GiB half-maximum); RocksDB caps nothing (103 SSTs after tenx1 fillseq). It is
  the default only; see "Blocked on" for the search-cost side.
- The range index covers the index's own `lookup_key` and `get`; snapshots and views keep the
  walk, scans keep opening every run past their start (both listed under Limits in the note).
- Lead's narrowed file set (2026-11-25, for strata-journal-behind): the branch's seven paths
  (`include/strata/core/index_merge.hh`, `merge_policy.hh`, `test/background_merge_test.cc`,
  `test/merge_policy_test.cc`, `bench/kv/ab.py`, `docs/design/rocksdb-parity.md`,
  `docs/mechanisms/sorted-runs.md`) are all inside it; nothing to move. Task 7 step 2 needs no
  path outside it (`index.hh`, `index_read.hh` are under `include/strata/core/**`).
