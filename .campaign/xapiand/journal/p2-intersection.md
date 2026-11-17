# p2-intersection: journal

State: waiting on jobs 261117-065609-561d97 (build-test), mutants 261117-065612-0ce8b7..065656-e2b644, A2 261117-065703-06c7aa, probe 261117-065716-5491a6, A/B 261117-065724-58ce08, and CI at prospect:387a47f
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
| A1 | build-test 261117-065609-561d97; mutants 0-9 at 387a47f as --fails-without jobs 261117-065612-0ce8b7, -065615-08dbde, -065619-923e47, -065623-f316a1, -065627-79ac60, -065631-d68413, -065636-91fca7, -065642-233ca5, -065649-9f6bb4, -065656-e2b644 (read each; any that survived is named in Result) |
| A2 | job 261117-065703-06c7aa (dump counted and uncounted vs main, verify, 962 gate queries, 1M) |
| A3 | long lists: 261117-065724-58ce08 (exclusive, 7 rounds, A/A); short lists (R1): 261117-065716-5491a6 (exclusive, run.sh); variant C measured earlier at c3a9fa9, job 261117-065517-ce9053: every case within the A/A (sparse50&blocks2000 1.039 [0.905-1.088], A/A [0.863-1.026]) |
| A4 | CI at prospect:387a47f (run id to read) |

## Result

In process at 1M, b/a over 7 rounds: intersection_2/3/gt3 and intersection_union TOP_k run at 0.80-0.92 of main's time; intersection_union COUNT rows are neutral (0.98-1.07, intervals span 1); A/A 0.998-1.026. Results byte-identical.

## Rulings I made

- The A/B driver bench/quick/classes_ab.py is on track-p, not in this branch or file set; the job runs track-p's copy by path, and the main binary and 1M index are track-p's scratch (`.scratch/bin/main`, built from eef601d, same sha as its A/A copy).
- Removed `count_below` (no caller left); one mechanism.
- Fix (ebd1c13): the failure was the test's, not bit 2's: term "dense" reached ordinal 1147 with 1100 documents. reader.hh is unchanged since 5227e00, so the earlier mutant jobs (run on a failing baseline) are void and were rerun. Failing at 5227e00: build-test 261117-025847-5389cc; passing at the fix: pending 261117-034554-d8f846.
- R1 (eafd2ea): find_in_block probes the next four slots from pos_, then compares branch-free up to buf_len_ rounded up to 16. R2: the dead single-posting fill is removed; mutants 1 and 3 of the earlier set were equivalent, and mutant 2 now is. R3: static_assert beside bits_rank_.
- R1 round 2 (c3a9fa9): the compare alone left sparse50&blocks2000 at 1.237 (far targets scan the block from 0). Final variant: probe 4 slots from pos_, bisect branch-free (main's search) when more than 32 slots remain, else compare from the 16-slot row. Only this variant was measured at exclusive (261117-065517-ce9053: all 7 cases within the A/A).
