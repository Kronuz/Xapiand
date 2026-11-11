# cold-tier-step2: journal

State: waiting on jobs 261110-165622-0ff5b1 261110-164238-c8f75e 261110-164726-ca2d5c
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code criteria done (A6, A8, A12) and passing in build-test 261110-164227-8f0e42 (73 of 73, lazy
default) at strata:5e06300b; the same check at the head strata:85dd1df6 (adds the K6 harness and docs) is
261110-165622-0ff5b1. A7's recheck c8f75e and A9's strict ca2d5c are queued; A9's fixtures are
built (95e365). A10's listing is written; prospect's suite under lazy segments fails 3 of 51
(below), which blocks the lazy default's pin bump in prospect, not this branch.

## Next

1. Read the jobs: build-test 809d01 at the head; K3 c8f75e (A7; also the whole suite eager,
   `STRATA_EAGER_SEGMENTS=1`); K6 strict ca2d5c (A9): write its rows into `docs/perf/idle-index.md`
   and judge "hot within 5% of main" with the reps' spread.
2. A1: T5's step-2 counters under TSan and ASan+UBSan: `catalog_stress_test` prints them but only
   reports (does not require) pins meeting each transitional state and reposts; a local sanitizer
   job with more ops, or CI's log. T5's step-2 shape (two catalogs on one budget at 8 open, 12
   descriptors, 16 maps, sweep at 1 kHz) is not what the stress runs today: check before claiming.
3. A2: `catalog_test` 1,000 seeds (`catalog_test 1000`) and the S3 to S7 mutants
   (`.scratch/mutants{3,4,5}.py` of the item worktrees, not on main; port them to `tools/mutants/`).
4. A4, A5: T1 to T4 and T2d need harnesses that do not exist (tier through the catalog, 1M
   indexes, Zipf, wake cost on `work_pool_executor`), and the Linux VM. A3's baselines first (the
   lead's).
5. A11: CI on `quest/xapiand/cold-tier-step2`, runs at the head strata:85dd1df6 (42933999804 at strata:2a404927;
   42933456525 at strata:5e06300b was cancelled by the next push).

## Blocked on

- The lead: prospect's tests at its pin with lazy segments (261110-164333-5b4cd8, prospect
  e8e5bd9 at strata:3df7af4f, `STRATA_EAGER_SEGMENTS=0`) fail 3 of 51: `format13_test.cc:595`,
  `:670`, `:777` and `index_test.cc:1427`, `:1823` (x5, an injected `EIO` at open), `:2093` (x2,
  "another index's segment of the same size", "a resealed document count"). Each expects the open
  to find segment damage, which a lazy open leaves to the first map (D11), as strata's own
  `index_test` cases did before they set `eager_segments` (`test/index_test.cc:1302`, `:1693`,
  `:1976`). No library failure. Options: (a, recommended) prospect's next strata pin bump sets
  `eager_segments = true` in those cases, a prospect change in its own quest or in
  manifest-log-prospect; (b) keep eager as strata's default until then (A12 waits). This branch's
  default stays lazy; a prospect pin past it fails those cases until (a).

- The lead: A3's re-measured idle-index baselines (C2) on main, before T1 to T4 and T2d gate.
  A9's job runs main's harness at strata:15d39c30 beside this branch's for its own "hot within 5%
  of today"; say whether that serves as K6's baseline or A3's must replace it.
- The lead: is A7's job the "rechecker with a build", or should an independent rechecker rerun
  `tools/mutants/rollback.py`?

## Evidence

| Criterion | Evidence |
|---|---|
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test 261110-164227-8f0e42 passed (catalog_wake) at strata:5e06300b; 261110-165622-0ff5b1 at the head |
| A7 | 261110-164238-c8f75e (queued): `tools/mutants/rollback.py` over `apply_atomic_test` and `tools/mutants/apply_throw_probe.cc`, three reverts of the rollback |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test 261110-164227-8f0e42 passed (cold_segment) at strata:5e06300b; 261110-165622-0ff5b1 at the head |
| A9 | 261110-164726-ca2d5c (strict, queued; fixtures and both harnesses built by 261110-164724-95e365) |
| A10 | `docs/status/cold-tier.md` "K7: who builds snapshots from `seg()`" (strata:5e06300b, strata:85dd1df6); prospect lazy suite 261110-164333-5b4cd8 failed 3 of 51 (Blocked on) |
| A12 | lazy is the default (strata:6d4e1164); build-test 261110-164227-8f0e42 passed 73 of 73 at strata:5e06300b with it; 261110-165622-0ff5b1 at the head; eager whole run in 261110-164238-c8f75e (queued) |

Examined: 261110-164701-5446c4 and 261110-164703-b52661, cancelled (the `$JOB_REF_MAIN` argument
was not expanded without a shell; resubmitted through `bash -c`).

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
- A10: the two pin bumps that adopt `seg()` are on file (prospect:e49453d, Xapiand:74d8236e5);
  no new bump here. prospect's suite runs at its own pin with `STRATA_EAGER_SEGMENTS=0` to stand
  for the next bump: prospect main pins strata:3df7af4f, before the manifest log, and
  manifest-log-prospect carries its move to strata main.
  Xapiand's suite is not run lazy: a host forces eager by `defer_io` (`shard.cc:170`).
- `docs/status/cold-tier.md` conflicts with `design/cold-tier` at its header (it does on main
  too); the K7 section sits between K8 and Log, which both sides leave alone.
