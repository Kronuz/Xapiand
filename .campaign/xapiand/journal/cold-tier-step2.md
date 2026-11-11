# cold-tier-step2: journal

State: waiting on jobs 261110-170310-3af67b 261110-164238-c8f75e 261110-165809-f0141f 261110-170249-bc3fdd
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head strata:c58a962d. The code criteria (A6, A8, A12) passed in build-test at strata:5e06300b and
strata:85dd1df6; the head's check is 3af67b (since then only `tools/`, `bench/` and docs changed).
The lead's rulings (Xapiand:de5b50841) are folded in: lazy stays the default, the K7 listing says
what prospect's tests must expect instead (strata:8f372200); K6 now interleaves main's harness with
this tree's rep by rep and is A3's baseline (f0141f); c8f75e is A7's evidence. A2's mutants are
ported (strata:c58a962d) and running (bc3fdd).

## Next

1. Read the jobs: 3af67b (build-test at the head), c8f75e (A7; the suite eager), f0141f (strict:
   A3 and A9; write the rows into `docs/perf/idle-index.md` with the reps' spread), bc3fdd (A2:
   catalog_test 1,000 seeds, then 167 mutants; compare survivors with the item ledgers' equivalents
   in `docs/status/cold-tier-step2.md`, e.g. item 1's U2d and C3, item 3's SZ1; the S5 and S7
   `stress` kinds were TSan mutants, so in a rel build they may survive; record each).
2. A4, A5: the T1 to T4 and T2d harness, `bench/catalog_tiers/` (new; nothing exists): a
   `catalog<kv_index, H>` over PosixIO directories built by `idle_index build`, a
   `work_pool_executor` and a timer thread (as `test/catalog_world.hh` does over faultfs), with:
   - `tier hot|warm|cold N`: open N through request pins; warm by `begin_demote` under `defer_io`
     (slot `owed`); cold by close (no slot). Meter as `idle_index` does (T1, with and without
     `defer_io`).
   - `first N`: one `get` per index in random order through `pin_request`, p50 and p99 (T2); with
     `defer_io` warm, the wake's `promote()` on the pool then the `get`, against cold's open plus
     `get` (T2d), 1 and 10 segments; cache dropped on Linux only (`echo 3 > drop_caches` in the VM;
     macOS's `purge` needs root).
   - `count 1000000` (T3: 8 threads, every index once then 1M Zipf at 0.99, 1 Hz samples of maps
     and descriptors, zero `ENFILE`/`EMFILE`/`ENOMEM`/`too_many_open`) and `zipf θ` (T4: hit
     ratio with two pins per request against `s8_variants`' model at the same capacity).
   Linux runs in the local VM (D24; T3 needs 12 to 16 GB); jobs as `strict` (exclusive on macOS
   for T3).
3. A1: T5's step-2 counters from strata CI's TSan and ASan+UBSan logs (the lead's ruling); today
   `catalog_stress_test` reports, not requires, pins meeting each transitional state and reposts,
   and does not run T5's two-catalogs-on-one-budget shape at 8 open, 12 descriptors, 16 maps:
   extend it so CI's run requires each counter, then cite the run.
4. A11: CI on `quest/xapiand/cold-tier-step2` at the head.

## Blocked on

Nothing. (Ruled 2026-11-10, Xapiand:de5b50841: lazy stays the default and prospect's test changes
are its pin bump's follow-up; A3's baseline is main's harness in the same strict job; c8f75e is A7's
evidence.)

## Evidence

| Criterion | Evidence |
|---|---|
| A2 | 261110-170249-bc3fdd (running): `tools/mutants/cold_tier.py build-mut 1000` over `tools/mutants/cold_tier_table.py` (S3 21, S4 34, S5/S6 31, S7 81) |
| A3 | 261110-165809-f0141f (strict, queued): main strata:15d39c30's `idle_index`, interleaved with this tree's, built by 261110-164724-95e365 |
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test passed at strata:5e06300b (261110-164227-8f0e42) and strata:85dd1df6 (261110-165622-0ff5b1); head: 261110-170310-3af67b |
| A7 | 261110-164238-c8f75e (running): `tools/mutants/rollback.py` over `apply_atomic_test` and `tools/mutants/apply_throw_probe.cc`, three reverts of the rollback |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test as A6 |
| A9 | 261110-165809-f0141f (as A3) |
| A10 | `docs/status/cold-tier.md` "K7: who builds snapshots from `seg()`" (strata:5e06300b, 85dd1df6, 8f372200), with what prospect's tests expect under lazy segments; 261110-164333-5b4cd8 (prospect at its pin, lazy: 48 of 51, the 3 listed) |
| A12 | lazy is the default (strata:6d4e1164); build-test as A6; eager whole run in 261110-164238-c8f75e |

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
