# phase3-ratchets: journal

State: waiting on jobs 261111-023449-dbd2bc 261111-023513-9cf689 261111-023527-01525e 261111-023529-0480c9 261111-023531-861bbf 261111-023533-e5b827 261111-023549-08d8e1 261111-023550-47b686 261111-023552-604cea 261111-023611-df2aff 261111-023613-74ba91 261111-023615-46b75c 261111-023617-714ad0 261111-023627-a73c21 (queued 2026-11-11 02:36)
Quest: .campaign/xapiand/quests/phase3-ratchets.md (the lead's; never edit it)

## Doing

Merged prospect main `f0d7871` into the branch (3ed265a): main moved strata's pin to the
manifest log (strata 94480d5; 074129c retires the manifest, live-docs, metadata and field-table
codecs), so indexing and commits run new code and every row runs again at `--ref main=f0d7871`.
Harness fe20f43: corpus line counts cached (the 5M unit spent 2.8 min reading the corpus cold
to count it; the job was 10.4 min for 7 min of indexing), smoke keeps the baselines' 5M
unmerged indexes (A1-before reads them), gate_job forwards LUCENE_RAM_MB/TANTIVY_HEAP_MB.
Docs ca5e913: the 9b88263 rows of wave 2 in the gate doc, the f0d7871 plan, coverage by tier.

Wave 2 at main 9b88263 (read; finished results stand as a main-commit measurement): see the
gate doc's "Gate run at main 9b88263". A1 after merging 14 pass / 7 miss / 19 straddle of 40
(misses to 1.18x); before merging 5 / 29 / 6 (to 1.68x); A2 1M 2.19x and 3.60x faster than
Tantivy-st; A4 serving 0.94x Tantivy (pass), allocations grow for the term class (miss),
probe: same 3,221 MiB peak without background merges (finding for the lead below); A5 whole
0.993x, postings 1.012x, terms 1.123x; A6 verify 0 mismatches.

D59 plan at main f0d7871 (one VM chain; exclusive rows wait for no hold):
prepare dbd2bc (light) -> smoke 9cf689 (strict 5: every step at 20k, 8-thread units, wamp at
16 MB) -> 5M unit 01525e (strict 10) -> merge 0480c9 (light 30) -> sections+verify+allocs
861bbf (light 30) -> servemem e5b827 (strict 5) -> parity 08d8e1 (light) -> wamp append
47b686 -> wamp replace 604cea (strict 10 each). Exclusive: A1-before df2aff (after the 5M
unit), A1-after 74ba91 (after verify), A2-1t 46b75c and A2-8t 714ad0 (after smoke). Coverage
a73c21 on the host (light 30). Ids are 261111-023xxx-<suffix>.

What each job decides and why its size suffices:
- A1 before / after (exclusive 10 each): two trials, engine order rotated, both sets (181
  queries + 40 terms), WARMUP_TIME=5 NUM_ITER=5 (f13 row 4b's settings). Decides each class
  against 1.0x by gate.py's interval over the two trials. Size: at 9b88263 the deciding misses
  clear 1.0x by their whole interval (before: 29 rows, lower bounds to 1.58x; after: 7, e.g.
  COUNT phrase 1.09-1.11x vs Tantivy, native, no JIT), so the verdict (miss, carried) cannot
  flip; 20 s warm-up x 5 commands x 3 engines x 2 sets is 600 s per trial alone, so the 5 s
  warm-up is what fits, and a class beating Lucene -bp by under 10% is reported as within the
  warm-up caveat (format13-gate-vm.md). After merging drops plain Lucene: rescoring the three
  9b88263 trials without it changes no row by more than 0.01.
- A2 one thread (exclusive 10): 1M at 1,039 MB, P T T P against Tantivy-st (faster baseline).
  Margin at 9b88263 2.19x-3.60x faster, so two pairs decide >= 1.0x; ~7 min after the
  line-count fix (pairs took 3.9-4.5 min with it).
- A2 eight threads (exclusive 10): 1M, P T L L T P, all at 8 threads; first measurement, two
  rotations give the interval; 1-thread 1M times bound it under 9 min even at 2x speedups.
- A2/A4 5M unit (strict 10): the scale-up time beside 2026-11-09's Tantivy-st 870 s and
  Lucene-st 1,162 s (R11) and the 5M peaks against the 2,056 MiB budget + 10%.
- A3 (strict 10 each): 500k keyed, every engine at a 64 MB buffer (R12), append and replace.
  Bytes, not time; 64 MB buffers make every engine flush and merge many times at 500k, which
  the 1M equal-memory runs (1-2 GB buffers, one flush) would not.
- A4/A5/A6 checks (light): verify both sets before (prospect's 5M segments) and after merging,
  sections against keyed -bp, allocations by class; serving memory strict. Correctness and
  bytes, deterministic.
- A6 parity (light): 1/4/16 segments, deletes at 4/16, oracle ranks f70efd10.

## Next

1. When the jobs land: score A1 with `bench/phase2/gate.py --md --target 1.0 --prospect
   prospect-merged` (after) and `--as lucene-10.4.0-st=lucene-10.4.0 --as
   tantivy-0.26-st=tantivy-0.26` (before); A2-A5 with `bench/gate3_report.py --budget-mb N DIR`.
   A second run only for a row whose interval straddles its target (D59).
2. Replace the gate doc's pending table with the f0d7871 rows (A11), coverage numbers in
   `docs/testing/parity.md` (A7), the Evidence table below.
3. CI: no runs on the branch head (I never push); the lead pushes or lands with --no-ci.
4. Correctness review: not launched (the lead's).

## Blocked on

Nothing. The lead ruled on all six proposals (D51, Xapiand `.campaign/xapiand/PLAN.md`, 2026-11-10):
1. C15 takes allocation counts at current code. 2. C7 carries to the next phase. 3. C9 and
C15's rest go to a follow-up quest. 4. A4's indexing budget miss (1.57x, limit 1.1x) stays a
miss in the gate doc; the fix (charge merge buffers) is a follow-up quest; state probe
261110-022424-057569's result when it lands. 5. A3's many-index rows go to a harness quest
(high priority). 6. A8: FreeBSD as a deferred check set by the lead; arm64 waits for a local host.
Folded into `docs/gates/phase-3-ratchets.md` at prospect `3e92ea9`.

Finding for the lead (not blocking): D51 item 4's follow-up (charge merge buffers to the
budget) rests on a premise the probe refutes: prospect keyed 5M with PROSPECT_AUTO_MERGE=0
peaked at 3,221 MiB, the same as with merges (job 261110-022424-057569). The excess over the
2,056 MiB budget is elsewhere in the indexing path; the follow-up quest should measure where.

A8 deferred check, the command for the lead once smoke 261110-023726-f5b60e passes (light:
the first run builds on the VM's 4 vCPUs; its times are informational, prospect only, since
the VM has no rust or JDK and 4.5 GB free disk bounds DOCS):

    campaign job submit --quest phase3-ratchets --later --ref main=<landed main> \
      --level light --minutes 120 --env DOCS=500000 --env PROSPECT_BUDGET_MB=1039 \
      --what "A8 FreeBSD rows: prospect keyed 500k, merge, verify, parity" -- tools/vm/bsd_rows.sh

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | queued: 261111-023611-df2aff (before merging), 261111-023613-74ba91 (after merging) |
| A2 | queued: 261111-023615-46b75c (one thread), 261111-023617-714ad0 (eight threads), 261111-023527-01525e (5M) |
| A3 | queued: 261111-023550-47b686 (append), 261111-023552-604cea (replace) |
| A4 | queued: 261111-023527-01525e (indexing peaks), 261111-023531-861bbf (allocations), 261111-023533-e5b827 (serving) |
| A5 | queued: 261111-023531-861bbf (sections) |
| A6 | queued: 261111-023531-861bbf (verify), 261111-023549-08d8e1 (parity) |
| A7 | pending coverage at f0d7871: `docs/testing/parity.md` "Coverage" holds the 9b88263 numbers |
| A8 | deferred by decision D51 (FreeBSD as a deferred check, arm64 waits for a host); driver `tools/vm/bsd_rows.sh` |
| A9 | `docs/gates/phase-3-ratchets.md` "Tools" |
| A10 | decision D51; `docs/gates/phase-3-ratchets.md` "Phase 2 gate conditions" |
| A11 | pending the f0d7871 rows: `docs/gates/phase-3-ratchets.md` holds every 9b88263 row with ratio, interval, jobs and the gate status table |

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
- R6 No recalibration of the 5M budget: prospect at 2,056 MB peaks under both baselines (70% of
  Lucene-st's, 28% of Tantivy-st's), so the time rows compare it with less memory than the
  reference, which can only understate it; the report states every peak.
- R7 Engine ref stays main `9b88263` after the rebase: main's only change since is
  docs/design/automata.md, so the VM's build and kept indexes stay valid.
- R8 Targeted trials (the quest's "Targeted runs" decision): A2's five interleaved trials run at
  1M (prospect at the 1M calibration, 1,039 MB, vs Tantivy-st, the faster baseline at 5M), the 5M
  runs being the one scale-up confirmation; A1's trials use the quick tier's sample
  (queries-sample 181, terms-sample 40) with WARMUP_TIME=20 (5 s flattered strata against the
  JVM by up to 10%, format13-gate-vm.md).
- R3 Memory rows from `bench/cgmem.sh` (cgroup v2 memory.peak, anon, file, file_mapped) beside
  GNU time's peak RSS; the corpus is piped from outside the cgroup so its page cache is not
  charged to the engine.
- R7 superseded: the engine ref is main `f0d7871` (strata's manifest log), every row rebuilt.
- R9 Timed rows by D59: one job of at most 10 minutes per row, pairs inside it (plan under
  Doing). Query rows at WARMUP_TIME=5 NUM_ITER=5 with the JVM caveat; R8's 20 s warm-up and
  five-trial waves are retired.
- R10 Harness fe20f43: line counts cached and warmed by prepare; smoke keeps only prospect's
  indexes (it used to overwrite the 5M unmerged Lucene-st/Tantivy-st indexes with 20k ones).
- R11 The 5M baselines are not rerun: Tantivy-st 870 s and Lucene-st 1,162 s exceed D59's
  10 minutes, and their builders run the same code at one thread (61be43a only adds thread
  counts); the 2026-11-09 times stand beside prospect's f0d7871 5M run.
- R12 Write amplification at equal segment budgets (PLAN, Phase 3 gate): every engine at a
  64 MB buffer (PROSPECT_BUDGET_MB, LUCENE_RAM_MB, TANTIVY_HEAP_MB), 500k documents.
- R13 Merge (16.4 min at 5M) and verify (8.4 min) run as light jobs: neither is a timed row.
