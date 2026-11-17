# p2-intersection: journal

State: waiting on jobs 261117-034554-d8f846 (build-test), mutants 261117-034556-bfc0aa..034616-5e4b22, A2 261117-034619-9464d1, A3 261117-034629-5eeca5, CI at prospect:ebd1c13
Quest: .campaign/xapiand/quests/p2-intersection.md (the lead's; never edit it)

## Doing

Code at prospect:ebd1c13 (quest branch pushed): P2 bits 1+2 applied to include/prospect/reader.hh with no switch (patch's reader.hh hunks only; galloping and block filter left out; `count_below` removed as unused). Tests test/intersection_test.cc, mutants test/intersection_mutants.py (7 mutants, one job each).

## Next

1. Read the jobs: build-test 261117-025847-5389cc; mutant jobs (each passes when the mutant is caught); A2 261117-030019-277d60; A3 261117-030032-f81a57; CI run 44070056329 (`gh run view`).
2. Fill the Evidence rows, state the A3 gain and A/A spread in Result, then set ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261117-025847-5389cc (ctest `intersection`); mutants 0-6 as --fails-without jobs 261117-025850-78b1d9, -025857-03282d, -025904-c44dfb, -025910-5aa1df, -025917-919ac8, -025924-b146ca, -025930-78ee6d |
| A2 | job 261117-030019-277d60 (`~/.scratch/p2-intersection/parity.sh`: dump counted and uncounted vs main's binary eef601d, verify, over the 962 gate queries, track-p's 1M index) |
| A3 | job 261117-030032-f81a57 (`~/.scratch/p2-intersection/ab.sh`: exclusive, 7 rounds, A/A = copy of main, driver track-p's classes_ab.py) |
| A4 | CI run 44070056329 on prospect:ebd1c13 |

## Result

Pending the jobs.

## Rulings I made

- The A/B driver bench/quick/classes_ab.py is on track-p, not in this branch or file set; the job runs track-p's copy by path, and the main binary and 1M index are track-p's scratch (`.scratch/bin/main`, built from eef601d, same sha as its A/A copy).
- Removed `count_below` (no caller left); one mechanism.
- Fix (ebd1c13): the failure was the test's, not bit 2's: term "dense" reached ordinal 1147 with 1100 documents. reader.hh is unchanged since ebd1c13, so the earlier mutant jobs (run on a failing baseline) are void and were rerun. Failing at ebd1c13: build-test 261117-025847-5389cc; passing at the fix: pending 261117-034554-d8f846.
