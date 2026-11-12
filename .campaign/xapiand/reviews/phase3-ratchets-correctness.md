# phase3-ratchets review: correctness

Commit: prospect:0aa68f435bd2579e8f43eaec28f34e46669001fc
Verdict: APPROVE
Date: 2026-11-11

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

The fba0e0d jobs in the journal's Evidence table (all exit 0, `--ref main=f0d7871`), read from
their files rather than the journal: A1 261111-072709-569667, 261111-072711-f9506e pooled with
fe20f43's 261111-023611-df2aff, 261111-023613-74ba91; A2 261111-072713-3b714c,
261111-072715-8d41f8, 261111-072646-cbabd8; A3 261111-072642-12c9fe, 261111-072644-00aa3e; A4-A6
261111-072649-debd0a, 261111-072651-cf4452, 261111-072654-b92e66; prepare 261111-072639-0c7c0a
(its `compile-tantivy-0.26-st.log` shows cargo recompiling the keyed builder, so the `make -B`
fix took). The merged 5M index the after-merging rows, sections and verify read is fe20f43's merge
(261111-023529-0480c9), which the report names; the engine is f0d7871 in both harness runs.

## What I checked myself

Scripts under `.scratch/review-phase3-ratchets-correctness/` in the Xapiand home checkout.

1. Risk 5 (R18). `git diff fba0e0d 0aa68f4` touches only `docs/gates/phase-3-ratchets.md` and
   `docs/testing/parity.md`; `git diff 8d149bb 0aa68f4` and `git diff 8382c94 0aa68f4` are empty
   (the ours-merge keeps the docs-only side's tree); `git diff f0d7871 fba0e0d` outside docs/,
   bench/ and tools/ is empty, and main `b23d139` adds only `docs/perf/index-memory.md` to
   f0d7871. The merge base is main, so main receives exactly the bench/ and tools/ files the
   jobs ran (gate_job.sh ships this checkout's bench/ and tools/, the engine from
   `$JOB_REF_MAIN`). 42e1c0a is reachable but not in the tree.
2. Risk 1. The branch's builder changes are `LUCENE_THREADS` and `TANTIVY_THREADS`, both
   defaulting to 1; analysis (StandardAnalyzer with no stop words, Tantivy TEXT), commit policy
   (one commit at close), NO_MERGE and the buffer settings are main's. gate3.sh's Lucene command
   is the engine Makefile's `idx` command. Prospect runs at 1,039 MiB against Tantivy-st's 2 GB
   heap and Lucene-st's 2,047 MB buffer, so memory is tilted against prospect, not for it. The
   upstream harness's builders (`~/.scratch/bc/sbg/engines/tantivy-0.26`, `lucene-10.4.0`) have
   the same feeding shape: Tantivy parses JSON on the main thread, Lucene in its workers.
3. Risk 2, recomputed from job files. A2 one thread: 36.55/87.06 = 0.4198, 36.98/86.27 = 0.4287,
   median 0.424 (2.38x, 2.33x faster); peak RSS 1,616,188/2,723,516 = 0.59x; 1,578 MiB / 1,039 =
   1.52x. A2 eight threads: 35.63/64.70 = 0.551, 35.43/61.94 = 0.572, median 0.561; 2,081 MiB /
   1,039 = 2.00x. 5M: 3,216 MiB / 2,056 = 1.56x. A3 from the `.result` lines: 834,519,040 /
   336,606,400 = 2.479 and 922,718,208 / 325,051,139 = 2.839 (0.87x); replace 3.620 / 4.442 =
   0.81x, device 1.429 / 1.388 = 1.03x; Tantivy-st ratios 1.59x, 1.26x, 0.75x, 0.49x. Disk:
   2,525,614,060 / 2,542,535,895 = 0.993x; postings 1.012x; terms with keys (terms, term_index,
   keys, key_index over tim, tip, tmd) 1.123x. Serving 530/563 = 0.94x. `ru_oublock` is the
   task's `write_bytes >> 9`, charged when a page turns dirty and never reduced by later
   deletes, so "dirtied" counts what write_amp.sh says it counts. The alloc log has 11,621
   lines for 11,620 queries sent: exactly one stdin read per query, so allocs.py's deltas line
   up.
4. Risk 2/4, A1. `a1_verdicts.sh` reruns query_verdicts.py at 42e1c0a on the four jobs:
   after merging 18 pass, 2 caveat, 14 miss, 6 straddle; before 6, 2, 24, 8; per-job tallies as
   the report gives them. Miss ranges 1.023x-1.242x (13 of 14 against Tantivy) and 1.016x-1.603x
   match. `a1_per_job_diff.sh` lists the rows whose class changes between pooled and single-job
   scoring. `a1_jvm_drift.py` prints every engine's median ratio for each passing row.
5. Risk 3. tools/parity.cc's non-delete path is unchanged in substance: same 1e-9 relative score
   tolerance, same tie rule, counted and pruned results both compared, and now also failing on
   zero queries; single-segment and PARITY_REORDER runs take the same path as before. The
   deletes mode is new: counts exact before the merge, everything exact after it. Probe job
   261111-175040-397996 (`parity_deletes_probe.sh`, `parity_deletes_probe.py`, macOS, at
   0aa68f4) adds three checks to the pre-merge deletes pass, which it lacks: pruned top 10 equal to
   counted top 10, every hit a live docid in 1..N, and no repeats. 419 of 419 pass at 4 and at 16
   segments, and 347 of 419 keep the oracle's docid order (scores drift by up to 5.4%).
6. Risk 4. The report states the 14 of 40 after-merging misses, the 24 of 40 before, the
   eight-thread 1.03x scaling at 3.3x the CPU, and the 2.00x, 1.56x and 1.52x budget peaks as
   misses, with magnitudes matching the job files.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | query_verdicts.py labels a pass "caveat" only when its binding engine is a JVM one. `TOP_1000` global after merging is bound by Tantivy at 0.906x [0.897-0.967], but Lucene `-bp`'s median is 0.901x. The report's own rule ("a class beating a JVM engine by less than 10%") makes that row within the caveat, not a pass, so the after-merging count is 17 pass and 3 caveat, not 18 and 2. The row also belongs in the list of passes that the ~18% warm-up drift leaves unestablished. This leans toward prospect, but A1 is carried as a miss either way. | `a1_jvm_drift.py`; 42e1c0a `bench/query_verdicts.py` (the `bind` and `caveat` lines) | Apply the caveat to the largest JVM ratio, not to the binding engine; correct the counts and the drift list in the report |
| F2 | no | nit | "Each job alone gives the same verdicts" is false row by row: 6 after-merging rows and 8 before-merging rows change class between the pooled score and either job alone. With two trials the interval is just the min-max range. The per-job tallies printed next to the sentence are correct. | `a1_per_job_diff.sh` | Say that the conclusion holds and that those rows move between straddle, miss and pass |
| F3 | no | gap | The f0d7871 after-merging jobs leave plain Lucene (`lucene-10.4.0`) out, yet the target and the report score against "the fastest of Lucene, Lucene `-bp`, Tantivy". The reason, "Lucene without reordering sets no row's ratio, within 0.01", appears only in the 9b88263 section. Which baseline binds depends only on the baselines, so that reason carries over, and no pass's upper bound is within 0.01 of 1.0 (the largest is 0.981). | job specs 261111-072711-f9506e, 261111-023613-74ba91; report lines 66-69 | One sentence in the f0d7871 section citing it |
| F4 | no | gap | Before the merge, the parity deletes pass checks counts only. Top-10 differences are printed but never fail the run, so a pruned top-k that returned a deleted document would pass. Probe 261111-175040-397996 shows the property holds at this commit. | tools/parity.cc `pass(..., exact=false)`; probe job | Add the probe's three checks (pruned equals counted, live and unique docids) to the deletes pass so the tool enforces them |
| F5 | no | nit | gate3_report.py's `queries()` groups files by the `q1` and `q2` stem across job directories, and gate.py's `trial()` keys queries by text. Pooling two jobs through it therefore folds four trials into two, and the later file's times overwrite the earlier one's. The report's pooled numbers came from query_verdicts.py with explicit trials, so they are unaffected. The tree that lands also lacks 42e1c0a's eight-thread scoring fix. | bench/gate3_report.py `queries()`; bench/phase2/gate.py `trial()` | Key trials by job directory and stem; land 42e1c0a as planned |
| F6 | no | nit | allocs.py rejects only too few log lines (`<`). An engine that issued several stdin reads per query would misattribute the deltas without any error. This run has exactly 11,621 lines for 11,620 queries, so its numbers stand. | bench/allocs.py (`len(marks) < len(sent) + 1`); 261111-072649-debd0a `allocs.log` | Require `len(marks) == len(sent) + 1` |
| F7 | no | nit | The 9b88263 section's Tantivy-st numbers (5M 870 s, "2.06x faster"; 1M 0.367x) and R11's "the same code at one thread" come from the stale builder the f0d7871 section describes, but that section does not say so. The 5M "unmerged" Tantivy-st index the before-merging rows read was built by the same binary. It has 1 segment, the layout that favors Tantivy, so it is conservative against prospect. | report lines 16-55, 169-176 | Label those rows as from the stale builder |
| F8 | no | gap | On eight threads Tantivy-st does not scale: 85-86 s wall against 86-87 s on one thread, with 206 s of CPU against 105 s. Its builder parses JSON and issues `delete_term` on one thread, as the benchmark game's Tantivy builder does. The report's "both baselines scale worse on this corpus" is therefore not established for Tantivy. The row's pass rests on Lucene-st, which parses in all eight workers, so the verdict stands. | 261111-072715-8d41f8 unit files; bench/engines/tantivy-0.26-st/src/bin/build_index.rs | Say the eight-thread row is bound by Lucene-st and that Tantivy-st's time is its builder's single feeding thread |
| F9 | no | nit | With PARITY_REORDER and PARITY_DELETES both set, `open_snapshot(true)` marks the one reordered segment as deleted. The gate never sets both. | tools/parity.cc, the reorder block and then `open_snapshot(every != 0)` | Reject the combination |

Risks 1, 3 and 5 are answered with no blocking finding. Risk 2's tools compute what they claim, and
every number recomputed matches the job files; the only error is the F1 classification. Risk 4
holds: every named miss is stated as a miss with its measured magnitude.

## Not run

No timing, no rebuild of the engine or the baselines, no query trials. I did not check A7 to
A11's artifact rows beyond the numbers above, the FreeBSD smoke (A8), CI at 0aa68f4, or
whether Tantivy with parallel parsing beats prospect on eight threads (it would need a
benchmark).
