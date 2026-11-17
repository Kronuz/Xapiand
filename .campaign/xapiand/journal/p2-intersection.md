# p2-intersection: journal

State: ready for review
Quest: .campaign/xapiand/quests/p2-intersection.md (the lead's; never edit it)

## Doing

Done at prospect:ebd1c13.

## Next

1. Review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261117-034554-d8f846 (ctest `intersection`); mutants 0-6 as --fails-without jobs 261117-034556-bfc0aa, -034559-10e420, -034603-7c65ba, -034606-b7f744, -034609-858907, -034613-bb1349, -034616-5e4b22 (at ebd1c13) |
| A2 | job 261117-034619-9464d1 (dump counted and uncounted vs main, verify, 962 gate queries, 1M) |
| A3 | job 261117-034629-5eeca5 (exclusive, 7 rounds, A/A c/a 0.998-1.026) |
| A4 | CI run 44076940308 on prospect:ebd1c13, green on all platforms |

## Result

In process at 1M, b/a over 7 rounds: intersection_2/3/gt3 and intersection_union TOP_k run at 0.80-0.92 of main's time; intersection_union COUNT rows are neutral (0.98-1.07, intervals span 1); A/A 0.998-1.026. Results byte-identical.

## Rulings I made

- The A/B driver bench/quick/classes_ab.py is on track-p, not in this branch or file set; the job runs track-p's copy by path, and the main binary and 1M index are track-p's scratch (`.scratch/bin/main`, built from eef601d, same sha as its A/A copy).
- Removed `count_below` (no caller left); one mechanism.
- Fix (ebd1c13): the failure was the test's, not bit 2's: term "dense" reached ordinal 1147 with 1100 documents. reader.hh is unchanged since 5227e00, so the earlier mutant jobs (run on a failing baseline) are void and were rerun. Failing at 5227e00: build-test 261117-025847-5389cc; passing at the fix: pending 261117-034554-d8f846.
