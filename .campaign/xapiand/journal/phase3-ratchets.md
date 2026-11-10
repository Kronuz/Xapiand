# phase3-ratchets: journal

State: waiting on jobs 261109-174807-1b0f95 261109-174809-eb0263 261109-174917-a48a55 261109-174919-b26fd0 261109-174921-0cc8d3
Quest: .campaign/xapiand/quests/phase3-ratchets.md (the lead's; never edit it)

## Doing

Wave 1 on the gate VM (strata's Linux VM, port 18772), engine at prospect main `9b8826349cfe`
(`--ref main=`), harness at the quest branch (prospect `6bec208`). Prepare passed
(261109-173556-26604e: prospect_engine x86-64 from main, baselines built; query baselines
lucene-10.4.0, -bp, tantivy-0.26 one segment each). Queued: smoke at 20k (strict,
261109-174807-1b0f95), parity incl. deletes (light, 261109-174809-eb0263), then after the smoke
three 5M keyed units (strict): prospect at 2,056 MB (screen, 261109-174917-a48a55, kept as the
before-merging index), Tantivy-st (261109-174919-b26fd0) and Lucene-st (261109-174921-0cc8d3)
with NO_MERGE=1 (calibration, kept).

## Next

0. If smoke or parity fail, read `campaign job log`, fix in bench/gate3.sh or tools/parity.cc,
   resubmit (downstream jobs are skipped on a failed smoke).
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

Not blocking the measuring yet; the lead's at landing. A10 asks every listed condition met by
its stated point; three cannot be met as worded:
1. C15's allocations per query "at `63a5339`, before Phase 3 changes a query path": the query
   changes landed. Recommendation: accept current-code allocations (`bench/allocs.py`, A4's
   property row) in its place.
2. C7 (WAND or an amended cost model, intersection_union at 1.0x): open, no measurement of
   conjunctions of disjunctions exists. Recommendation: carry with the query rows (Track P).
3. C9's 2,048 MB fuzz hour and text items, C15's frequent-term analysis, Phase 2 indexing
   trial files and corpus checksum: recommendation: a small docs/fuzz follow-up quest.

## Evidence

| Criterion | Evidence |
|---|---|
| A10 | `docs/gates/phase-3-ratchets.md` "Phase 2 gate conditions" (prospect `6bec208`): C1, C3, C5, C13, C14 closed; C2, C9, C16 partly; C7, C15 open (see Blocked on) |

Jobs examined: 261109-173556-26604e prepare, passed. 261109-173630-d3b142 smoke, failed: the
20k index at a 256 MB budget had one segment, so the merge had nothing to reorder (fixed: smoke
budget 4 MB, `3d215c6`); its three downstream units were skipped. 261109-174424-1568bd parity,
failed: my PARITY_DELETES refactor returned postings viewing a destroyed map ("term repeated
within a document"; fixed `3d215c6`).

## Result

<When ready for review.>

## Rulings I made

- R1 Harness: each gate row is a job of at most 30 minutes driving the VM through
  `tools/vm/gate_job.sh STEP` (prospect `eb105a3`, `4433595`); the engine builds from
  `$JOB_REF_MAIN` (main), so a harness change on the branch does not change the measured code.
- R2 Indexing units call each builder directly (not `make index`, whose Lucene target reruns
  gradle inside the timed run) with NO_MERGE=1 for the baselines (BENCHMARKS, "Indexing speed";
  the 2026-10-17 run `n1017` had no NO_MERGE, so its baseline times include force merges).
- R4 Allocations per query come from `bench/alloc_count.c`, an LD_PRELOAD shim over the release
  binary (glibc `__libc_*`), logging totals at each stdin read; per-query deltas need the
  driver to send one query and wait for its answer (`bench/allocs.py`). It measures what ships.
- R5 Parity with deletes uses the existing oracle vectors: an all-deleted first segment (older
  versions of every 7th document, and documents added then deleted) precedes the oracle's
  10,000; counts must be exact before the merge (weights drift: deleted docs stay in the
  statistics), everything exact after a merge drops them. No oracle change needed.
- R3 Memory rows from `bench/cgmem.sh` (cgroup v2 memory.peak, anon, file, file_mapped) beside
  GNU time's peak RSS; the corpus is piped from outside the cgroup so its page cache is not
  charged to the engine.
