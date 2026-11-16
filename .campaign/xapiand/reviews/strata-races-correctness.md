# strata-races review: correctness

Commit: strata:2a4d2aa0ccde
Verdict: CHANGES
Date: 2026-11-11

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

The head differs from fb15069a only in `docs/perf/flakes.md`, so the contributor's fb15069a jobs
stand for the code at 2a4d2aa0:

- A1: burst_closes under load 0 of 160 twice at the head (261111-065359-06bdf8,
  261111-065400-8620b9); 7 of 160 at main (261111-062817-1fabdf, `--fails-without`).
- A2: hook test passes at the head (261111-065349-3912eb) and fails with main's `resource_budget.hh`
  (261111-065351-d0c5c6); TSan load 0 of 160 twice (261111-065354-020159, 261111-065355-a0477c).
- A5: `catalog_relief` passes in the suite (261111-065032-2e5b27, 75/75) and fails at `close.end`
  with cf76b825's `catalog.hh` (261111-065033-c4aeb6).

Not rerun. CI at 2a4d2aa0 was still running when I looked; it is the lead's gate, not this review's.

## What I checked myself

Scripts in `.scratch/review-strata-races-correctness/` (home checkout); each job runs at
strata 2a4d2aa0 and patches its own checkout. Superseded: 261111-090047-ed7ff8 (cancelled; it ran
`catalog_s8_test` without ctest's `S8_SCALE=20`).

1. Risk 1 (`note()`, `closing_over()`, every close begin), by reading against the happens-before
   edges. Each counter pair that one event moves in opposite directions is written raising half
   first, with the lowering half a release RMW, and read lowering half first, with acquire: a close's
   end (`closing_` then `live_`, `catalog.hh:1059-1061`, read `live_` then `closing_`,
   `:1491-1492`), and a due close's begin (`release_due` then `note()` at all six begin sites:
   `:747`, `:819`, `:853`, `:2090`, `:2337`, and reopen; read `closing_` then `pending_open_`).
   Every write to `live_`, `closing_`, `used_` and the counts is an RMW, so a reader that loads a
   later value still synchronizes through the release sequence. The pin paths that call
   `note()` before `release_due` (`:1304-1306`) move neither `live_` nor `closing_`. A begin's
   `release_due` before `book()` leaves a moment with both bookings out (an over-count), never both
   in. I found no interleaving of these three counters that ends a pass short or counts a close twice.
2. Risk 2 (`tick()` against `note_failure()`), by case split on the single total order of the four
   seq_cst accesses: either the re-read follows the stamp (take-back; a failure that lowered below the
   raise makes the take-back's compare-exchange fail and keeps its own value) or the failure's load
   follows the raise (it lowers from the raised cap when its count is below). The take-back can only
   undo this pass's own raise: raises happen only under `mu_`, failures only lower. The deterministic
   hook test asserts the exact cap (70) and that the hook ran. Probe `probe_takeback.sh`, job
   261111-090049-d5dd3f: the take-back turned into a plain store (overwriting a lower cap) fails the
   suite test 32 of 100 runs (head 0 of 100), caught only by the concurrent rounds at
   `resource_budget_test.cc:500`.
3. Risk 3 (relief against drops, every path), by instrumentation: `patch_catalog.py` prints
   RELIEF-STALE wherever a closing slot's booking is taken back while it exceeds what the slot still
   holds (the state in which `closing_over()` subtracts relief for counts already gone).
   `probe_relief.sh`, job 261111-092114-7bb6ed (all nine catalog tests): at the head, stale at the
   entry of `unbook_for_close()` in catalog (3), catalog_stress (1), catalog_wake (2) and
   catalog_sweep (1). `patch_prep.py` / `probe_prep.sh`, job 261111-091401-b2c07b, places probes
   through the close chore: catalog_test first goes stale right after the warm-with-leftover-maps
   `demote()` + `reclaim(0)` in `run_close` (`catalog.hh:2000-2001`: booked 2 descriptors 3 vnodes,
   held 1 and 1); catalog_stress is stale already when `run_replace`'s chore starts (booked 3 and 3,
   held 2 and 2), so the drop came between the begin's `book()` and the chore (a drain or a fenced
   replace), outside any close chore.
4. Risk 3, the replace site: same job, the head with `run_replace`'s `unbook_for_close()` removed.
   Every catalog test still passes (nine of nine exit 0) while the booking outlives the slot's counts
   until `finish_close` 1,296 times in catalog, 526 in catalog_stress, 27 in catalog_host and 555 in
   catalog_teardown: the replace path is exercised, and no test checks its relief.
5. Risk 4: the new and changed tests assert values: the exact cap and `!hook`
   (`resource_budget_test.cc:479-482`), and in `catalog_relief` the booking against the slot's
   counts at every hook site, `sites > 0`, `pass_closes == 1`, the booked descriptors equal to the
   slot's, and both bookings back to 0 with one open slot.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | yes | defect | A5 holds for `close()` and `retire` but not on every path: a closing slot's booked relief still outlives the counts it covers whenever the slot drops descriptors or vnodes before `unbook_for_close()`: in the close chore's preparation (`run_close`'s `demote()` + `reclaim(0)` at `catalog.hh:2000-2001`, and by the same shape `commit_merge_output`, the bound commit and `reclaim()`), and between the begin's `book()` and the chore (seen on the replace path: a drain's or a fenced replace's drops). Meanwhile `closing_over()` subtracts relief for counts already gone and a pass can end short, the defect class A5 fixes. The comment at `catalog.hh:1084-1085` ("never subtracts relief for counts already dropped") and flakes.md "Descriptor and vnode relief" claim more than the code does. | jobs 261111-092114-7bb6ed (stale at `pre-close` in catalog 3, stress 1, wake 2, sweep 1), 261111-091401-b2c07b (first stale site per path) | Options: (a) make the booking track drops: cap what `closing_over()` can subtract per slot by what the slot holds, e.g. lower `pend_desc`/`pend_vnodes` (and the global sums) as a closing slot's counts fall, so relief never exceeds holdings at any point; (b) unbook at the chore's start and book a replace only when its drain ends, accepting an over-count for the chore's preparation (safe direction; a pass may begin extra closes meanwhile); (c) keep the code and narrow the claim in the comment and flakes.md to the `close()`/`retire` window, with the residual named under "Open" (the lead's call, since A5 asks for "no stale order"). Test: extend `catalog_relief` with the warm-with-leftover-maps close (catalog_test's case) and a replace with a draining holder, sampling the booking against the slot's counts where those drops happen, and failing at the head. |
| C2 | yes | gap | Half of the A5 fix has no failing test: removing `unbook_for_close()` from `run_replace` (`catalog.hh:2128`) passes every catalog test, although the booking then outlives the counts on 2,404 replace closes across four tests. `catalog_relief` drives only a sweep close through `run_close`. | job 261111-092114-7bb6ed (variant `no-replace-unbook`: 9 of 9 tests exit 0) | Add a replace case to `catalog_relief` (a fenced index, or a reopen with a holder) sampling the booking against the slot's counts at every hook site; it must fail with line 2128 removed (a `--fails-without` job). Can share C1's test. |
| C3 | no | gap | The interleaving "raise, then a failure lowers below it, take-back fails" has no deterministic test; only the concurrent rounds reach it (a store-based take-back fails 32 of 100 runs). A test hook between the raise and the stamp re-read would force it. | job 261111-090049-d5dd3f | Optional: a second hook point in `tick()` after the raise. |
| C4 | no | nit | The IO's own directory descriptor and vnode drop in `retire` (`s.io.reset()`), posted to another chore with no release fence of its own; flakes.md says the drops are in `close()`. The order still holds when `close()` dropped anything after the fence (every later RMW on `used_` and the counts is in its release sequence), and on hardware regardless. | `catalog.hh:2175-2183`, `counted_io.hh:158-163` | Say so in flakes.md, or put a release fence before `finish_close`'s drops in `retire`. |
| C5 | no | nit | `note()`'s release on `closing_.fetch_sub` (a close's end) is not needed by the argument (the `live_` release after it carries that edge); the comment's reason fits only the `fetch_add`. | `catalog.hh:1058-1059` | Optional: relaxed on the sub, or reword. |

## Not run

- No TSan build: the risks were settled by reading and by Release-build instrumentation; the
  contributor's TSan load runs stand.
- No load run of `burst_closes` with a descriptor or vnode cap, so C1's effect on a pass (one close
  short) is argued from `closing_over()`, not measured; the instrumented states are what it reads.
- Linux, FreeBSD and x86_64: CI's. My probes ran on this macOS arm64 host only.
- Xapiand-side callers of the catalog: not in this quest.

## Re-check

Commit: strata:7b579648ac1b
Verdict: CHANGES
Date: 2026-11-11

Scope: 2a4d2aa0..7b579648 only (the booking moved into `resource_counts::booked`, lowered in
`resource_reservation::release()`; `unbook_for_close()` removed; `catalog_relief` gains a replace
case; `resource_budget_test` gains `booking_falls`).

### Evidence I relied on

- C2 fails-without, 261111-110831-f89967 (exit 0, built and ran): with 2a4d2aa0's two headers the
  sweep case passes and the replace case fails at `catalog_relief_test.cc:101`, "booked 2
  descriptors 4 vnodes, slot holds 2 and 3" from `holder.reclaim` through `chore.replace`. The
  right reason: the drain's drops outlive the booking until the replace chore.
- C1 probe, 261111-110833-b55465 (exit 1): stale 0 at every site in eight of nine tests; the warm
  close's `after-demote-reclaim0` was reached in catalog_wake (2) and catalog_sweep (1),
  `after-bound-commit-demote` (3, 1), `after-reclaim` (3), `replace-chore-start` (533 in stress,
  555 in teardown). It exits 1 because catalog_test did not build (C7), so catalog_test, where the
  first review saw the warm close go stale, is not covered by it.
- Suite, 261111-110829-ecdfb8: failed (C7). A2, 261111-110836-43a905: failed, `booking_falls` at
  `resource_budget_test.cc:551` in 8 of 200 rounds (C6).
- The A1 and TSan load jobs (c0e9c3, d29e6d, 5573ed, 0bff87) were still running or queued; not read.

### What I checked myself

1. Risk 1, `book()` against one `release()` (by reading): a store-buffering pair, all four accesses
   seq_cst (`resource_budget.hh:89`, `:104-105`, `:418`): either the release's load of the booking
   follows `book()`'s store (it lowers) or `book()`'s load of the count follows the drop (it lowers);
   a stale relaxed `c` is corrected by that lowering. Every change to a slot's booking is a CAS,
   exchange or store-from-0, each matched by one RMW on the holder's sum, so no update is lost and
   the sum ends equal to the slots' bookings. The sum dipping during `book()` is an over-count.
   This part holds.
2. Risk 1, two lowerings of one slot (probe `probe_two_droppers.sh` + `patch_two_droppers.py`, job
   261111-111453-ea2d6a, strict): it fails. `lower()` CASes the slot's booking and only then
   subtracts from the holder's sum (`resource_budget.hh:107-108`), and `release()` fences only when
   its own `lower()` lowered (`:422`). A drop whose booking another thread already lowered (a second
   release on the slot, or `book()`'s own `lower()`) finds nothing to lower, skips the fence and
   drops the catalog count (`:424`) while that other thread's holder subtraction is pending or not
   ordered before it. A reader then sees the count fall with the sum still above it: the state A5
   rules out. In sequential terms: R1 drops the slot 64 to 63, R2 drops it to 62 and CASes the
   booking 64 to 62, R1 sees 62 <= 62 and drops the catalog to 63, the reader reads 63 against a
   sum of 64. Two threads dropping 128 reservations of one booked slot, a reader as `closing_over()`
   reads: head 25 to 78 bad reads per run (of 42-46 M, 5 of 5 runs); a yield between the CAS and the
   subtraction, 0.6 to 1.1 M per run; holder-first lowering (subtract, then a release CAS, add back
   on failure) with `release()`'s fence unconditional, 0 of 106-126 M per run even with the yield.
   The contributor's own `booking_falls` hitting it with one dropper fits the `book()` case.
3. Risk 2, every drop path: the slot and catalog counts are written only by `reserve()`
   (`resource_budget.hh:263-264`) and `release()` (`:414-427`), plus the leak-on-destroy zeroing,
   which zeroes `booked` too (`catalog.hh:2450-2451`, `:2460-2461`); every reservation the catalog
   makes carries both counts (`catalog.hh:1563`). Drain, merge commit, warm demote + `reclaim(0)`,
   bound commit, `reclaim()`, `close()` and `retire` (the IO's directory descriptor included) all
   drop through `release()`. No bypass. The bookings end at `end_at` (`catalog.hh:1720`),
   `after_close` (`:2154`) and `:2237`.
4. Risk 3: C2's case fails at 2a4d2aa0 for the right reason (above). The C1 probe reaches the warm
   close's preparation only in catalog_wake and catalog_sweep (3 visits, stale 0); catalog_test, the
   test that showed it, did not run.
5. Risk 4: the demotion's maps relief (`pend_maps`, `pending_maps_`, read by `pass()`'s `over_maps`
   at `catalog.hh:624-625`) is outside A5, which names `closing_over()`'s descriptor and vnode terms.
   But flakes.md does not name it: "maps" appears there only in the list of the close chore's drops;
   the residual is in the journal (item 10) only.

### Findings (re-check)

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| C1 | no | defect | Closed for the state window: the booking now falls at the one drop point, on every path (check 3); the long-lived stale booking the first review saw is gone (stale 0 across eight tests). The residual race in the new mechanism is C6. | jobs 261111-110833-b55465; `resource_budget.hh:414-427` | Closed once C6 and C7 are fixed and the probe's catalog_test row runs stale 0. |
| C2 | no | gap | Closed: `replace_with_draining_holders` (`catalog_relief_test.cc:77-105`) fails with the fix reverted, at the drain's drops. | job 261111-110831-f89967 | None. |
| C6 | yes | defect | A drop whose booking another thread lowered publishes the catalog's lower count without ordering against that lowering's holder subtraction: `lower()` subtracts from the sum after its CAS (`resource_budget.hh:107-108`) and `release()` fences only when it lowered (`:422`). `closing_over()` then subtracts relief for counts already gone, for a moment: the A5 class, reached with two concurrent drops on a closing slot or a drop racing `book()`. The contributor's `booking_falls` fails at the head. | job 261111-110836-43a905 (`resource_budget_test.cc:551`, 8 of 200 rounds); job 261111-111453-ea2d6a (head 25-78 per run, 5 of 5; fixed variant 0) | Options: (a) lower the holder's sum first, then the slot's booking with a release CAS (add back on failure), and fence in `release()` whenever a booking was seen (the probe's `fixed` variant); (b) fence unconditionally and have a lowering that finds the booking already lowered wait out or redo the other's subtraction. Test: `booking_falls` passing, plus a two-dropper round (as in `patch_two_droppers.py`), in the suite and the TSan load run. |
| C7 | yes | defect | The suite does not build: `catalog_test.cc:1057` `static_assert(cat_t::slot_bytes() <= 192)` fails at 200 B. `booked[3]` (12 B, `resource_budget.hh:69`) replaces `pend_desc`/`pend_vnodes` (4 B), so every slot grows 8 B. | job 261111-110829-ecdfb8; job 261111-110833-b55465 (`catalog_test: build failed`) | Options: (a) book only descriptors and vnodes on the slot, in 16 bits (saturating, as before), with the holder's 64-bit sum kept outside the slot's struct; (b) find the 8 B elsewhere in the slot; (c) raise the 192 B bound, which is D4's slot-cost target and the lead's or owner's call with numbers. Test: the suite builds and passes. |
| C8 | no | gap | The demotion's maps relief keeps the stale-booking shape; outside A5's claim, but flakes.md does not name it, only the journal. | `catalog.hh:624-625`; flakes.md "Descriptor and vnode relief" | Add a line to flakes.md's "Not covered". |
| C9 | no | nit | `replace_with_draining_holders` prints "ok" after a failed CHECK (seen in f89967's log). | `catalog_relief_test.cc:104` | Print only when `sm.over == 0`. |

C4 is addressed by flakes.md (retire's drops lower like any other, through `release()`'s fence) and
C5 by the relaxed fall at `catalog.hh:1059`; C3 stays an optional note.

### Not run (re-check)

- No TSan build; the contributor's TSan load runs were still queued.
- The fixed variant ran only the two-dropper case, not `booking_falls`; it shows the shape, not the
  fix.
- catalog_test under the C1 probe (it does not build at this commit).
- Linux, FreeBSD and x86_64: CI's.
