# cold-tier-step2: journal

State: waiting on jobs 261110-175050-5f4b70 261110-175103-e894df 261110-175117-cf8c26 261110-175119-5ed6d2 261110-175121-79ed08 261110-175124-924979 261110-175126-a4c90d 261110-175128-0c2a26 261110-175131-d1e3f9 261110-175133-51d13b 261110-175136-37ec7d 261110-175139-fed9f8 261110-175141-fe6964 261110-175143-04d6bb 261110-175145-97cb94 261110-175147-213dd5 261110-170249-bc3fdd 261110-165809-f0141f
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head strata:0c054df1 (pushed; CI run 42941832467). Since c58a962d: A1's counters required in
`catalog_stress_test` (strata:d5b9fe05), and `bench/catalog_tiers` with `tiers.sh` for T1 to T4 and
T2d (strata:0c054df1; how it runs: strata `docs/perf/catalog-tiers.md`). The macOS rows are queued
as strict jobs behind the build (e894df, light) and a smoke (cf8c26, strict, every mode on 2,000).

- 3af67b (build-test at c58a962d) passed. c8f75e (A7) FAILED: the unmutated `apply_throw_probe`
  aborts, `[keys, lazy] apply of {replace Q2, replace Q1}: corrupt_segment: keys checksum`, so the
  revert check cannot judge (`PROBLEM: (unmutated)`); the three revert mutants are killed. Lazy
  segments being the default is the likely cause (the probe's lazy case); not investigated (not in
  this contributor's scope).
- bc3fdd (A2) is running; f0141f (A3 baseline, A9) is queued.

## Next

1. Read the jobs. 5f4b70: build-test at the head. CI 42941832467: A1 (TSan, ASan+UBSan logs show
   `catalog_stress` passed; a pass now requires every T5 counter) and A11.
   cf8c26 (smoke) then T1 5ed6d2 (d1s1), 79ed08 (d100s1), 924979 (d1000s1), a4c90d (d100s10),
   0c2a26 (d1000s10); T2/T2d d1e3f9 (d1s1), 51d13b (d1000s1), 37ec7d (d1000s10); T3 fed9f8; T4
   fe6964 (0.99), 04d6bb (0.99 scan), 97cb94 (1.2), 213dd5 (1.2 scan). Write each row, with main's
   interleaved baseline, into strata `docs/perf/catalog-tiers.md` "Results" and decide A5's
   sentence per platform. A job that ends 124 (the 30-minute bound): split it (t1 by reps, t4 with
   `WARM REQUESTS` halved twice) and resubmit.
2. Read bc3fdd (A2) and f0141f (A3, A9) as the previous Next said: survivors against the item
   ledgers' equivalents; K6's rows into `docs/perf/idle-index.md` with the reps' spread.
3. Delete `.scratch/tiers` (about 20 GB of fixtures) once the rows are in.

## Blocked on

- **Lead, Linux host** (D24; the qemu VMs on 18762/18763 are Detent's, so not touched here). On
  the gate VM, root, a checkout of strata at 0c054df1 (`SRC`) and of main at 15d39c30 (`MAIN`),
  12 to 16 GB free for T3, `ulimit -Hn` at least 150,000 for T4 (CT-R12), stock sysctls:
  ```
  OUT=/var/tmp/tiers T=$SRC/bench/catalog_tiers/tiers.sh
  $T build $OUT $MAIN 100000 1000000
  $T smoke $OUT
  for f in d1s1 d100s1 d1000s1 d100s10 d1000s10; do $T t1 $OUT $f 100000 3; done
  for f in d1s1 d1000s1 d1000s10; do $T t2 $OUT $f 100000 1; $T t2 $OUT $f 100000 1 drop; done
  $T t3 $OUT 1000000
  for c in "0.99 noscan" "0.99 scan" "1.2 noscan" "1.2 scan"; do bash -c "$T t4 $OUT $c"; done
  ```
  Each line under a host hold of at most 30 minutes (`QUIET_VM=<qemu pid> quiet strict ...`); the
  script picks Linux's caps (soft_hot 10,000; soft_open 50,000 for T3, 110,000 for T4).
- **Lead, macOS cache dropped** (A5's "cache dropped" on macOS): `purge` needs root, so the macOS
  jobs run warm only. In a checkout at 0c054df1, after e894df built the fixtures, under
  `quiet strict`: `for f in d1s1 d1000s1 d1000s10; do sudo bench/catalog_tiers/tiers.sh t2
  /Users/kronuz/code/Kronuz/strata-wt/xapiand/cold-tier-step2/.scratch/tiers $f 100000 1 drop; done`.
- **Finding for the lead** (not blocking): every `basic_index::open` syncs the directory, then the
  manifest log (strata `include/strata/core/index.hh` the rollover's D6.6 rule, on main too). A
  `sample` of `idle_index open` on this host under load spent nearly all its time in those two
  `fcntl`s; opens ran 25 to 60 ms. T2's cold row, T3's touch and T4's misses pay two syncs per open.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `test/catalog_stress_test.cc` requires T5's counters over the run (strata:d5b9fe05; untallied reposts fail it after eight repeats, local); CI 42941832467 at strata:0c054df1 (running): `linux-tsan`, `linux-asan-ubsan` |
| A2 | 261110-170249-bc3fdd (running): `tools/mutants/cold_tier.py build-mut 1000` over `tools/mutants/cold_tier_table.py` (S3 21, S4 34, S5/S6 31, S7 81) |
| A3 | 261110-165809-f0141f (strict, queued): main strata:15d39c30's `idle_index`, interleaved with this tree's, built by 261110-164724-95e365 |
| A4 | macOS (running, strict, behind e894df and cf8c26): T1 5ed6d2 79ed08 924979 a4c90d 0c2a26; T2 d1e3f9 51d13b 37ec7d; T3 fed9f8; T4 fe6964 04d6bb 97cb94 213dd5. Linux: Blocked on |
| A5 | macOS cache warm: T2d rows of d1e3f9 51d13b 37ec7d (defer_io warm/cold/hot, promote() per segment). Dropped and Linux: Blocked on |
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test passed at strata:5e06300b (261110-164227-8f0e42), strata:85dd1df6 (261110-165622-0ff5b1) and strata:c58a962d (261110-170310-3af67b); head: 261110-175050-5f4b70 |
| A7 | 261110-164238-c8f75e (FAILED: the unmutated probe aborts, see Doing): `tools/mutants/rollback.py` over `apply_atomic_test` and `tools/mutants/apply_throw_probe.cc`, three reverts of the rollback |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test as A6 |
| A9 | 261110-165809-f0141f (as A3) |
| A10 | `docs/status/cold-tier.md` "K7: who builds snapshots from `seg()`" (strata:5e06300b, 85dd1df6, 8f372200), with what prospect's tests expect under lazy segments; 261110-164333-5b4cd8 (prospect at its pin, lazy: 48 of 51, the 3 listed) |
| A12 | lazy is the default (strata:6d4e1164); build-test as A6 (head: 261110-175050-5f4b70); the eager whole run is in c8f75e, which failed on its probe step before it (see Doing) |

Examined: 261110-164701-5446c4, 261110-164703-b52661 (cancelled: `$JOB_REF_MAIN` unexpanded
without a shell), 261110-164726-ca2d5c (cancelled: replaced by the interleaved f0141f),
261110-165548-809d01 and 261110-165819-a33792 (cancelled: build-test superseded by a newer head).

## Result

<When ready for review.>

## Rulings I made

- A12: `STRATA_EAGER_SEGMENTS=1` now forces eager for a whole run (it was `=0` forcing lazy), so
  the eager path keeps a whole-suite run. CI's workflow is outside the file set: no CI job runs
  the suite eager; the K3 job does once.
- A8: Q3's twelve cases on main's sections: term index → `key_index`, postings → `keys`, stored →
  stored; search (no search in core) → `find_key`, the read that skips stored, so one case
  answers exactly. Scan's four added (16 cases).
- A6: no mutant: the control (a lease-less read of a second owed index maps and counts) shows the
  oracle is not vacuous; `pin_after_promote` already kills "pin before the wake ends".
- A10: the two pin bumps that adopt `seg()` are on file (prospect:e49453d, Xapiand:74d8236e5).
  prospect's suite runs at its own pin (strata:3df7af4f, before the manifest log) with
  `STRATA_EAGER_SEGMENTS=0` to stand for its next bump. Xapiand's suite is not run lazy: a host
  forces eager by `defer_io` (`shard.cc:170`).
- A2: the 34 anchors the code moved past are rewritten to today's text with the same rule
  broken (S3-M7 to M11, M21; S4-W1, B1, R1, F1, D1, D3, T1, X1, X2, SZ2; S5-A1, A4, A7 to A9, A11
  to A13, B2, C2, C5; S7-Z8, Z10, Z20, Z42, Y29, Y30, T3). S4-X2's lock stand-in gains `unlock()`
  (the open now unlocks early); S5-C2's second edit writes `failure`, the exception built before
  the mutex.
- `docs/status/cold-tier.md` conflicts with `design/cold-tier` at its header (it does on main
  too); the K7 section sits between K8 and Log, which both sides leave alone.
- A1: T5's "busy restores" is the policy's `stats.busy` (a candidate put back: pinned, in a
  transition, or used since; `catalog.hh:1527`, `:1546`); "open failures in backoff" is
  `failed_fast - wake_gated`; "a reopen in place among" the stale handles is counted by the
  pinner: a re-pin by a handle it gave to `reopen()` that comes back with another generation. The
  counters are summed over the stress phases and required once each; a run short of one repeats
  the defer_io phase with the policy up to eight times (each phase meets them by timing).
- A1: T5's step-2 shape (64 indexes, two catalogs on one budget at 8 open, 12 descriptors, 16
  maps, sweep at 1 kHz) is not added to `catalog_stress_test`: the lead's scope was the counters;
  the two-catalog caps are item 2's `two_catalogs` and `stress` (`docs/status/cold-tier-step2.md`
  "two catalogs on one budget"). Say if A1 needs that shape in this test.
- A4: cold is demote then close: a close needs the slot's maps at zero, the sweep's order.
- A4: a one-document index has one segment, so the 10-segment fixtures are d100s10 and d1000s10
  (T1), and T2's "1 document, 10 segments" cell is d1000s10's only.
- A4 T3 (macOS): `kern.num_vnodes` is the system's vnode cache, at `kern.maxvnodes` (263,168)
  here even at idle, so the row compares this process's vnodes (distinct files behind its
  descriptors and mapped regions, `proc_pidinfo`) with the budget's hard cap, and reports the
  system's. The T3 job is strict, as the lead asked, not exclusive.
- A4: `idle_index`'s meters and heap counter move to `bench/idle_index/meters.hh` and
  `heap_count.hh` (no change in what it prints), so both harnesses meter alike.
