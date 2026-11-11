# cold-tier-step2: journal

State: waiting on jobs 261111-151520-66533e 261111-151526-29ff8a 261111-151532-1f0913 261111-151536-71d322 261111-151540-9ba721 261111-151543-9397aa 261111-151547-259305 261111-151551-cd5253
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead's rulings of 11-11 on F1/F2, performance assignment. Head strata:a5399d7a (code at 7b126bc1).

- T3 (F1): cause measured (5b7a23, strata:f3f7d9c2, Zipf medians): 0 slots demoting/closing, net
  open 2,043 and hot 572 (= the counts); passes 19/s of which 1 started (the timer), 18 reposts.
  The catalog's own soft caps had no trigger; a timed pass's repost chain ends once relief covers
  the excess, then a second of misses (112/s) and maps (74 wakes/s) piles up. Fix kept,
  strata:5d3c948a (`over_soft()` on a miss and a last unpin): 358d4c, 0 of 278 samples over
  soft+16 after the touch, max open 2,012 hot 512, within from the first sample; passes 41,969
  vs 11,584 per run. Suite 7882c4 green at 5d3c948a (74/74); mutant ade11c killed (2 CHECKs in
  `soft_caps_post`). Zipf 311 vs 375 req/s, p50 391 vs 175 µs in single runs, unattributed.
- Stuck `pass_queued_` on a throwing post: fixed at strata:7b126bc1, test `pass_post_throws`
  (flaky executor). Suite 261111-123742-b13378, mutant 261111-123743-7732e9 (catalog.hh from
  5d3c948a; exit 0 only on line 611's CHECK failing).
- T4 reruns at 7b126bc1 (the trigger changes eviction timing): build 261111-123757-b065fc, then
  261111-123758-7ea903 (0.99) and 261111-123800-7a33ff (1.2).
- Verdicts written (strata:fa7f9339): T3 soft caps MET at 5d3c948a; T2 hot d1s1 MISS beyond A/A
  (6d8a9c: p50 1.5 vs 0.9 µs, +71% median, ranges 1.4-2.2 vs 0.7-1.2 µs; p99 +69%); T1 warm heap
  MISS (6aaa2d: listed pieces 3,651 B +7% = 3,907 B vs 5,022 B; unlisted 1,360 B: manifest and file
  maps 544, view's own allocations 256, host IO 256, merge machinery 224, ctor 80); T2 cold
  rejudged (472ec032). `docs/mechanisms/catalog.md` names who posts a pass (a5399d7a).

## Next

0. Lead's question 11-11 (after 418363): the 19.4/19.7 ms p50s are the one-slot trigger's, via the
   median's step; the trigger costs throughput (strata:ed9102bd, `docs/perf/catalog-tiers.md` "The
   trigger's cost, an A/B"). Four one-pair exclusive jobs ac195f, 7e33e6, 071676, 141fc0: on
   253/216/256/303 vs off 303/276/320/416 req/s (0.73-0.83 of off, 4 of 4); device slow in every run
   (probe p50 52-63 ms); waited share on 58-60% vs off 47-59%; passes ~26-29k vs ~7k; opens,
   demotes and closes equal. 7430a0's earlier "on faster" read came from a job that timed out, so it
   is not evidence and was withdrawn.
   Experiment (strata:062182b2): post at a full pass's excess (`pass_posts`). Suite 66533e, mutant
   29ff8a (7b126bc1's catalog.hh fails `soft_caps_post` line 550), build 1f0913, pairs 71d322
   9ba721 9397aa 259305, T3 cd5253 (358d4c's command). If within off's spread and T3 0 samples over
   soft+16: keep, update `docs/mechanisms/catalog.md` (who posts a pass) and cite for A1/A4. Else:
   revert to 7b126bc1's trigger and bring the lead the measured residual (T3 met vs ~20% Zipf).
1. Read 261111-124104-418363 (`tiers.sh t3ab`, strata:63dc1c18: on = 7b126bc1's binary from
   b065fc, off = f3f7d9c2's from 261111-124103-7e9274; 50,000 indexes, 12,000 Zipf, 4 pairs
   alternating). Zipf req/s per run: if on's mean is below off's beyond the larger of the two
   sides' own spreads, coalesce the trigger (post only on the net-over crossing edge, or at most one
   queued pass per tick), rerun T3 (358d4c's command) and this A/B.
2. Read b13378 (suite) and 7732e9 (mutant) for `pass_post_throws`; read the T4 reruns 7ea903 and
   7a33ff and write T4's verdict at 7b126bc1.

## Follow-up (review findings, not fixed here)

- F3: `promote_probe`'s 90-116 µs exceeds the whole warm-defer request (47-85 µs) that contains
  `promote()`; the doc still quotes the probe against D3's estimate. Say the end-to-end wake
  bounds it, or find what the probe adds.
- F4: D3's "macOS's map is unmeasured" and "T2d decides the latency" (`docs/design/cold-tier.md`
  :198-201) want T2d's macOS result; T2d reruns if the sync deferral lands.
- F6: `test/catalog_stress_test.cc` fails the run when a timing-dependent counter misses in eight
  repeats; CI flake risk. Keep, or report and require the counters from the deterministic tests.
- F7: T2d at 500 indexes keeps every file cached; the full-scale milestone row settles it.

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
| A1 | `soft_caps_post` (strata:5d3c948a: build-test 261111-113837-7882c4, mutant killed 261111-113839-ade11c), `pass_post_throws` (strata:7b126bc1: 261111-123742-b13378, mutant 261111-123743-7732e9); the band check in `soft_caps_post` (strata:062182b2: suite 261111-151520-66533e, mutant 261111-151526-29ff8a, pending); `test/catalog_stress_test.cc`: T5's counters required over the run (strata:d5b9fe05) and two catalogs on one budget, `stress_two` (strata:955343df, 9ef55ee5); build-test 261111-034631-5cd30b at strata:9621595f; CI run 42993976189 at strata:948c0869: `ci:linux-tsan`, `ci:linux-asan-ubsan` green |
| A2 | 261111-035757-eb666b: `tools/mutants/cold_tier.py build-mut 1000`, 162 of 162 killed; 261111-035757-1edccb: the TSan-only S4-S3, S4-X2, S7-T1, S7-T2 and S7-T3, 5 of 5 killed; S4-SZ1 refused at compile time (`test/catalog_test.cc` static_assert) |
| A3 | main strata:15d39c30's `idle_index` interleaved in each row's strict job at strata:9621595f: 261111-035757-efe281, 261111-035757-083bc8 (T1), 261111-085642-fdea98, 261111-085644-609714, 261111-035757-f9dbcf (T2/T2d), 261111-035757-a253be (K6). T3 and T4 have no main baseline (main has no catalog). `docs/perf/catalog-tiers.md` |
| A4 | met except: T1 cold (292 B against 16 B), T2 hot d1s1 (p50 +56% and +40% in 2 of 3 pairs), T2 hot d1s1 beyond its A/A spread (261111-113118-6d8a9c, +71% p50), T2 cold d1000s10 p99 (+13.9%, inside main's own spread; same-mode pairs only), T1 warm heap (261111-113121-6aaa2d: 5,022 B against the listed pieces plus 7%, 3,907 B). T3 soft caps MET at strata:5d3c948a (261111-113856-358d4c; cause 261111-113116-5b7a23). The one-slot trigger's Zipf throughput is a MISS: 0.73-0.83 of off in 261111-143023-ac195f, 261111-143025-7e33e6, 261111-143034-071676, 261111-143035-141fc0 (agrees with 261111-124104-418363, 3 of 4). The band trigger (strata:062182b2) is pending: 261111-151536-71d322, 261111-151540-9ba721, 261111-151543-9397aa, 261111-151547-259305, T3 261111-151551-cd5253. macOS warm, one strict job per row (D59) at strata:9621595f: T1 261111-035757-efe281, 261111-035757-083bc8; T2 261111-085642-fdea98, 261111-085644-609714, 261111-035757-f9dbcf; T3 261111-034629-8f5208; T4 261111-035757-a2d41c, 261111-035757-bbb067. Verdicts in `docs/perf/catalog-tiers.md`. Linux and full scale: Milestone checks |
| A5 | macOS warm: T2d warm defer below cold at p50 and p99 on every fixture, no interval overlapping: 261111-085642-fdea98 (d1s1: 47-85 against 264-20,946 µs at p50), 261111-085644-609714 (d1000s1: 54-71 against 379-41,137), 261111-035757-f9dbcf (d1000s10: 351-394 against 1,097-21,908); p99 108-799 against 42,224-72,264. Dropped and Linux: Milestone checks |
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test passed at strata:5e06300b (261110-164227-8f0e42), strata:85dd1df6 (261110-165622-0ff5b1) and strata:c58a962d (261110-170310-3af67b); head: 261110-175933-b257b3 |
| A7 | 261111-035757-bf4776: `apply_atomic_test` with each damaged section eager and lazy, `tools/mutants/rollback.py`'s three reverts killed; in build-test 261111-034631-5cd30b |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test 261111-034631-5cd30b at strata:9621595f |
| A9 | 261111-035757-a253be: `bench/idle_index/k6.sh measure` 1,000 indexes, 2 pairs: hot's heap equals main's, RSS -0.0% to +3.3%; warm and lazy rows in `docs/perf/catalog-tiers.md` |
| A10 | [artifact] `docs/status/cold-tier.md` section "K7: who builds snapshots from seg()" (strata:5e06300b, 85dd1df6, 8f372200): prospect's and Xapiand's construction sites with the pin bumps that adopt seg() (prospect:e49453d, Xapiand:74d8236e5), and the six prospect cases a lazy open leaves to the first map |
| A11 | CI run 42993976189 at strata:948c0869: every job green (`ci:linux-tsan`, `ci:linux-asan-ubsan`, `ci:freebsd-x86_64`, `ci:macos-arm64`, `ci:macos-x86_64`, `ci:linux-arm64-gcc`, `ci:linux-x86_64-gcc`, `ci:linux-x86_64-clang`) |
| A12 | lazy is the default (strata:6d4e1164); build-test 261111-034631-5cd30b; the whole suite eager (`STRATA_EAGER_SEGMENTS=1`) in 261111-035757-bf4776 |
| A13 | [artifact] `docs/design/cold-tier.md` revision 8 (strata:19ea55d9, over design/cold-tier's revision 7 merged at 6f8884bc): item 5b's host and word rows in D4, the ledger's "Design vs code" in D4, fence replacement, commit chore and create intent in D5, the limits |
| A14 | [artifact] `docs/status/cold-tier-step2.md`: Q6 ruled (`docs/design/cold-tier.md` Q6), item 6's seam ruling applied ("Item 6's seam ruling"), the last questions answered or left to the lead ("Questions for the lead and the architect", strata:7a5ca76a: question 3's by-key A/B is the lead's measurement) |

Examined: 261110-164333-5b4cd8 (prospect's suite at its pin, lazy for the whole run: 48 of 51,
exit 8; the three failures are the six cases K7 lists for prospect's pin bump, so it informs A10
and proves nothing by passing). 261111-035757-23475b (timeout: split, see Doing).
261110-211226-81fd7c, 261110-211221-3a7f09, 261111-011618-0d4ed0, 261111-011622-bbbac4,
261110-211225-45bedb, 261110-211229-6ec5bb, 261110-211231-026a08, 261111-010253-46fd89,
261111-010255-f16b39, 261111-010257-bace72, 261111-011626-b1b43d (superseded by the reruns at
9621595f). 261110-164701-5446c4, 261110-164703-b52661 (cancelled: `$JOB_REF_MAIN` unexpanded
without a shell), 261110-164726-ca2d5c (cancelled: replaced by the interleaved f0141f),
261110-165548-809d01 and 261110-165819-a33792 (cancelled: build-test superseded by a newer head).
261110-165809-f0141f and the 14 catalog_tiers strict jobs 8e474b to 151048 (cancelled under D59);
the jobs at 955343df, 9ef55ee5 and 338e72c2 (superseded: see Doing).

## Result

strata `quest/xapiand/cold-tier-step2` at 58812244: T5's counters and two catalogs on one budget
(`test/catalog_stress_test.cc`), the S3-S7 mutants in `tools/mutants/cold_tier.py`, the tier
harness `bench/catalog_tiers` with its rows in `docs/perf/catalog-tiers.md`, lazy segments the
default, `docs/design/cold-tier.md` revision 8, `docs/status/cold-tier.md` (K7) and
`docs/status/cold-tier-step2.md`. Linux, full-scale and dropped-cache rows are Milestone checks.

## Rulings I made

- A4 trigger A/B: one pair per exclusive job (7430a0's three pairs passed 10 minutes in the slow
  flush mode); jobs 3 and 4 carry `--env T3AB_PAIR=N` so they are not deduplicated against 1 and 2.
- A4 trigger A/B: the one experiment is a band trigger: post once the excess reaches a full
  pass (`pass_posts`, 16). T3's bound is "within a pass of the caps", so the band keeps T3's bar
  while cutting the passes, which are the measured difference (opens, demotes and closes are equal).

- F1: the soft-cap trigger is a defect fix inside the sweep (the budget's caps already post at
  their crossing); kept on 358d4c's result. Its check is net of relief in flight, so a pass is
  not posted for an excess already covered.
- T1 warm: sites mapped to the design's pieces by their frames; demote()'s 32 B and 16 B counted
  as the record's bits and control block, the key table's 14 B with the slot's share.
- T2 hot A/B ran at strata:f3f7d9c2, before the trigger; the trigger adds two loads to a last
  unpin only while the sweep runs (T2 does not start it), so it is not rerun.

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
- A4 T3: the sampler's descriptor gap is its own skew (strata:3177e491, `fd-budget.md`); it reads
  the budget before and after the kernel's count and reports the excess over the larger.
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
- A9: K6's job a253be at strata:9621595f measured `.scratch/k6`'s binaries built at strata:dbfc1898
  (k6.sh prints that commit). Their only code diff to 9621595f is `reader.hh`'s stored frame
  decode and check (main's e5979526, 71750940), which K6's open-then-meter never reaches, so the
  row stands at the head; not rerun (the lead: finished results stand).
- A3 to A5: the brief's `campaign job rerun 261110-211226-81fd7c` is met by splitting it (23475b,
  its rerun, timed out at 10 minutes): one strict job per fixture, d1000s10 already f9dbcf's.
