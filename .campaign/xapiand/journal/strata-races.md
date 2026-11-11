# strata-races: journal

State: waiting on jobs 261111-065349-3912eb 261111-065351-d0c5c6 261111-065354-020159 261111-065355-a0477c 261111-065359-06bdf8 261111-065400-8620b9 261111-062817-1fabdf
Quest: .campaign/xapiand/quests/strata-races.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Head strata fb15069a (catalog_relief in the suite). Suite 75/75 and A5 proven there. The CMakeLists
change made A2's earlier jobs "other code", so A2 and A1 rerun at the head (ids in State): A2
deterministic + mutant (light), TSan 2 x 10x16 strict (build 261111-065353-a3a24b, slot
races-tsan-head); A1 burst 2 x 10x16 strict (build 261111-065357-424afb). A1's main run stands.

## Next

1. When jobs finish: fill docs/perf/flakes.md Results, paste `campaign brief`'s table below.
2. A4: CI at the new head (the lead pushes; cf76b825 was pushed).
3. Set State to ready for review (correctness lens, concurrency).

## Blocked on

Nothing.

## Evidence

A1 and the burst runs: the job overwrites test/catalog_stress_test.cc in its own fresh checkout with
the exploration's (`git show 26dc030e:test/catalog_stress_test.cc`, burst mode + diagnostics); that
file is outside the file set (cold-tier-step2 owns it). The main-side run also takes the driver from
b7bd5251. A2's mutant: `--fails-without` job at the head with main's resource_budget.hh copied over.

| Criterion | Evidence |
|---|---|
| A1 | head fb15069a, strict 2 x 10x16: 261111-065359-06bdf8, 261111-065400-8620b9; main 10x16 --fails-without 261111-062817-1fabdf (build 261111-062806-177d38) |
| A2 | head fb15069a: 261111-065349-3912eb (deterministic); fails without 261111-065351-d0c5c6; TSan strict 2 x 10x16 261111-065354-020159, 261111-065355-a0477c. At b7bd5251: 261111-061317-5cd220 passed, 261111-062645-154f0d failed at :482, TSan 20x16 0 of 320 261111-061320-6fa85e (main 3 of 320, 261111-053734-7d63a2) |
| A3 | [artifact] `include/strata/core/catalog.hh` (note(), closing_over(), each close begin) and `include/strata/core/resource_budget.hh` (note_failure(), tick()): one-line ordering comment at each changed atomic |
| A5 | suite `catalog_relief` at fb15069a: 261111-065032-2e5b27 (75/75); fails without: 261111-065033-c4aeb6 (catalog.hh from cf76b825: at close.end 2 descriptors, 2 vnodes booked, 0 held) |
| A4 | 261111-065032-2e5b27 (suite 75/75 at fb15069a); TSan suite and CI: CI on push (lead) |

## Result

Two races fixed (catalog counter-pair orders; tick() take-back after raise). TSan load 0 of 320 (main 3 of 320); hook test fails without the fix. Burst numbers pending. How it works: strata `docs/perf/flakes.md`.

Superseded: 261111-061303-afa5e8 and 261111-061305-f25392 (zsh `$X:t` modifier mangled the sha),
261111-061319-5f0ba7 (mutant failed as intended but --fails-without wants the command inverted; it
failed at resource_budget_test.cc:482, the new hook check), 261111-062621-428fdf / 261111-062635-95c190
(cancelled: load runs must be strict, per the coordinator); 261111-062745-9a0936 / 261111-062757-f52802
(cancelled: catalog.hh's close path changed under them at bafafa9e). Load runs are now `--level strict
--minutes 10`, built first by a light job in the same slot.

## Rulings I made

1. Catalog, beyond the explorer: a due close's begin raised `closing_` before releasing its
   `pending_open_` booking, so `closing_over()` could count it twice and stop short (same class as
   the reported race). Every close begin now calls `release_due` before `note()`; `closing_` RMWs
   are release and its load acquire. No deterministic test: catalog tests are outside the file set.
2. tick(): seq_cst only on the store-buffering four (note_failure's stamp store and cap load,
   tick's raise and stamp re-read); acq_rel would need both stamp accesses as RMWs. First stamp load
   relaxed, take-back relaxed, cap lowering release / first cap load acquire, each argued in a
   comment.
3. The limit-change clear of the descriptor cap stays unconditional (failures under the old limit
   say nothing of the new one); I tried folding it into the undo and dropped it as unprincipled (a
   failure before the stamp read was still lost).
4. Stamp equality as "no failure since": the only ABA is a failure that read the clock at the old
   stamp and stalled a quiet period before storing; its failure is then quiet-old, so raising is
   correct.
5. The deterministic test also checks the hook ran (`!hook`), so it cannot pass vacuously.
6. repeat_load.sh exits 1 on any failure so the jobs' status is the evidence.
7. A5: the descriptors drop in `index->close()` (run_close / run_replace), not in `retire`, so
   unbooking in retire would not close the window; `unbook_for_close()` runs before `close()` with a
   release fence, and closing_over() reads both shares, an acquire fence, then the bookings. Fences
   keep budget `used()` and counts reads relaxed for every other caller. While close() runs the slot
   is neither booked nor dropped (over-count, safe); a refused replace retries unbooked (over-count).
   Not covered: a drop by another thread (a view released later on a stuck close) has no fence; the
   stuck path already unbooks in after_close.
