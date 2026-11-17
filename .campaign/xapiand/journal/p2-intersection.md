# p2-intersection: journal

State: waiting on jobs 261117-060112-e8ad6a 261117-060113-7e021c 261117-060117-dae932 261117-060120-35e79c 261117-060124-c1060f 261117-060127-884294 261117-060130-ae26ae 261117-060133-4f8a73 261117-060137-f668dd 261117-060144-9cd5a2 261117-060147-f69839 261117-060150-8546f2 and CI at prospect:eafd2ea
Quest: .campaign/xapiand/quests/p2-intersection.md (the lead's; never edit it)

## Doing

R1-R3 fixed at prospect:eafd2ea; reading its jobs.

## Next

1. Read the jobs; state the results in Result; set ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | build-test 261117-060112-e8ad6a; mutants at eafd2ea as --fails-without jobs 261117-060113-7e021c (0), -060117-dae932 (1), -060124-c1060f (3), -060127-884294 (4), -060130-ae26ae (5), -060133-4f8a73 (6), -060137-f668dd (7); mutant 2 (-060120-35e79c, skip the first probe) is equivalent: the compare finds the same slot, so it is not claimed |
| A2 | job 261117-060144-9cd5a2 (dump counted and uncounted vs main, verify, 962 gate queries, 1M) |
| A3 | long lists: 261117-060150-8546f2 (exclusive, 7 rounds, A/A); short lists (R1): 261117-060147-f69839 (strict, the reviewer's run.sh, no row above the A/A) |
| A4 | CI at prospect:eafd2ea (run id to read) |

## Result

In process at 1M, b/a over 7 rounds: intersection_2/3/gt3 and intersection_union TOP_k run at 0.80-0.92 of main's time; intersection_union COUNT rows are neutral (0.98-1.07, intervals span 1); A/A 0.998-1.026. Results byte-identical.

## Rulings I made

- The A/B driver bench/quick/classes_ab.py is on track-p, not in this branch or file set; the job runs track-p's copy by path, and the main binary and 1M index are track-p's scratch (`.scratch/bin/main`, built from eef601d, same sha as its A/A copy).
- Removed `count_below` (no caller left); one mechanism.
- Fix (ebd1c13): the failure was the test's, not bit 2's: term "dense" reached ordinal 1147 with 1100 documents. reader.hh is unchanged since 5227e00, so the earlier mutant jobs (run on a failing baseline) are void and were rerun. Failing at 5227e00: build-test 261117-025847-5389cc; passing at the fix: pending 261117-034554-d8f846.
- R1 (eafd2ea): find_in_block probes the next four slots from pos_, then compares branch-free up to buf_len_ rounded up to 16. R2: the dead single-posting fill is removed; mutants 1 and 3 of the earlier set were equivalent, and mutant 2 now is. R3: static_assert beside bits_rank_.
