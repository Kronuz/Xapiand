# strata-races: journal

State: waiting on jobs 261111-061303-afa5e8 261111-061305-f25392 261111-061317-5cd220 261111-061319-5f0ba7 261111-061320-6fa85e 261111-061322-03cbd0
Quest: .campaign/xapiand/quests/strata-races.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Jobs running for A1, A2, A4 (ids in State). Code and docs committed on strata
quest/xapiand/strata-races (2ea3092b fixes, b7bd5251 driver exit status, 5e53b1a5 doc).

## Next

1. When jobs finish: fill docs/perf/flakes.md Results with their numbers, paste `campaign brief`'s
   table below; if the doc commit made evidence stale, rerun with `campaign verify` / resubmit.
2. A4's TSan and CI: CI (cpp-libraries cmake.yml, sanitizer matrix) runs on push; the lead pushes.
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
| A1 | 261111-061303-afa5e8 (head), 261111-061305-f25392 (main, --fails-without) |
| A2 | 261111-061317-5cd220 (deterministic, head), 261111-061319-5f0ba7 (main's tick(), --fails-without), 261111-061320-6fa85e (TSan 20x16, head) |
| A3 | [artifact] `include/strata/core/catalog.hh` (note(), closing_over(), each close begin) and `include/strata/core/resource_budget.hh` (note_failure(), tick()): one-line ordering comment at each changed atomic |
| A4 | 261111-061322-03cbd0 (suite); TSan suite and CI: on push (lead) |

## Result

<pending jobs>. How it works: strata `docs/perf/flakes.md`.

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
7. Found, not fixed (outside the scope): descriptor/vnode relief in `closing_over()` has the same
   wrong order (`retire` drops the index before `finish_close` unbooks; budget counters relaxed).
   Written up in docs/perf/flakes.md "Open"; a follow-up quest for the lead.
