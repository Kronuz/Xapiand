# cold-tier-step2: journal

State: waiting on jobs 261110-211214-179f5f 261110-211216-248ffb 261110-211218-6a6200 261110-211233-3c01fa 261110-211220-b3b9af 261110-211221-3a7f09 261110-211223-969922 261110-211225-45bedb 261110-211226-81fd7c 261110-211228-c5746c 261110-211229-6ec5bb 261110-211231-026a08
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head strata:dbfc1898 (pushed). Since c71d904f: A7's probe folded into `apply_atomic_test`
(strata:f2caed00); the A2 survivors' tests tightened (strata:88d18147, dbfc1898); T5's two
catalogs on one budget in `catalog_stress_test` (strata:955343df, 9ef55ee5); the tier rows scaled
to 10-minute jobs under D59 (strata:338e72c2).

- D59 (Xapiand:d558d0f73): the 15 strict jobs (430 minutes declared) are cancelled. Each row is
  now one strict job of at most 10 minutes holding its interleaved pairs, behind one light build
  (b3b9af: both harnesses' binaries, fixtures kept): T1 1-segment 3a7f09 (d1s1, d1000s1) and
  10-segment 969922 (d100s10, d1000s10), 1,000 indexes, 2 pairs; A9 45bedb (k6, d1s1 d100s1
  d100s10, 1,000, 2 pairs); T2/T2d 81fd7c (d1s1 d1000s1 d1000s10, 500, 3 pairs); T3 c5746c
  (50,000 indexes, soft_open 2,000, soft_hot 500, 100,000 Zipf requests); T4 6ec5bb (0.99, scan
  and not) and 026a08 (1.2): 20,000 indexes, soft_open 1,000, soft_hot 250, 20,000 + 40,000
  requests. 68 strict minutes declared. Sizing (a light hold, not a measurement): 5,000 main
  opens 26 s, tier hot 8 s, cold defer 28 s; first-query cold defer on d1000s10 80 s; T3 on
  20,000 66 s; T4 on 20,000 149 s.
- build-test failed at 9ef55ee5 (261110-184531-4c1fc1, also 443289): `catalog_wake_test.cc:525`.
  My S5-B9 check read `auto edges = w.cat->stats().edges`, a pointer into a dead temporary
  (an array member decays); -O1 passed by luck, -O2 reproduced. Fixed at strata:dbfc1898.
- S7-T3 survived four TSan runs (261110-184529-a16dbb; S4-S3, S4-X2, S7-T1 were killed there).
  `shutdown_joiners` (strata:dbfc1898) races four joins on a catalog with no slot open, so one
  join completes the shutdown while the others push; it kills S7-T3 on a plain build (local).
  Local asserts-on: S4-S2, S5-B9, S7-Z8 and S7-T3 all killed.
- A7, root cause: the rollback is sound; the probe's oracle was wrong under the lazy default.
  Reproduced on an asserts-on build: `[keys]`, `[keys, lazy]` throw in stage 1 and leave nothing;
  eager opens refuse `key_index` and `versions` damage; lazy opens accept it (D11), the first
  keyed apply throws and the segment stays failed, so the probe's unguarded follow-up
  `apply({add Q3})` threw `corrupt_segment` and aborted. `[stored]` applied whole (no throw) and
  the probe still called Q2=b2 "HALF VISIBLE". Fix: `section_attempt` in `apply_atomic_test.cc`
  (build-test) over the four sections, eager and lazy: refused, or thrown with nothing visible, or
  whole; a follow-up may throw `corrupt_segment` only after a throw; an absent key's read must
  throw, not answer absent; exactly four cases reach stage 1. `apply_throw_probe.cc` deleted;
  `rollback.py` builds the test only. Local (asserts on): passes; the three reverts are killed.
- A2 survivors, judged:
  - S4-S2: the anchor (bound close, `close_ready() && maps == 0`) is equivalent: the bound
    close runs at pins 0, and a demote leaves no map. Re-anchored to the sweep chore's maps test
    (`ready = ready && maps == 0`), kind `sweep`, run by `catalog_sweep_test`.
  - S5-B9: `bound_and_backoff` now pins a closing slot after `begin_bound_close`; the reopen
    must go closing to opening once with no closing-to-free edge, then fail with the bound's
    backoff, which a reopen in place without the reuse reset breaks.
  - S7-Z8: `reclaim_zero_under_demoting` drops the held view inside the demote chore, so only
    `reclaim(0)` frees its maps.
  - S4-S3, S4-X2, S7-T1: data races, killed only by TSan (their ledger rows say so); the asserts
    build of bc3fdd cannot see them. Killed by TSan in a16dbb; rerun at the head in 6a6200.
  - S7-T3: survived TSan; `shutdown_joiners` now kills it (above).
  - S4-SZ1: dropped, uncompilable: its edit fails `catalog_test.cc:1057`'s
    `static_assert(slot_bytes() <= 192)`.
- A1: `stress_two` (plain and defer_io), local asserts-on: maps at 16 of 16, descriptors at most
  33 of 64, 0 kernel refusals, 0 over a cap, 50 to 87 merges; five whole runs (`400 4` twice,
  `3000 4` twice, default) pass.

## Next

1. Read the jobs. 3c01fa: build-test at strata:dbfc1898. CI on the pushed head: A1 (`linux-tsan`,
   `linux-asan-ubsan`). 179f5f (A7, K3, A12 eager run). 248ffb (A2, every mutant, 1,000 seeds;
   the three TSan-only ones survive there) and 6a6200 (those three and S7-T3 on TSan).
2. The rows: 3a7f09 and 969922 (T1), 45bedb (A9), 81fd7c (T2/T2d), c5746c (T3), 6ec5bb and
   026a08 (T4). Each into strata `docs/perf/catalog-tiers.md` "Results" (A9's into
   `docs/perf/idle-index.md`) with main's interleaved pairs and their spread as the interval; A5's
   sentence for macOS warm. A second run only when the interval straddles a target (D59). A job
   ending 124: halve its count, never split into reps.
3. Delete `.scratch/tiers` (about 20 GB of fixtures) and `.scratch/k6` once the rows are in.

## Blocked on

Nothing.

## Milestone checks

Run once on main, not gates of this quest (D57: platforms are CI's; no Linux host, no AWS, the
qemu VMs on 18762/18763 are Detent's). Under D59 each line below is split into jobs of at most 10
minutes when it runs.

- **Full-scale macOS rows (A4, deferred under D59).** T1 and T2 at 100,000 indexes, T3 on
  1,000,000 (`tiers.sh t3 $OUT 1000000`, soft_open 50,000, soft_hot 5,000), T4 over 1,000,000
  (`tiers.sh t4 $OUT THETA [scan]`, 5M + 20M requests); the fixtures from `tiers.sh build $OUT
  $MAIN 100000 1000000`.

- **Linux rows (A4, A5).** On a Linux host, root, a checkout of strata (`SRC`) and of main at
  15d39c30 (`MAIN`), 12 to 16 GB free for T3, `ulimit -Hn` at least 150,000 for T4 (CT-R12),
  stock sysctls:
  ```
  OUT=/var/tmp/tiers T=$SRC/bench/catalog_tiers/tiers.sh
  $T build $OUT $MAIN 100000 1000000
  $T smoke $OUT
  for f in d1s1 d100s1 d1000s1 d100s10 d1000s10; do $T t1 $OUT $f 100000 3; done
  for f in d1s1 d1000s1 d1000s10; do $T t2 $OUT $f 100000 1; $T t2 $OUT $f 100000 1 drop; done
  $T t3 $OUT 1000000
  for c in "0.99 noscan" "0.99 scan" "1.2 noscan" "1.2 scan"; do bash -c "$T t4 $OUT $c"; done
  ```
  Each line under a host hold of at most 30 minutes; the script picks Linux's caps (soft_hot
  10,000; soft_open 50,000 for T3, 110,000 for T4).
- **macOS cache dropped (A5).** `purge` needs root. In a checkout, after `tiers.sh build` made the
  fixtures, under `quiet strict`: `for f in d1s1 d1000s1 d1000s10; do sudo
  bench/catalog_tiers/tiers.sh t2 $OUT $f 100000 1 drop; done`.

## Finding for the lead: every open syncs twice

- Where: `basic_index::open` (strata `include/strata/core/index.hh:598`, `ix.recover(to)` at
  `:607`) calls `recover()` (`:5189`), whose newest-generation branch runs `sync_dir()` (`:5263`)
  then `log_->sync_data()` (`:5265`). Same lines on main strata:15d39c30.
- Cost: a `sample` of `idle_index open` on this Mac under load spent nearly all its time in
  those two `fcntl`s; opens ran 25 to 60 ms each. T2's cold row, T3's touch and T4's misses pay
  both per open; at millions of indexes per host that is the open's whole cost.
- Why it is there (D6.6, C4): a rollover's rename or an edit a crash left in the page cache must
  be durable before this process reports that generation durable, or appends an edit that writes
  no new file after it.
- Does a read-only open need it: not at open. It writes nothing, so the only exposure is reporting
  a generation durable (an applied index acknowledged to a replica) or serving it as committed
  before a power loss takes it back. Deferring both syncs to the first append, commit or
  durability report keeps D6.6 and frees query-only opens. Not fixed here.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | `test/catalog_stress_test.cc`: T5's counters required over the run (strata:d5b9fe05) and two catalogs on one budget, `stress_two` (strata:955343df, 9ef55ee5); build-test 261110-211233-3c01fa and CI at strata:dbfc1898: `linux-tsan`, `linux-asan-ubsan` |
| A2 | 261110-211216-248ffb: `tools/mutants/cold_tier.py build-mut 1000` over `tools/mutants/cold_tier_table.py` at strata:dbfc1898 (S4-S2, S5-B9, S7-Z8, S7-T3 tightened); 261110-211218-6a6200: S4-S3, S4-X2, S7-T1, S7-T3 on TSan; S4-SZ1 dropped (uncompilable, see Doing); earlier 261110-170249-bc3fdd (7 survivors), 261110-184529-a16dbb (TSan: S7-T3 survived) |
| A3 | main strata:15d39c30's `idle_index` interleaved in each row's job: 3a7f09, 969922 (T1), 81fd7c (T2/T2d), 45bedb (K6); built by 261110-211220-b3b9af. T3 and T4 have no main baseline (main has no catalog) |
| A4 | macOS warm, one strict job per row (D59), at strata:dbfc1898: T1 261110-211221-3a7f09, 261110-211223-969922; T2 261110-211226-81fd7c; T3 261110-211228-c5746c (50,000, 25x the open cap); T4 261110-211229-6ec5bb, 261110-211231-026a08 (20,000). Linux and full scale: Milestone checks |
| A5 | macOS warm: T2d rows of 261110-211226-81fd7c (defer_io warm/cold/hot, promote() per segment, d1s1 d1000s1 d1000s10). Dropped and Linux: Milestone checks |
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test passed at strata:5e06300b (261110-164227-8f0e42), strata:85dd1df6 (261110-165622-0ff5b1) and strata:c58a962d (261110-170310-3af67b); head: 261110-175933-b257b3 |
| A7 | 261110-211214-179f5f: `apply_atomic_test` with each damaged section eager and lazy (strata:f2caed00) and `tools/mutants/rollback.py`'s three reverts of the rollback; in build-test (261110-211233-3c01fa). Earlier 261110-164238-c8f75e failed on the probe's oracle (see Doing) |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test as A6 |
| A9 | 261110-211225-45bedb: `bench/idle_index/k6.sh measure` 1,000 indexes, 2 pairs, main interleaved with hot/warm/lazy on d1s1 d100s1 d100s10 |
| A10 | `docs/status/cold-tier.md` "K7: who builds snapshots from `seg()`" (strata:5e06300b, 85dd1df6, 8f372200), with what prospect's tests expect under lazy segments; 261110-164333-5b4cd8 (prospect at its pin, lazy: 48 of 51, the 3 listed) |
| A12 | lazy is the default (strata:6d4e1164); build-test 261110-211233-3c01fa; the whole suite eager (`STRATA_EAGER_SEGMENTS=1`) in 261110-211214-179f5f |

Examined: 261110-164701-5446c4, 261110-164703-b52661 (cancelled: `$JOB_REF_MAIN` unexpanded
without a shell), 261110-164726-ca2d5c (cancelled: replaced by the interleaved f0141f),
261110-165548-809d01 and 261110-165819-a33792 (cancelled: build-test superseded by a newer head).
261110-165809-f0141f and the 14 catalog_tiers strict jobs 8e474b to 151048 (cancelled under D59);
the jobs at 955343df, 9ef55ee5 and 338e72c2 (superseded: see Doing).

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
- A1: T5's two catalogs on one budget (`stress_two`) use 64 hard descriptors, not the design's
  12: `resource_budget` refuses a hard descriptor cap below `min_indexes` (64). Maps 16, soft
  open 4 per catalog (8 together), sweep at the fast tweak. The storage-budget check after every
  close is not repeated there (item 2's tests hold it). A run repeats (up to eight, at least 3,000
  ops) until it meets the maps cap with an overload and a merge; a wake failure is reported, not
  required: T5 names the caps, and `catalog_wake_test` holds the wake's backoff deterministically.
- A2: the four TSan survivors are judged on a TSan build (a16dbb), not by tightening the asserts
  build's tests: a race without a wrong value is visible to TSan only.
- A7: the probe's cases live in `apply_atomic_test` (build-test), not as a separate probe.
- D59 scaling: T3 keeps "far above the caps" by scaling the caps with the count (50,000 indexes,
  25x soft_open, 100x soft_hot); T4 keeps the model's ratio of cache to population (5%, as 50,000
  of 1,000,000) on 20,000. T1 drops d100s1 from its jobs (K6's A9 job covers it). soft_hot is
  250 of soft_open 1,000 (T4) and 500 of 2,000 (T3), not the full scale's 1 in 10.
- A4: cold is demote then close: a close needs the slot's maps at zero, the sweep's order.
- A4: a one-document index has one segment, so the 10-segment fixtures are d100s10 and d1000s10
  (T1), and T2's "1 document, 10 segments" cell is d1000s10's only.
- A4 T3 (macOS): `kern.num_vnodes` is the system's vnode cache, at `kern.maxvnodes` (263,168)
  here even at idle, so the row compares this process's vnodes (distinct files behind its
  descriptors and mapped regions, `proc_pidinfo`) with the budget's hard cap, and reports the
  system's. The T3 job is strict, as the lead asked, not exclusive.
- A4: the top-level `CMakeLists.txt` is outside the file set, so `bench/catalog_tiers` is its own
  CMake project over strata's (`bench/catalog_tiers/CMakeLists.txt`) and CI does not compile it;
  main's `idle_index` builds the fixtures. The jobs first queued at strata:0c054df1 (which touched
  `CMakeLists.txt`) were cancelled before they ran and resubmitted at strata:c71d904f (tiers.sh now builds in fresh directories: a build directory from another job's checkout fails CMake).
- A4: `idle_index`'s meters and heap counter move to `bench/idle_index/meters.hh` and
  `heap_count.hh` (no change in what it prints), so both harnesses meter alike.
