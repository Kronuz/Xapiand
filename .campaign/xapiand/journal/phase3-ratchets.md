# phase3-ratchets: journal

State: waiting on jobs: wave 2 and the 8-thread row (ids under Doing), build-test 261110-023715-edf507 at prospect `3e92ea9`, FreeBSD smoke 261110-023726-f5b60e; all queued behind 97 others at 02:34
Quest: .campaign/xapiand/quests/phase3-ratchets.md (the lead's; never edit it)

## Doing

Rebased onto main `e8e5bd9` (main changed docs/design/automata.md only; `git diff 9b88263 e8e5bd9`
is that file, so the measured engine stays `--ref main=9b8826349cfe`, R7). Harness at prospect
`ba6a400` (adds `gate3.sh wamp`, forwards PROSPECT_AUTO_MERGE). build-test 261110-021707-040070.

Wave 2 (gate VM, engine main `9b88263`):
- merge 261110-022415-2c93a9 (strict) -> sections 261110-022417-832326, verify
  261110-022419-0d97c8, allocs 261110-022420-407111 (light) -> servemem 261110-022422-c56975
  (strict: prospect-merged, Tantivy, Lucene, Lucene -bp).
- A4 probe 261110-022424-057569: 5M at 2,056 MB with PROSPECT_AUTO_MERGE=0 (strict).
- A3: wamp smoke 20k 261110-022426-648c03 -> append 1M 261110-022427-e83b7c -> replace 1M
  261110-022429-f5a5aa (strict, chained so no other VM job writes the device meanwhile).
- A1 after merging QA1-5: 261110-022431-02bb66 022436-743e61 022442-e18d7a 022448-7e7c9b
  022453-b2d8f9; before merging QB1-5: 022433-017540 022438-9a0901 022444-671c80
  022450-fc6501 022456-0b4e2c (exclusive; queries-sample 181 + terms-sample 40, WARMUP 20).
- A2 1M trials I1-5 (exclusive, prospect at 1,039 MB vs Tantivy-st, order alternating):
  022435-14ea61 022440-084be4 022446-d9749a 022452-3e9f96 022457-9bd18c (ids prefixed 261110-).
- A7 coverage on the host 261110-022513-292125 (light).
- A2 8-thread row (harness `61be43a`: LUCENE_THREADS, TANTIVY_THREADS): prepare 261110-023211-4c1f94
  (after the I and QB jobs) -> smoke 20k 261110-023212-dd083e -> trials M1-5 at 1M, exclusive,
  order rotated: 261110-023213-439939 023215-c412cf 023216-903d9c 023218-de1101 023219-e92006;
  5M confirmation 261110-023221-07f41b (strict).

## Wave 1 read (jobs passed; engine main `9b88263`, gate VM, Linux x86_64, 8 vCPUs)

- Smoke 261109-174807-1b0f95 passed every step at 20k. Parity 261109-174809-eb0263: 419/419
  counts and top-10 at 1, 4, 16 segments; with deletes (5 and 17 segments) counts 419/419, top-10
  157/419 (weights drift, R5), exact again after the merge drops them (419/419, 3.87e-16).
- A2 5M keyed one thread, single runs (screen strict, not the confirm stage):
  prospect 423.3 s at 2,056 MB, 3 segments, peak RSS 3,221 MiB, anon 3,204 MiB;
  Tantivy-st 870.0 s, 1 segment, RSS 11,357 MiB (anon 8,573); Lucene-st 1,162.2 s, 3 segments,
  RSS 4,588 MiB. prospect 2.06x faster than Tantivy-st at 28% of its peak RSS, 2.75x faster
  than Lucene-st at 70% of its peak. CPU: prospect 376 s user+sys, Tantivy 1,002 s, Lucene 1,344 s.
- Calibration drift: 2,056 MB peaked at 4,608 MB on 2026-10-17 (Lucene-st 4,593); now 3,221 MiB
  against Lucene-st's 4,588 MiB, 30% under. Not recalibrated (R6).
- A4 indexing: peak RSS 1.57x the 2,056 MiB node budget, past budget + 10% (2,262 MiB): a miss
  (D17 notes merge buffers are not charged yet; the probe tests that).

## Next

1. Read wave 2; score A1 with `bench/phase2/gate.py --md --target 1.0 --prospect prospect-merged`
   (after) and `--as lucene-10.4.0-st=lucene-10.4.0 --as tantivy-0.26-st=tantivy-0.26` (before).
2. Write A11's report rows; A9 tool list; A7 parity.md rows with the coverage numbers.
3. Owed and not submitted: the many-index rows (A3: no harness), A8 FreeBSD/arm64 (deferred).
4. A9 runner: `bench/gate3_report.py DIR...` (prospect `b97171d`) scores all four axes.

## Blocked on

Nothing. The lead ruled on all six proposals (D51, Xapiand `.campaign/xapiand/PLAN.md`, 2026-11-10):
1. C15 takes allocation counts at current code. 2. C7 carries to the next phase. 3. C9 and
C15's rest go to a follow-up quest. 4. A4's indexing budget miss (1.57x, limit 1.1x) stays a
miss in the gate doc; the fix (charge merge buffers) is a follow-up quest; state probe
261110-022424-057569's result when it lands. 5. A3's many-index rows go to a harness quest
(high priority). 6. A8: FreeBSD as a deferred check set by the lead; arm64 waits for a local host.
Folded into `docs/gates/phase-3-ratchets.md` at prospect `3e92ea9`.

A8 deferred check, the command for the lead once smoke 261110-023726-f5b60e passes (light:
the first run builds on the VM's 4 vCPUs; its times are informational, prospect only, since
the VM has no rust or JDK and 4.5 GB free disk bounds DOCS):

    campaign job submit --quest phase3-ratchets --later --ref main=<landed main> \
      --level light --minutes 120 --env DOCS=500000 --env PROSPECT_BUDGET_MB=1039 \
      --what "A8 FreeBSD rows: prospect keyed 500k, merge, verify, parity" -- tools/vm/bsd_rows.sh

## Evidence

| Criterion | Evidence |
|---|---|
| A2 | partial: 5M confirmation jobs 261109-174917-a48a55, 261109-174919-b26fd0, 261109-174921-0cc8d3 (2.06x faster than Tantivy-st); trials I1-5 and the 8-thread row queued |
| A6 | partial: parity 261109-174809-eb0263 (1/4/16 segments, deletes at 5 and 17, merged); verify 261110-022419-0d97c8 queued |
| A8 | decision D51 (FreeBSD deferred, arm64 waits for a host); driver `tools/vm/bsd_rows.sh` (prospect `3e92ea9`), smoke 261110-023726-f5b60e queued |
| A9 | `docs/gates/phase-3-ratchets.md` "Tools" (prospect `7206c0f`) |
| A10 | decision D51; `docs/gates/phase-3-ratchets.md` "Phase 2 gate conditions" (prospect `3e92ea9`): C1, C3, C5, C13, C14 closed; C15's allocations at current code (allocs job 261110-022420-407111); C2, C16 partly; C7 carried; C9 and C15's rest to a follow-up quest |

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
