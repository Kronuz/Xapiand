# phase3-ratchets: journal

State: ready for review (correctness lens). Prospect branch head 0aa68f4, pushed; CI running there.
Quest: .campaign/xapiand/quests/phase3-ratchets.md (the lead's; never edit it)

## Doing

Session 2026-11-11 (resumed after the reruns): every fba0e0d job had finished; scored and
reported in prospect `docs/gates/phase-3-ratchets.md` "Gate report at main" (d489c33, 8382c94),
the status table, the C14/C15/C16 citations, and `docs/testing/parity.md` "Coverage" (2332c9).
- A1 pools the fe20f43 and fba0e0d trials (4 per row, same engine), classified by
  `bench/query_verdicts.py` (prospect 42e1c0a). After: 18 pass, 2 caveat, 14 miss (1.02-1.24x,
  13 vs Tantivy), 6 straddle; global 0.79-0.91x. Before: 6/2/24/8; global 1.01-1.19x. Each job
  alone gives the same verdicts.
- A2 1t 1M at fba0e0d: 0.424x [0.420-0.429] Tantivy-st (2.33-2.38x faster). fe20f43's 0.355x is
  superseded: its stale Tantivy-st took 105 s, the rebuilt one 86-87 s (overstated by ~19%).
- A2 8t 1M: 0.561x [0.551-0.572] Lucene-st (1.75-1.82x faster). Finding for Track P: prospect
  on 8 threads is 1.03x its 1-thread speed at 3.3x the CPU, peak 2.00x budget.
- A3 500k, 64 MB buffers: dirtied/final append 2.479 vs Lucene 2.839 (0.87x), replace 3.620 vs
  4.442 (0.81x); device 1.190 vs 1.497 (0.79x), 1.429 vs 1.388 (1.03x). Many-index rows owed (D51).
- A4-A6 reruns agree with fe20f43 exactly (sections, allocations, verify, parity); serving
  530 MB vs Tantivy 563 (0.94x).
- CI at 9de065a: every Linux job failed in apt (`Temporary failure resolving
  'azure.archive.ubuntu.com'`, exit 100, then `ccache: command not found`, 127); FreeBSD and
  both macOS passed. A runner network flake, not the branch (its non-doc code is bench/tools).
  Rerun by the push of 0aa68f4.
- History: 42e1c0a (gate3_report's 8-thread fix, query_verdicts.py) broke the docs-only chain
  every row's evidence rides on; reverted, then rebuilt the branch on 9de065a with the doc
  commits and joined the pushed history with an ours-merge (force-push was refused), so the
  first-parent line is docs-only since fba0e0d and 42e1c0a stays reachable.

## Next

1. Lead: correctness review; land the scoring tools (42e1c0a) after the quest as a harness-only
   change; read the eight-thread finding for Track P.
2. If CI at 0aa68f4 fails again in apt, rerun the failed jobs; nothing to fix on the branch.

## Blocked on

Nothing. The lead ruled on all six proposals (D51, Xapiand `.campaign/xapiand/PLAN.md`, 2026-11-10):
1. C15 takes allocation counts at current code. 2. C7 carries to the next phase. 3. C9 and
C15's rest go to a follow-up quest. 4. A4's indexing budget miss (1.57x, limit 1.1x) stays a
miss in the gate doc; the fix (charge merge buffers) is a follow-up quest; state probe
261110-022424-057569's result when it lands. 5. A3's many-index rows go to a harness quest
(high priority). 6. A8: FreeBSD as a deferred check set by the lead; arm64 waits for a local host.
Folded into `docs/gates/phase-3-ratchets.md` at prospect `3e92ea9`.

Finding for the lead, answered: the probe's same-peak result is explained by prospect
docs/perf/index-memory.md (b23d139); the fix is quest flush-within-budget. A4 cites it.

A8 deferred check, the command for the lead once smoke 261110-023726-f5b60e passes (light:
the first run builds on the VM's 4 vCPUs; its times are informational, prospect only, since
the VM has no rust or JDK and 4.5 GB free disk bounds DOCS):

    campaign job submit --quest phase3-ratchets --later --ref main=<landed main> \
      --level light --minutes 120 --env DOCS=500000 --env PROSPECT_BUDGET_MB=1039 \
      --what "A8 FreeBSD rows: prospect keyed 500k, merge, verify, parity" -- tools/vm/bsd_rows.sh

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261111-072709-569667, 261111-072711-f9506e (passed at fba0e0dc148e; docs-only since); pooled with 261111-023611-df2aff, 261111-023613-74ba91 (same engine) in the report |
| A2 | 261111-072646-cbabd8, 261111-072713-3b714c, 261111-072715-8d41f8 (passed at fba0e0dc148e) |
| A3 | 261111-072642-12c9fe, 261111-072644-00aa3e (passed at fba0e0dc148e); many-index rows to a harness quest (D51) |
| A4 | 261111-072646-cbabd8, 261111-072649-debd0a, 261111-072651-cf4452 (passed at fba0e0dc148e) |
| A5 | 261111-072649-debd0a (passed at fba0e0dc148e) |
| A6 | 261111-072649-debd0a, 261111-072654-b92e66 (passed at fba0e0dc148e) |
| A7 | `docs/testing/parity.md` "Coverage" (job 261111-072722-2332c9) |
| A8 | 261111-073924-e0cd69; decision D51 (FreeBSD deferred check, arm64 waits for a host) |
| A9 | `docs/gates/phase-3-ratchets.md` "Tools" |
| A10 | decision D51; `docs/gates/phase-3-ratchets.md` "Phase 2 gate conditions" |
| A11 | `docs/gates/phase-3-ratchets.md` "Gate report at main" and "Gate status (2026-11-11)" |

Jobs examined: 261109-173556-26604e prepare, passed. 261109-173630-d3b142 smoke, failed: the
20k index at a 256 MB budget had one segment, so the merge had nothing to reorder (fixed: smoke
budget 4 MB, `3d215c6`); its three downstream units were skipped. 261109-174424-1568bd parity,
failed: my PARITY_DELETES refactor returned postings viewing a destroyed map ("term repeated
within a document"; fixed `3d215c6`).

## Result

Phase 3's ratchet rows at prospect main `f0d7871`, gate VM (Linux x86_64), in prospect
`docs/gates/phase-3-ratchets.md` "Gate report at main" and "Gate status (2026-11-11)":
- Pass: A2 1t 1M 0.424x Tantivy-st's time [0.420-0.429]; A2 8t 1M 0.561x Lucene-st's
  [0.551-0.572]; serving memory 0.94x Tantivy; disk whole 0.993x keyed `-bp`; verify 0
  mismatches (3 segments and 1); parity exact, deletes included (top 10 157/419 until merged).
- Reported: write amplification 0.87x/0.81x Lucene-st's bytes dirtied (append/replace), 0.79x/
  1.03x its device bytes; 5M 249.1 s.
- Miss, carried: A1 after merging 14 of 40 (1.02-1.24x), before 24 of 40 (1.02-1.60x);
  indexing peak 1.56x the budget (flush-within-budget); allocations grow for the term class;
  postings 1.012x and terms 1.123x (D29).
- Owed: many-index rows (harness quest), arm64 (no host), FreeBSD rows (deferred check; smoke
  passed). Coverage: 5 headers owe lines or branches (`docs/testing/parity.md`).

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
- R14 Harness faults that invalidated rows are fixed, not footnoted: fba0e0d (write
  amplification by bytes dirtied, no force merge; builders rebuilt with make -B). The tool
  keys evidence by the whole tree, so every row reruns at fba0e0d; the fe20f43 numbers stay
  the report's (same engine, main f0d7871) unless a rerun disagrees.
- R15 The warm-up rule stays as fixed before data (a class beating a JVM engine by <10% is
  "within the caveat"), and the report adds the measured drift (up to ~18%) and names the
  passes it leaves unestablished.
- R16 A1's verdicts pool both jobs' trials (fe20f43, fba0e0d: same engine, same query harness),
  four per row; classified by one rule (`query_verdicts.py`), which reads `TOP_100` phrase
  (lower bound 1.001x) as a miss and `TOP_1000` global (`-bp`, 0.94x) as within the caveat.
- R17 The A2 rows take fba0e0d's runs only: the fe20f43 Tantivy-st builder was stale.
- R18 Scoring tools stay out of the measured tree until the quest lands (42e1c0a, reachable
  through the ours-merge 0aa68f4): any non-doc commit after fba0e0d invalidates every row.
