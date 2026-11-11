# strata-races: journal

State: fixes ready: C1, C2 with jobs 261111-110829-ecdfb8 261111-110831-f89967 261111-110833-b55465 261111-110836-43a905 261111-110853-c0e9c3 261111-110855-d29e6d 261111-110859-5573ed 261111-110900-0bff87
Quest: .campaign/xapiand/quests/strata-races.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Review fix round (correctness review at 2a4d2aa0: CHANGES, C1 and C2 blocking). Head strata
7b579648, pushed for CI. C1 (lead: option 1): the close booking moved into
`resource_counts::booked` (slot booking, catalog sum); `resource_reservation::release()` lowers it to
the slot's count before the catalog count and budget drop, so every drop path (drain, merge commit,
warm demote + reclaim(0), bound commit, reclaim, close(), retire) lowers it; `unbook_for_close()` is
gone. C2: `catalog_relief` gains a healthy reopen whose draining holder merges, commits, reclaims
before the replace chore; at 2a4d2aa0 it fails (4 vnodes booked, 3 held from the holder's reclaim
to `chore.replace`; local run). C5 folded (closing_ fall relaxed, comment reworded). flakes.md
rewritten for the mechanism; results table filled from the finished fb15069a runs.

## Next

1. Lead: when the jobs in State pass, `campaign accept strata-races correctness --confirm
   C1=261111-110833-b55465 C2=261111-110831-f89967` (suite 261111-110829-ecdfb8).
2. A4: CI at 7b579648.

## Blocked on

Nothing.

## Evidence

A1 burst runs overwrite test/catalog_stress_test.cc in the job's checkout with the exploration's
(`26dc030e`, burst mode); that file is outside the file set. A2's mutant (main's resource_budget.hh)
is not rerun at 7b579648: `booking_falls` uses the new API, so it would fail to compile, a vacuous
mutant; 261111-065351-d0c5c6 stands for the tick() fix, whose code is unchanged.

| Criterion | Evidence |
|---|---|
| A1 | head 7b579648, strict 2 x 10x16: 261111-110853-c0e9c3, 261111-110855-d29e6d (build 261111-110851-1264ab); at fb15069a 0 of 160 twice (261111-065359-06bdf8, 261111-065400-8620b9); main 7 of 160, --fails-without 261111-062817-1fabdf |
| A2 | head 7b579648: 261111-110836-43a905 (clock hook, booking_falls); TSan strict 2 x 10x16 261111-110859-5573ed, 261111-110900-0bff87 (build 261111-110857-6f907c). Fails without: 261111-062645-154f0d, 261111-065351-d0c5c6. At fb15069a TSan 0 of 160 twice |
| A3 | [artifact] `include/strata/core/catalog.hh` (note(), closing_over(), each close begin) and `include/strata/core/resource_budget.hh` (note_failure(), tick(), resource_counts::book()/lower(), release()): one-line ordering comment at each changed atomic |
| A5 | suite at 7b579648: 261111-110829-ecdfb8; fails without (2a4d2aa0's two headers): 261111-110831-f89967; C1 probe (review's RELIEF-STALE on all nine catalog tests, exits 1 on any stale site): 261111-110833-b55465 |
| A4 | suite 261111-110829-ecdfb8; CI at 7b579648 (pushed) |

Review findings: C1 fixed (one mechanism at release(); confirm with 261111-110833-b55465 and the
replace case). C2 fixed (261111-110831-f89967). C3 not done (non-blocking: a second tick() hook
point; the concurrent rounds cover it). C4 resolved by C1: retire's drops, the IO's directory
descriptor included, go through release() and lower the booking behind its own fence. C5 folded.

## Result

Two races fixed (catalog counter-pair orders; tick() take-back after raise), and the close relief
falls with the counts at release(). TSan load and burst_closes 0 of 160 twice at fb15069a (main 7 of
160, 3 of 320). How it works: strata `docs/perf/flakes.md`.

Superseded: 261111-061303-afa5e8 and 261111-061305-f25392 (zsh `$X:t` modifier mangled the sha),
261111-061319-5f0ba7 (mutant not inverted), 261111-062621-428fdf / 261111-062635-95c190 (load runs
must be strict), 261111-062745-9a0936 / 261111-062757-f52802 (catalog.hh changed under them);
the fb15069a A2/A5 jobs for the code at 7b579648.

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
8. C1 fix shape (lead ruled option 1): the booking moved into `resource_counts` so `release()`, the
   one place counts drop, lowers it; per-caller patches at each drop site were the alternative.
   `booked` is int32 (both kinds are int-counted by the kernel) and signed: `book()` adds to the
   sum after its own lowering, so the sum can dip below the bookings (over-count), never above.
   book()/release() meet by seq_cst store-load on booking and count. Residual, in flakes.md: a
   reservation and a close begin both landing between the pass's two reads.
9. The warm-with-leftover-maps close has no deterministic case: at 2a4d2aa0 no hook site lies
   between the close chore's drops and `close()`, and I found no short setup that leaves an owed
   slot with a finished merge. Its drops go through the same `release()`; the C1 probe job checks
   the chore's preparation points in catalog_test, where the review saw it stale.
10. pend_maps/pending_maps_ (a demotion's maps relief) keep their own booking: moving maps onto
    `booked` would change demotion pacing, a separate change. Same staleness shape; worth a quest.
