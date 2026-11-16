# unpin-after-kick: journal

State: waiting on jobs 261112-090807-6824f4 261112-090816-e82be7 (and CI at strata:86fe7c47)
Quest: .campaign/xapiand/quests/unpin-after-kick.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Lead's message (CI 43260965516 at 441abc39): all green but linux-x86_64-gcc `catalog`, catalog_test.cc:965, the schedules'
coverage CHECK: `wake: reposts 0` over 300 seeds. The jobs at 441abc39 passed: TSan 261112-083911-63e3ed, ASan+UBSan
261112-083917-866b8a, mutant 261112-083923-f54687, build-test 261112-083928-dbd7e3.
- Cause: the sim's repost step (r == 89) acted only if an earlier random step had left a chore lease held on an `owed`
  slot; whether that happened in any of 300 seeds rested on draw order. The new `transition.ended` site draws on
  every visit (on_site's `rng() % 3`), which shifted gcc's schedules (gcc also evaluates `pinner(...)`'s rng
  arguments in another order than clang) to none.
- Fix strata:86fe7c47: the step makes its precondition when none is held (`hold_warm`: chore pin on an idle warm slot,
  demoting an idle hot one first, lease held), then sends the request. Draw order no longer decides it.
- Jobs: catalog_test probe 261112-090807-6824f4 (prints the repost count), build-test 261112-090816-e82be7, CI pushed.
  The sanitizer suites at 441abc39 build only catalog_teardown, catalog_sweep and catalog_stress, which do not use
  catalog_test.cc.


A3's linux catalog_stress abort (`resource_counts::book`: `booked[i] == 0`), fixed at strata:503df8f0, test reworked at 441abc39 (pushed; CI 43260965516 queued, 43231465982 cancelled by the push):
- Cause: end_at (catalog.hh, main ab0e26a6:1751) unbooked a closing slot after the exchange that left `closing`.
  A refused close ends `{0, open}`; a pass on another thread can begin_close it and book before that unbook runs
  (book asserts), and in NDEBUG the late unbook erases the new close's booking. Pre-existing since strata:7d031203.
  Not this quest's change: da983792/b04ca9e0 do not touch end_at, book or unbook.
- Fix: unbook before the exchange in end_at, and in finish_close the same way (there it was safe, free_slot and the
  open's post follow, but one rule). New hook site `transition.ended` after end_at's exchange.
- Test: catalog_teardown_test `refused-end`: a hot slot's close is refused (a map held) and ends at `open`; at
  `transition.ended` the hook begins the next close; at the end nothing may stay booked. Without the fix the late
  unbook erases the second close's slot booking but not its share of the catalog's sum, so `pending_descriptors()`
  stays at the slot's count after both closes end (local run at the mutant: that check fails, rc 1).
- Attribution, proven: probe 261112-082839-973970 at main ab0e26a6 with only a 1 ms sleep after end_at's exchange on a
  closing end: catalog_stress (asserts) aborts on book's assertion on run 1. Same sleep at the fix:
  261112-082842-4c0531 passed (10 runs, no assertion). Scripts: strata worktree .scratch/tsan-book/{suite,mutant,delay}.sh.
- Jobs at 441abc39: TSan+asserts suite 261112-083911-63e3ed, ASan+UBSan+asserts 261112-083917-866b8a, mutant
  (`--fails-without`) 261112-083923-f54687, build-test 261112-083928-dbd7e3. Superseded at 503df8f0 (the first test
  version's throw at `close.call` was never reached: the hot slot refuses before it): 261112-082829-50b165,
  261112-082833-a65a89, 261112-082836-a10320, 261112-082844-da8069.
- Earlier attribution: the failure hit linux-tsan (runs 43214565766, 43218222486, 43231465982 attempt 1) and
  linux-asan-ubsan (43225658870), always in catalog_stress; macOS TSan at main did not reproduce in 25 runs
  (261112-080635-a9cefb), consistent with a narrow window.

Earlier (review CHANGES folded in):

Review CHANGES (reviews/unpin-after-kick-correctness.md) folded in at strata:b04ca9e0 (pushed, CI running):
- F1: miss tail runs over_soft() before post(run_open); test miss_under_shutdown (the reviewer's probe as a
  `catalog_teardown_test miss` case). Mutant 261112-055258-a6bd1d (post then over_soft, ASan, must report
  heap-use-after-free).
- F2(a): mark_in_unpin_window (`mark-window`): shutdown begins at the `unpin` hook, between the unpin's load
  and its exchange; checks the unpin began the close, one open->closing and closing->free, shutdown ended.
  Mutant 261112-055301-475b83 (close decided from the first load).
- F2(b): new hook `shutdown.mark`; unpin_in_mark_window (`unpin-window`) releases the last pin there. Mutant
  261112-055304-392b68 (the mark returns instead of retrying).
- Re-audit (R3): also fixed acquire_impl's refusal/catch paths (uncount_miss posts the decrement and the
  completion) and unlink_waiter's counters (under the mutex). No deterministic failing test exists for these.
- Suite at the head: TSan 261112-055252-e509d2, ASan+UBSan 261112-055255-91db1f (all teardown cases + sweep),
  build-test 261112-055306-ba7c31.
- N3: pin_smoke A/B run 261112-051905-9ea016 (strict, at da983792; unpin unchanged since) still queued.
- Local TSan at b04ca9e0: `unpin`, `miss`, both window cases pass.

Earlier jobs at da983792, all exit 0: TSan 261112-051834-5fd5e6, ASan+UBSan 261112-051837-327568, A1 mutant
261112-051840-b0a9ca (ASan heap-use-after-free in over_soft() catalog.hh:1423 from the unpin), A2 mutant
261112-051843-666637 (soft_caps_post's `hot_indexes() == 1` check fails without unpin's over_soft()).

## Next

1. Read the six jobs above and linux-tsan CI at 503df8f0; then re-review of F1, F2, the N3 record and this fix.

## Blocked on

Nothing. N3, the lead's ruling: at most 3 tries, else record the cost and stop. Two tries:
- Try 1, strata:58f9e0ab, not kept: one inline compare-exchange, with the loop, mark and drain moved out of
  line. Strict job 261112-074010-73b976, by handle, medians of 3 interleaved runs:
  - base: 48.7, 54.3, 49.6
  - fix: 58.4, 62.2, 58.4
  - try 1: 61.4, 59.8, 55.5
  It recovered nothing, so the branch is back at b04ca9e0.
- Try 2, attribution probe 261112-075446-07de70 (unversioned): the old `fetch_sub` unpin body on the new
  header. By handle, minimum of 3 runs:
  - base: 46.1
  - try 1: 53.2
  - old body: 52.5
  The exchange is not the cost. The gap comes from elsewhere in the header, likely layout or inlining, and is
  not explained. The host's noise is large: one base median read 59.9.
I stopped there and recorded the cost in docs/mechanisms/catalog.md (strata:a0bc0430).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261112-055252-e509d2 (TSan), 261112-055255-91db1f (ASan+UBSan); fails without: 261112-051840-b0a9ca (ab0e26a6's catalog.hh, ASan heap-use-after-free in over_soft from the unpin), 261112-055258-a6bd1d (F1: heap-use-after-free in over_soft from the miss), 261112-055301-475b83 (F2(a): mark-window checks fail), 261112-055304-392b68 (F2(b): unpin-window checks fail) |
| A2 | 261112-055306-ba7c31 (build-test at b04ca9e0); fails without: 261112-051843-666637 (no over_soft() in unpin: soft_caps_post's `hot_indexes() == 1` fails) |
| A3 | 261112-055306-ba7c31 (build-test at b04ca9e0); CI 43225986515 green on every platform at da983792; CI 43231465982 at b04ca9e0: every job green but linux-tsan, whose catalog_stress aborted on `resource_counts::book`'s `booked[i] == 0` assertion. That failure is pre-existing: the same assertion fails in CI 43225658870 and 43218222486 on quest/xapiand/read-only-open, which does not change catalog.hh, resource_budget.hh or catalog_stress_test.cc from main (ab0e26a6). Job 261112-080635-a9cefb at main reproduces it under TSan with asserts (pending). The lead decides; CI on a0bc0430 is running. |

Review findings: F1 fixed (miss_under_shutdown, 261112-055258-a6bd1d); F2 fixed (mark_in_unpin_window,
unpin_in_mark_window, 261112-055301-475b83, 261112-055304-392b68); N1, N2 left (R6); N3 measured, two tries, cost recorded (Blocked on).

## Result

strata:da983792 + b04ca9e0 (include/strata/core/catalog.hh, test/catalog_teardown_test.cc; docs by the lead at
3adfdc27). The slot word's `shut` bit makes the last unpin's exchange its last touch of the catalog (or the
start of the shutdown close, whose post is last). The miss now runs over_soft() before posting the open. The
refusal paths pass their count back in a chore. The waiter counters go in under the mutex. Hot-path cost:
pin_smoke by handle 48.7 to 56.6 ns, by key 62.3 to 74.9, not attributable to the exchange (Blocked on);
recorded in docs/mechanisms/catalog.md "Shutdown".

## Rulings I made

- R1 The race is wider than `kick(); over_soft();`. Once unpin's decrement makes the slot `{0, open}`,
  shutdown's own loop (join_shutdown's kick of every slot, on the owner's thread) can close it and the
  host destroy the catalog while the unpinner is still in kick() (reading `shutting_`, the word, `stuck`).
  Reordering over_soft() before kick() leaves that window. Fix: the slot word carries a `shut` bit
  (`pins:24 | shut:1 | state:3 | freq:2 | gen:34`; 8 states fit 3 bits). shutdown_slot() marks a pinned
  slot `shut` with a compare-exchange; unpin() is a compare-exchange loop whose last-pin exchange, when
  `shut` is set and the slot is open/owed, goes straight to `{0, closing}` and posts the shutdown close
  (post is its last touch); without `shut` the exchange is its last touch. The word serializes the two:
  a mark that lands first fails the unpinner's exchange, which retries and sees it; an unpin that lands
  first leaves `{0, open}` for shutdown_slot to close. unpin no longer reads `shutting_` (which also
  retires the release/acquire store-load pairing it had with join_shutdown's exchange, formally short
  of B1's seq_cst). `with_state` keeps the bit, so transitions carry it; `make()` clears it on free.
- R2 over_soft() runs before the last pin's exchange (while the pin still holds the slot): its counts
  do not include the slot's pin, so the decision is unchanged; a pass it posts may run before the
  exchange and see the slot pinned, which the next miss, unpin or period covers.
- R3 Every path re-audited for "an access after an event that can let the host destroy the catalog" (a post,
  a kick, a pin or count decrement shutdown waits on, a delivery), on a thread ~catalog does not wait for. Model
  (the lead's, from F1): any thread, host call or not, whose event shutdown's completion waits on must make that
  event its last touch. Each site:
  - unpin (pin::release, deliver's and abandon's gone pins, reopen's plain unpin): the word's exchange is last,
    or `{0, closing}` then post (R1). Fixed.
  - acquire_impl miss or failed-to-opening tail `post(run_open); over_soft();` (review F1): over_soft() now runs
    first, so the post is last (the slot is `opening` and counted until the open runs). Fixed; test
    miss_under_shutdown (the reviewer's probe).
  - acquire_impl's refusal after B1's count, and its two catch paths (alloc_slot, the table insert): they
    decremented `occupied_` then called maybe_complete_shutdown(), whose reads race a completion by another
    thread (owner's join_shutdown or a close chore) that saw the count reach zero. Now uncount_miss() posts
    the decrement and the completion as a chore (inline only if the post throws). Fixed; no test can fail
    without it deterministically: the old code has no hook or post between its decrement and its reads.
  - unlink_waiter (abandon, cancel_wait): `unlinked_` and `cancels_` were bumped after the shard mutex was
    released, after the waiter's pin was uncounted; a transition's end may then close the slot. Both now go in
    under the mutex. Fixed; same caveat on a deterministic test (no hook after the unlock in the old code).
  - acquire_impl's sync wake (post_wake_chore: counter, then post) and its async joins (the pin is counted and
    held): last touch is the post, or the pin holds the slot. Clean.
  - try_fast, try_pin's pin: the caller holds a pin. Clean.
  - drain_unpin: counter before the drain decrement; the last decrement's post is last. Clean.
  - reopen: in-lock writes after the hand-back exchange happen before finish_close, which takes the same
    shard mutex before it frees; the post (holders == 0) is last; the plain branch is unpin. Clean.
  - deliver: after a waiter's `st` exchange it touches only the waiter, except the abandoned-pin unpin (fixed
    above). maybe_complete_shutdown: its deliver is last. Clean.
  - chores (run_open/run_wake/transition ends and their kicks, run_close, finish_close, erase_failed from the
    pass, run_pass, stuck retries): ~catalog waits on group_ before it frees. Clean.
  - join_shutdown's kicks and maybe_complete_shutdown: on the awaiter's thread in await_ready, sequenced before
    it can destroy. Clean.
  - timer callbacks and the budget listener: through life_ (mutex, nulled by ~catalog) and the timers_ count.
    Clean.
- R6 Review N1 (a failed post in unpin's close is swallowed) and N2 (transition ends' acquire loads of
  `shutting_`): pre-existing, left as they were; kick() swallowed the same failure before.
- R5 New hook site `shutdown.mark` in shutdown_slot's pinned branch, before the mark's exchange (F2(b)); the
  schedule sim's site list is explicit, so it does not require injections there.
- R7 The catalog_stress abort is end_at's late unbook (Doing), in catalog.hh, inside this quest's file set; fixed here
  rather than in a quest of its own because A3 needs it. Its test went in catalog_teardown_test (the file set's test),
  though it is a sweep race, not a teardown one.
- R8 test/catalog_test.cc is outside the quest's file set; I changed it at strata:86fe7c47 because the lead asked
  (msg on CI 43260965516). The lead must widen the file set to include it before landing.
- R4 The word layout in docs/mechanisms/catalog.md and docs/design/cold-tier.md: updated by the lead at
  strata:3adfdc27 (file set widened).
