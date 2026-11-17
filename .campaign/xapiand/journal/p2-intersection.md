# p2-intersection: journal

State: waiting on jobs 261117-072238-8ce0c3 (build-test), mutants 261117-072240-309cc3..072319-fd1112 at prospect:0aad777
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
| A1 | at 0aad777 (tests only since 387a47f; reader.hh unchanged): build-test 261117-072238-8ce0c3; mutants 0-9 as --fails-without jobs 261117-072240-309cc3, -072244-880b46, -072248-575707, -072252-14b5e4, -072256-38c7b0, -072300-99692a, -072304-ae8cc7, -072309-65fc3b, -072314-efd042, -072319-fd1112 (mutant 4, -072256-38c7b0, survives and is equivalent: ordinals strictly increase, so when base[half] equals the target `<=` moves base onto that slot and the final `*base < target` is then 0, the same position `<` reaches; mutants 7-9 were still running when this was written; survivors of the previous round, 0 2 3 4 6, were not killed by it: the test only advanced to each target from a fresh iterator, which never reached the compare past the probes; strides now do, and tail terms of 30 postings pad-check it) |
| A2 | identity dump and verify vs main on the 1M index at 0aad777: job 261117-073948-099d4a |
| A3 | at 0aad777: 1M A/B 261117-073948-1c1cd6 (exclusive, 7 rounds, A/A c/a 0.98-1.01): intersection rows 0.86-1.03, 12 of 30 with intervals wholly below 1.0; intersection_union_3 COUNT 1.106 [1.003, 1.220] against its A/A 0.987 [0.892, 1.092], per-query best 1.010, and 1.034 [0.917, 1.165] in the 387a47f run, so read as noise at one row in 30; short-list probe 261117-073743-15805e (exclusive), every case within its A/A |
| A4 | lead checked GitHub CI run 44113619238: green on every platform at 0aad777 |

## Result

Smaller than the P2 prototype's 0.78-0.92: the final search (probe 4 slots, bisect a long remainder, compare a short one from its row) trades part of the all-128-slot compare's long-list gain for no regression on short lists.

1M A/B (job 261117-065724-58ce08, b/a, 7 rounds; A/A c/a 0.95-1.08): intersection rows 0.90-1.03 of main's time, none slower. Intervals entirely below 1.0: intersection_2 `TOP_100` 0.930 [0.894, 0.967], intersection_3 `TOP_100_COUNT` 0.946 [0.909, 0.983], intersection_gt3 `TOP_100_COUNT` 0.927 [0.870, 0.986], intersection_union_2 `TOP_10` 0.933 and `TOP_100_COUNT` 0.897, intersection_union_3 `TOP_100_COUNT` 0.972, intersection_union_gt3 `TOP_100` 0.915. Every other row (all `COUNT` rows 0.97-1.03, most `TOP_10` and `TOP_1000`) is within noise: its interval spans 1.0 as does the A/A's.

Short lists (probe 261117-065716-5491a6, new/old; A/A): tail40&tail100 0.954 [0.883-0.971], tail10&tail120 1.010, tail40&100&120 0.972, one&tail100 0.995, one&tail40&100 0.961, sparse50&blocks2000 1.033 [0.923-1.108] (A/A [0.926-1.063]), tail100&blocks2000 0.945; all within the A/A. Before the fix: 1.5-2.3x slower (review R1).

## Rulings I made

- The A/B driver bench/quick/classes_ab.py is on track-p, not in this branch or file set; the job runs track-p's copy by path, and the main binary and 1M index are track-p's scratch (`.scratch/bin/main`, built from eef601d, same sha as its A/A copy).
- Removed `count_below` (no caller left); one mechanism.
- Fix (ebd1c13): the failure was the test's, not bit 2's: term "dense" reached ordinal 1147 with 1100 documents. reader.hh is unchanged since 5227e00, so the earlier mutant jobs (run on a failing baseline) are void and were rerun. Failing at 5227e00: build-test 261117-025847-5389cc; passing at the fix: pending 261117-034554-d8f846.
- R1 (eafd2ea): find_in_block probes the next four slots from pos_, then compares branch-free up to buf_len_ rounded up to 16. R2: the dead single-posting fill is removed; mutants 1 and 3 of the earlier set were equivalent, and mutant 2 now is. R3: static_assert beside bits_rank_.
- R1 round 2 (c3a9fa9): the compare alone left sparse50&blocks2000 at 1.237 (far targets scan the block from 0). Final variant: probe 4 slots from pos_, bisect branch-free (main's search) when more than 32 slots remain, else compare from the 16-slot row. Only this variant was measured at exclusive (261117-065517-ce9053: all 7 cases within the A/A).
