# rocksdb-parity: journal

State: waiting on jobs 261125-041712-dedde3 261125-041728-c7bab8 261125-035437-7398ae 261125-020727-610311
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

5. Suite fix round (lead: full suite 261125-020751-9cf6ff failed 13 of 139 at strata:54b4c043):
   - Real regression, apply_atomic (commit alloc N=120-122: retry broke the docid invariant):
     `index_ranges()` ran after the `segs_` change and its `bad_alloc` escaped, leaving a flush
     half done and the commit unfenced. Fix strata:8888ecf2: `index_ranges()` is
     noexcept; a failed allocation leaves the ranges unfinished (lookups probe all). Class: every
     `index_ranges()` call site follows a `segs_` change (flush x2, merge publish, checkpoint,
     open, close, destroy, warm, heal, run_merges); noexcept covers all. Test that fails without:
     apply_atomic (failed in 261125-020751-9cf6ff, passes locally after).
   - Old merge shape (tests that force tier merges with segments_per_tier 2 / floor 1 over
     ordered keys, now runs): write_buffer, reclaim_backlog(+_mapped), defer_io(+_mapped),
     shared_read(+_mapped), catalog_host, warm and index_prereq (cold_hooks fixture). New
     behavior is right (disjoint segments are runs); `max_runs = 0` now means no runs, the tiers
     alone (strata:d80addd0), and those tests set it, so they count the merges they did. A first
     try with max_runs 0 as "cap 0" kept merging outputs into the next run (warm demote,
     close_ready, reclaim counts failed): rejected.
   - index(+_mapped): the commit-driven index keeps 40 one-key runs under cap 64 (checked ==
     40) and stays under 20 at cap 8 (strata:d80addd0).
   - Local (quiet light): all 16 affected ctest entries pass with the edits.

6. msg-1 search row (strata:3292724f probe, strata:e73c2683 notes; pushed):
   - Probe `bench/kv/probes/rocksdb-parity/search-runs.sh` + `search_runs.py`: prospect
     `prospect_engine` (prospect:c99e6be, task at strata's pin, one line: `STRATA_MAX_RUNS` sets
     `merge_policy::max_runs` for `index`) built against the quest's strata; 550k Zipfian docs keyed
     in order (zero-padded ids), 4 MiB buffer; A, A2 at max_runs 0 (tiers, built apart = A/A),
     B at 64, C at 16; 200 term / phrase / `+a +b` queries, TOP_10 and COUNT (COUNT = identity).
     strata has no query bench of its own (search is prospect's); new code is in bench/kv/.
   - Light probe (shared host, 2 rounds, 500k docs, A2 = same index): A 4 segments, B 62 runs.
     TOP_10 term p50 59 -> 440 us (7.5x), p99 317 -> 802 (2.5x); AND p50 6.4x, p99 1.5x; phrase
     p50 3.7x, p99 1.17x; COUNT term 14x/12.9x; COUNT AND 4.0x/2.1x; COUNT phrase p99 tie 0.94.
     0 count mismatches. About 6.6 us a run per TOP_10 term query. Indexing 43.8 s -> 20.7 s.
   - Notes: `docs/perf/search-runs.md` (method, light table, reading), sorted-runs.md "Why 64"
     (search fan-out binds; maps/vnodes per segment; range index negligible; RocksDB and Lucene).
   - Jobs: strict smoke 261125-041712-dedde3, exclusive A/B with A/A 261125-041728-c7bab8 (after
     the smoke), both at strata:3292724f, `--ref PROSPECT=prospect@c99e6be`.

## Next

0. msg-1: read 261125-041712-dedde3 and 261125-041728-c7bab8; put the exclusive table (A, A2, B,
   C) in `docs/perf/search-runs.md` "Exclusive A/B" and the note's "Why 64" numbers; answer msg-1
   with them. Light numbers already pass 10% p99 on every TOP_10 row (see Blocked on).
0a. Full suite 261125-035437-7398ae at strata:7ba472a4 (the lead merged
   pending/rocksdb-parity/tier-tests into the quest branch and widened the file set; the earlier
   261125-035316-b8b4f2 is cancelled). Mutants/tests/smoke read: size-gate failed only at 387, no-cap only at
   345 416, no-key-ranges only at 437-441, tests 9f6c9c pass; smoke 4543a9: tenx1 fillseq
   1.33x, wamp 1.25 -> 0.57; twox1 fillseq wamp 1.63 -> 0.58; readrandom 0.88 (1 trial).
0b. Read jobs: tests 261125-020618-9f6c9c (e2c18e42); mutants 261125-020620-96fc88 (size-gate,
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

File set: six test paths outside it carry `max_runs = 0` (commit strata:ff7b4d97 on
`pending/rocksdb-parity/tier-tests`, patch also at ~/.scratch/rocksdb-parity/step2/outside-file-set.patch):
test/catalog_host_test.cc, test/cold_hooks.hh, test/defer_io_test.cc, test/reclaim_backlog_test.cc,
test/shared_read_test.cc, test/write_buffer_test.cc. Widen to them, then the quest branch
fast-forwards to ff7b4d97.

msg-1's verdict (light probe; exclusive pending): 64 runs costs search more than 10% p99 on every
TOP_10 row (term 2.5x, AND 1.5x, phrase 1.17x) on a sequentially keyed text index, against the
tiers' 4 segments. `merge_policy::max_runs` is strata's default for every layout, prospect's search
layout included (prospect's `basic_index_options<search_layout>` takes the default policy), so
prospect inherits this when it next moves its strata pin. Options, none taken in this step (it
changes the mechanism): (a) per-layout default: 64 for `document_layout` (keys and stored: no
postings, only maps and the range index pay), 0 (tiers) for layouts with postings; (b) a lower cap
for all (C = 16 in the exclusive job shows its cost); (c) merge adjacent runs when the index is
idle (background work under load is not free); (d) a per-run skip by term bounds: no help on text
(every run holds the common terms), only fields that follow the key. Recommendation: (a) before
step 2 lands, a one-line default in `segment_policy` or the layout's options plus a test; prospect
keeps the tiers it has measured. Lead's call: it changes prospect-visible behavior.

Earlier question, superseded by the numbers above: the run cap trades search
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
