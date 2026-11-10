# phase3-ratchets: journal

State: waiting on jobs 261109-173556-26604e 261109-173630-d3b142 261109-173631-2121fe 261109-173633-7991cc 261109-173634-b8d9e2
Quest: .campaign/xapiand/quests/phase3-ratchets.md (the lead's; never edit it)

## Doing

Wave 1 on the gate VM (strata's Linux VM, port 18772), engine at prospect main `9b8826349cfe`
(`--ref main=`), harness at the quest branch: prepare (light) -> smoke at 20k (strict) -> three
5M keyed indexing units (strict): prospect at the 2,056 MB budget (screen; kept as the
before-merging index), Tantivy and Lucene one-thread with NO_MERGE=1 (calibration; kept).

## Next

1. Read wave 1: prospect's 5M wall time and peak against Tantivy-st/Lucene-st NO_MERGE; is
   2,056 MB still within 3% of the reference's peak (recalibrate only on drift)?
2. Wave 2: merge (light, 8 reorder threads) -> sections (A5) + verify before/after (A6) ->
   five after-merging query trials, split per set (exclusive, prospect-merged, lucene-bp,
   tantivy, lucene; WARMUP 60) -> five before-merging trials (prospect, tantivy-st, lucene-st
   unmerged).
3. A2 trials: prospect vs the faster one-thread baseline, alternating units, exclusive.
4. A3 (write_amp.sh), A4 serving memory (serve_mem.py / cgmem.sh) and allocations per query
   (an allocation-counting build, A9), A6 parity with deletes (tools/parity.cc), A7/A10/A11 docs.
5. A8 FreeBSD and arm64 as deferred checks.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|

## Result

<When ready for review.>

## Rulings I made

- R1 Harness: each gate row is a job of at most 30 minutes driving the VM through
  `tools/vm/gate_job.sh STEP` (prospect `eb105a3`, `4433595`); the engine builds from
  `$JOB_REF_MAIN` (main), so a harness change on the branch does not change the measured code.
- R2 Indexing units call each builder directly (not `make index`, whose Lucene target reruns
  gradle inside the timed run) with NO_MERGE=1 for the baselines (BENCHMARKS, "Indexing speed";
  the 2026-10-17 run `n1017` had no NO_MERGE, so its baseline times include force merges).
- R3 Memory rows from `bench/cgmem.sh` (cgroup v2 memory.peak, anon, file, file_mapped) beside
  GNU time's peak RSS; the corpus is piped from outside the cgroup so its page cache is not
  charged to the engine.
