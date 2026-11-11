# cold-tier-step2: journal

State: waiting on jobs 261111-010253-46fd89 261111-010255-f16b39 261111-010257-bace72 261111-011615-c7bbb1 261111-011618-0d4ed0 261111-011622-bbbac4 261111-011626-b1b43d
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head strata:948c0869 (not pushed: the lead pushes). Since dbfc1898: main merged (strata:5e7a7b47,
the stored check); the TSan-only mutants marked (strata:8875d17d); `catalog_s8` scaled under TSan
(strata:9bf6b833); the tier fixtures rebuilt by this tree's `idle_index` (strata:2a2ec768, 948c0869);
the macOS rows written (strata:60b7d781); design/cold-tier merged and `cold-tier.md` revision 8
(strata:6f8884bc, 19ea55d9); the ledger's last questions (strata:7a5ca76a). After 9bf6b833 only
`docs/` and `bench/` change, so the jobs at 9bf6b833 (46fd89, f16b39, bace72) test this head's
code.

- Results at dbfc1898: build-test 261110-211233-3c01fa passed; A7 261110-211214-179f5f passed
  (the section cases, the three reverts killed, the whole suite eager 73 of 73); A2 TSan
  261110-211218-6a6200 killed S4-S3, S4-X2, S7-T1 and S7-T3. A2 at 1,000 seeds
  261110-211216-248ffb: 162 of 167 killed; survivors S4-S3, S4-X2, S7-T1 (TSan-only, killed in
  6a6200) and S7-T2 (shutdown's failed commits named without the mutex: killed in bc3fdd's run,
  survived here, a race with no wrong value); S4-SZ1 not run (it does not compile). Fixed in
  strata:8875d17d: the four races are marked `tsan` and run only on a TSan build (bace72 runs them
  with S7-T3); S4-SZ1 leaves the table (the static_assert refuses it, as the ledger says).
- CI at dbfc1898: `linux-tsan` was not superseded. It hit the job's 60-minute limit inside
  `catalog_s8_smoke`, which ran 43 minutes before the cancel. Main's last three CI runs (42979496571,
  42951960800, 42931264451) end the same way, so it is pre-existing. Fixed in my file set
  (strata:9bf6b833): under TSan the smoke divides by 10 more; locally the TSan build runs it in
  22 s and passes. Every other job at dbfc1898 passed. At 9ef55ee5, `linux-tsan` also failed
  `resource_budget_test.cc:494` (`b.external(D) <= 70`) once; that test and `resource_budget.hh`
  are main's, unchanged here, and passed at dbfc1898 and in main's runs: a flake for the lead.
- The rows (written in strata `docs/perf/catalog-tiers.md` "Results"): T1 hot through the catalog
  +650 B heap over main (the slot and payload), cold 292 B (T1's "cold ≤ 16 B" is not met at step
  2); K6 hot equals main's heap, RSS -2.0% to +1.4%; T2d warm below cold at p50 and p99 by about
  300 times; T3 PASS at 50,000; T4 PASS in all four cells, each above s8_variants. The 10-segment
  rows were void: main's `idle_index` ignores SEGMENTS, so the s10 fixtures held one segment;
  rerun on rebuilt fixtures as 0d4ed0 (T1) and bbbac4 (T2/T2d), behind c7bbb1.
- Findings for the lead: an open is 21 ms at p50 on this Mac in most pairs, about 1 ms in a few
  (the two syncs); T3 saw the kernel hold up to 1,050 descriptors beyond the budget's count.

## Next

1. Read b1b43d (build-test at the head), 46fd89 (A7, A12), f16b39 (A2 asserts, the four races
   skipped), bace72 (A2 TSan, five). CI once the lead pushes (A11).
2. Read 0d4ed0 and bbbac4 (the 10-segment T1 and T2/T2d rows) into `docs/perf/catalog-tiers.md`,
   replacing the void row's note.
3. Delete `.scratch/tiers` and `.scratch/k6` once the rows are in.

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
| A1 | `test/catalog_stress_test.cc`: T5's counters required over the run (strata:d5b9fe05) and two catalogs on one budget, `stress_two` (strata:955343df, 9ef55ee5); build-test 261111-011626-b1b43d; CI `linux-asan-ubsan` passed at strata:dbfc1898 (run 42964131146), `linux-tsan` on the next push |
| A2 | 261111-010255-f16b39: `tools/mutants/cold_tier.py build-mut 1000` over `tools/mutants/cold_tier_table.py` at strata:9bf6b833 (the TSan-only races skipped); 261111-010257-bace72: S4-S3, S4-X2, S7-T1, S7-T2, S7-T3 on TSan; S4-SZ1 refused at compile time. At dbfc1898: 261110-211216-248ffb (162 of 167), 261110-211218-6a6200 (TSan, 4 of 4) |
| A3 | main strata:15d39c30's `idle_index` interleaved in each row's job: 261110-211221-3a7f09 (T1), 261110-211226-81fd7c (T2/T2d), 261110-211225-45bedb (K6); 10 segments 261111-011618-0d4ed0, 261111-011622-bbbac4. T3 and T4 have no main baseline (main has no catalog). Results: `docs/perf/catalog-tiers.md` |
| A4 | macOS warm, one strict job per row (D59): T1 261110-211221-3a7f09 and 261111-011618-0d4ed0 (10 segments); T2 261110-211226-81fd7c and 261111-011622-bbbac4; T3 261110-211228-c5746c PASS (50,000, 25x the open cap); T4 261110-211229-6ec5bb, 261110-211231-026a08 PASS. Results: `docs/perf/catalog-tiers.md`. Linux and full scale: Milestone checks |
| A5 | macOS warm: T2d in 261110-211226-81fd7c, warm defer below cold at p50 and p99 on d1s1 and d1000s1 (54-90 vs 20,963-21,069 µs p50); 10 segments 261111-011622-bbbac4. Dropped and Linux: Milestone checks |
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test passed at strata:5e06300b (261110-164227-8f0e42), strata:85dd1df6 (261110-165622-0ff5b1) and strata:c58a962d (261110-170310-3af67b); head: 261110-175933-b257b3 |
| A7 | 261110-211214-179f5f (passed at strata:dbfc1898) and 261111-010253-46fd89 (at 9bf6b833): `apply_atomic_test` with each damaged section eager and lazy and `tools/mutants/rollback.py`'s three reverts; in build-test 261111-011626-b1b43d |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test 261110-211233-3c01fa passed at dbfc1898; head 261111-011626-b1b43d |
| A9 | 261110-211225-45bedb: `bench/idle_index/k6.sh measure` 1,000 indexes, 2 pairs: hot's heap equals main's, RSS -2.0% to +1.4%; warm and lazy rows in `docs/perf/catalog-tiers.md` |
| A10 | [artifact] `docs/status/cold-tier.md` "K7: who builds snapshots from `seg()`" (strata:5e06300b, 85dd1df6, 8f372200): prospect's and Xapiand's sites with the pin bumps (prospect:e49453d, Xapiand:74d8236e5); 261110-164333-5b4cd8 ran prospect lazy at its pin (48 of 51, the 3 listed there) |
| A11 | CI after the lead's push. At strata:dbfc1898 (run 42964131146) every job passed but `linux-tsan`, cancelled at 60 minutes in `catalog_s8_smoke` (as on main); scaled in strata:9bf6b833 |
| A12 | lazy is the default (strata:6d4e1164); build-test 261111-011626-b1b43d; the whole suite eager (`STRATA_EAGER_SEGMENTS=1`) passed in 261110-211214-179f5f, again in 261111-010253-46fd89 |
| A13 | [artifact] `docs/design/cold-tier.md` revision 8 (strata:19ea55d9, over design/cold-tier's revision 7 merged at 6f8884bc): item 5b's host and word rows in D4, the ledger's "Design vs code" in D4, fence replacement, commit chore and create intent in D5, the limits |
| A14 | [artifact] `docs/status/cold-tier-step2.md`: Q6 ruled (`cold-tier.md` Q6), item 6's seam ruling applied ("Item 6's seam ruling"), the last questions answered or left to the lead ("Questions for the lead and the architect", strata:7a5ca76a: question 3's by-key A/B is the lead's measurement) |

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
- A2: a race with no wrong value is a `tsan` mutant, judged on a TSan build only; S4-SZ1 is not a
  mutant (its edit does not compile).
- A11: `catalog_s8_smoke` divides by 10 more under TSan (my file set; the hang is on main too).
- A13: design/cold-tier is merged into this branch (docs only) and revision 8 written over it;
  `docs/status/cold-tier.md` keeps this branch's HISTORY header and K3.
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
