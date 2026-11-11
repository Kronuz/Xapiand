# cold-tier-step2: journal

State: waiting on jobs 261110-164227-8f0e42 261110-164238-c8f75e 261110-164333-5b4cd8 261110-164724-95e365 261110-164726-ca2d5c
Quest: .campaign/xapiand/quests/cold-tier-step2.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State, the first step under Next, and the Evidence table. State is one of: working,
waiting on jobs <ids>, ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Code criteria done (A6, A8, A12), A7's recheck and A10's listing written; their jobs queued or
running. A9's harness (idle_index `tier`, segments; `bench/idle_index/k6.sh`) built, its strict
job queued behind the build job.

## Next

1. Read the jobs: build-test 8f0e42 (A6, A8, A12 at the default, now lazy), K3 c8f75e (A7; also
   the whole suite eager, `STRATA_EAGER_SEGMENTS=1`), prospect lazy 5b4cd8 (A10/A12), K6 build
   95e365 then strict ca2d5c (A9). Fix what fails; rerun `campaign verify` at the final head (the
   build-test ran at strata:5e06300b; strata:2a404927 adds only the harness and docs).
2. A1: T5's step-2 counters under TSan and ASan+UBSan: `catalog_stress_test` prints them but only
   reports (does not require) pins meeting each transitional state and reposts; a local sanitizer
   job with more ops, or CI's log. T5's step-2 shape (two catalogs on one budget at 8 open, 12
   descriptors, 16 maps, sweep at 1 kHz) is not what the stress runs today: check before claiming.
3. A2: `catalog_test` 1,000 seeds (`catalog_test 1000`) and the S3 to S7 mutants
   (`.scratch/mutants{3,4,5}.py` of the item worktrees, not on main; port them to `tools/mutants/`).
4. A4, A5: T1 to T4 and T2d need harnesses that do not exist (tier through the catalog, 1M
   indexes, Zipf, wake cost on `work_pool_executor`), and the Linux VM. A3's baselines first (the
   lead's).
5. A11: CI on `quest/xapiand/cold-tier-step2` (pushed at strata:2a404927).

## Blocked on

- The lead: A3's re-measured idle-index baselines (C2) on main, before T1 to T4 and T2d gate.
  A9's job runs main's harness at strata:15d39c30 beside this branch's for its own "hot within 5%
  of today"; say whether that serves as K6's baseline or A3's must replace it.
- The lead: is A7's job the "rechecker with a build", or should an independent rechecker rerun
  `tools/mutants/rollback.py`?

## Evidence

| Criterion | Evidence |
|---|---|
| A6 | `test/catalog_wake_test.cc` `reads_pin` (strata:f06a4489), with `pin_after_promote`; build-test 261110-164227-8f0e42 (running) |
| A7 | 261110-164238-c8f75e (queued): `tools/mutants/rollback.py` over `apply_atomic_test` and `tools/mutants/apply_throw_probe.cc`, three reverts of the rollback |
| A8 | `test/cold_segment_test.cc` `damage_cases` (16 cases, strata:b5e29ba0) and `open_checks` (missing, resized; eager and lazy); build-test 261110-164227-8f0e42 (running) |
| A9 | 261110-164726-ca2d5c (strict, queued after build 261110-164724-95e365) |
| A10 | `docs/status/cold-tier.md` "K7: who builds snapshots from `seg()`" (strata:5e06300b..2a404927); prospect lazy suite 261110-164333-5b4cd8 (running) |
| A12 | lazy is the default (strata:6d4e1164); build-test 261110-164227-8f0e42 (running) runs it so; eager whole run in 261110-164238-c8f75e |

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
  no new bump here. prospect's suite at its own pin with `STRATA_EAGER_SEGMENTS=0` stands for the
  next bump (prospect main does not build against strata main until manifest-log-prospect lands).
  Xapiand's suite is not run lazy: a host forces eager by `defer_io` (`shard.cc:170`).
- `docs/status/cold-tier.md` conflicts with `design/cold-tier` at its header (it does on main
  too); the K7 section sits between K8 and Log, which both sides leave alone.
