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
