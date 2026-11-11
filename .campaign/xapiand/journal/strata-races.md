# strata-races: journal

State: fixes ready: C6, C7 with jobs 261111-121449-2bc470 261111-121450-3218fe 261111-121453-bb533f 261111-121510-fbd377 261111-121513-28aadf 261111-121515-de9f0a 261111-121519-849eaa 261111-121521-62916a
Quest: .campaign/xapiand/quests/strata-races.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Second review fix round (re-check at 7b579648: CHANGES, C6 and C7 blocking). Head strata 22a9a750
(main merged), pushed for CI. C6: every booking fall (`lower()`, `unbook()`, one helper `fall()`)
subtracts from the holder's sum first, then moves the slot's booking by a release CAS (seq_cst on
failure), adding the sum back when it loses; `release()` fences after `lower()` whether or not it
lowered. `unbook()` had the same window (exchange to 0, then subtract), fixed the same way. C7:
`booked[2]` (descriptors, vnodes; maps relief stays the catalog's), slot back to 192 B (catalog_test's
static_assert builds). C8: flakes.md names the maps relief gap. C9: both relief cases print ok only
with no failed CHECK. New suite case `two_droppers`; `BOOKING_ONLY=1 ROUNDS=N` runs the two racing
cases alone (`docs/perf/flakes/booking_load.sh`, the reviewer's two-droppers probe adapted: it adds
a case the suite now has).

## Next

1. Lead: when the jobs in State pass, `campaign accept strata-races correctness --confirm
   C6=261111-121510-fbd377 C7=261111-121449-2bc470` (suite 261111-121449-2bc470; C6 failed at
   7b579648: 261111-111453-ea2d6a, 261111-110836-43a905).
2. A4: CI at 22a9a750.

## Blocked on

Nothing.

## Evidence

A1 burst runs overwrite test/catalog_stress_test.cc in the job's checkout with the exploration's
(`26dc030e`, burst mode); that file is outside the file set. A2's mutant (main's resource_budget.hh)
is not rerun at 7b579648: `booking_falls` uses the new API, so it would fail to compile, a vacuous
mutant; 261111-065351-d0c5c6 stands for the tick() fix, whose code is unchanged.

| Criterion | Evidence |
|---|---|
| A1 | head 22a9a750, strict 2 x 10x16: 261111-121513-28aadf, 261111-121515-de9f0a (build 261111-121512-bac43b); main 7 of 160, --fails-without 261111-062817-1fabdf |
| A2 | head 22a9a750: TSan strict 2 x 10x16 261111-121519-849eaa, 261111-121521-62916a (build 261111-121517-cbfb41); booking cases 2000 rounds x 5, strict, 261111-121510-fbd377. Fails without: 261111-062645-154f0d, 261111-065351-d0c5c6 |
| A3 | [artifact] `include/strata/core/catalog.hh` (note(), closing_over(), each close begin) and `include/strata/core/resource_budget.hh` (note_failure(), tick(), resource_counts::book()/lower()/fall()/unbook(), release()): one-line ordering comment at each changed atomic |
| A5 | suite 261111-121449-2bc470; booking_falls + two_droppers 261111-121510-fbd377 (at 7b579648 they fail: 261111-110836-43a905, probe 261111-111453-ea2d6a); C2 fails without (2a4d2aa0's headers) 261111-121453-bb533f; C1 probe on all nine catalog tests 261111-121450-3218fe |
| A4 | suite 261111-121449-2bc470; CI at 22a9a750 (pushed) |

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
the fb15069a A2/A5 jobs for the code at 7b579648; the 7b579648 jobs (C6, C7) for 22a9a750's.

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
11. C7 shape (lead ruled 16-bit slot fields with the sum outside the slot's struct): I kept one
    `booked[2]` of int32 on `resource_counts`, the slot's booking on a slot and the sum on the
    catalog's counts (as `n` is). The literal shape needs the sum reachable from `release()`, which
    has only `resource_counts* catalog_` (counted_io.hh, outside the file set, passes that type):
    a separate sum means a downcast or new plumbing. Dropping the maps booking takes the struct from
    48 to 40 B and the slot from 200 to 192 B, D4's bound; 16-bit fields would buy 8 B more only with
    the sum moved out (no saturation bound needed at 32 bits). Lead: say if 184 B is worth the cast.
12. C6 covers `unbook()` too: its exchange-to-0-then-subtract had the reviewer's window against a
    drop (a drop seeing booking 0 skips lowering while the sum still holds it), so `lower()` and
    `unbook()` share `fall()`.
