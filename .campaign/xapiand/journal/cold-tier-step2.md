# cold-tier-step2: journal

State: waiting on jobs 261110-182811-215b45 261110-182822-35997d 261110-182844-f6c165 261110-182903-8aa475 261110-175937-8e474b 261110-175939-b2914a 261110-175941-16d5ff 261110-175943-bda5ff 261110-175945-30012f 261110-175947-0cb6ed 261110-175948-1088d2 261110-175950-458409 261110-175952-21dca2 261110-175954-748b38 261110-175956-b4c8bd 261110-175958-6bd160 261110-180000-6b5071 261110-180002-151048 261110-165809-f0141f
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head strata:955343df (pushed). Since c71d904f: A7's probe folded into `apply_atomic_test`
(strata:f2caed00), the A2 survivors' tests tightened (strata:88d18147), T5's two catalogs on one
budget in `catalog_stress_test` (strata:955343df). `bench/` and `include/` are unchanged since
c71d904f, so the catalog_tiers jobs queued there (build 45fe51 done) measure this head's code.

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
  - S4-S3, S4-X2, S7-T1, S7-T3: data races, killed only by TSan (their ledger rows say so); the
    asserts build of bc3fdd cannot see them. Judged by f6c165 on a TSan build; S7-T3 counts as
    killed when one of up to four runs kills it (it races about one run in four).
  - S4-SZ1: dropped, uncompilable: its edit fails `catalog_test.cc:1057`'s
    `static_assert(slot_bytes() <= 192)`.
- A1: `stress_two` (plain and defer_io) runs locally in 8 s total with the rest:
  maps at 16 of 16, descriptors at most 31 of 64, 0 kernel refusals, 0 over a cap, 75 and 79
  merges, 6 wake failures under defer_io.

## Next

1. Read the jobs. 8aa475: build-test at strata:955343df. CI on the pushed head: A1 (`linux-tsan`,
   `linux-asan-ubsan`: `catalog_stress` with the two-catalog phase). 215b45 (A7, K3, A12 eager
   run). 35997d (A2, all mutants, 1,000 seeds; expect every one but the four TSan ones killed)
   and f6c165 (the four on TSan).
2. The catalog_tiers rows as before: 8e474b (smoke) then T1 b2914a (d1s1), 16d5ff (d100s1),
   bda5ff (d1000s1), 30012f (d100s10), 0cb6ed (d1000s10); T2/T2d 1088d2 (d1s1), 458409 (d1000s1),
   21dca2 (d1000s10); T3 748b38; T4 b4c8bd (0.99), 6bd160 (0.99 scan), 6b5071 (1.2), 151048 (1.2
   scan). Each row, with main's interleaved baseline, into strata `docs/perf/catalog-tiers.md`
   "Results"; A5's sentence for macOS warm. A job ending 124: split it (t1 by reps, t4 with
   `WARM REQUESTS` halved twice) and resubmit. f0141f (A3, A9): K6's rows into
   `docs/perf/idle-index.md` with the reps' spread.
3. Delete `.scratch/tiers` (about 20 GB of fixtures) once the rows are in.

## Blocked on

Nothing.

## Milestone checks

Run once on main, not gates of this quest (D57: platforms are CI's; no Linux host, no AWS, the
qemu VMs on 18762/18763 are Detent's).

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
| A1 | `test/catalog_stress_test.cc`: T5's counters required over the run (strata:d5b9fe05) and two catalogs on one budget, `stress_two` (strata:955343df); build-test 261110-182903-8aa475 and CI at strata:955343df: `linux-tsan`, `linux-asan-ubsan` |
| A2 | 261110-182822-35997d: `tools/mutants/cold_tier.py build-mut 1000` over `tools/mutants/cold_tier_table.py` at strata:955343df (S4-S2, S5-B9, S7-Z8 tightened); 261110-182844-f6c165: S4-S3, S4-X2, S7-T1, S7-T3 on TSan; S4-SZ1 dropped (uncompilable, see Doing); earlier 261110-170249-bc3fdd (7 survivors) |
| A3 | 261110-165809-f0141f (strict, queued): main strata:15d39c30's `idle_index`, interleaved with this tree's, built by 261110-164724-95e365 |
| A4 | macOS, cache warm (strict, queued at strata:c71d904f; `bench/` and `include/` unchanged to 955343df): T1 b2914a 16d5ff bda5ff 30012f 0cb6ed; T2 1088d2 458409 21dca2; T3 748b38; T4 b4c8bd 6bd160 6b5071 151048. Linux: Milestone checks |
| A5 | macOS cache warm: T2d rows of 1088d2 458409 21dca2 (defer_io warm/cold/hot, promote() per segment). Dropped and Linux: Milestone checks |
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test passed at strata:5e06300b (261110-164227-8f0e42), strata:85dd1df6 (261110-165622-0ff5b1) and strata:c58a962d (261110-170310-3af67b); head: 261110-175933-b257b3 |
| A7 | 261110-182811-215b45: `apply_atomic_test` with each damaged section eager and lazy (strata:f2caed00) and `tools/mutants/rollback.py`'s three reverts of the rollback; in build-test (8aa475). Earlier 261110-164238-c8f75e failed on the probe's oracle (see Doing) |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test as A6 |
| A9 | 261110-165809-f0141f (as A3) |
| A10 | `docs/status/cold-tier.md` "K7: who builds snapshots from `seg()`" (strata:5e06300b, 85dd1df6, 8f372200), with what prospect's tests expect under lazy segments; 261110-164333-5b4cd8 (prospect at its pin, lazy: 48 of 51, the 3 listed) |
| A12 | lazy is the default (strata:6d4e1164); build-test 261110-182903-8aa475; the whole suite eager (`STRATA_EAGER_SEGMENTS=1`) in 261110-182811-215b45 |

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
- A1: T5's two catalogs on one budget (`stress_two`) use 64 hard descriptors, not the design's
  12: `resource_budget` refuses a hard descriptor cap below `min_indexes` (64). Maps 16, soft
  open 4 per catalog (8 together), sweep at the fast tweak. The storage-budget check after every
  close is not repeated there (item 2's tests hold it).
- A2: the four TSan survivors are judged on a TSan build (f6c165), not by tightening the asserts
  build's tests: a race without a wrong value is visible to TSan only.
- A7: the probe's cases live in `apply_atomic_test` (build-test), not as a separate probe.
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
