# unpin-after-kick: journal

State: waiting on jobs 261112-055252-e509d2 261112-055255-91db1f 261112-055258-a6bd1d 261112-055301-475b83 261112-055304-392b68 261112-055306-ba7c31 261112-051905-9ea016
Quest: .campaign/xapiand/quests/unpin-after-kick.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

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

1. Read the jobs; record the pin_smoke ns/pin+unpin (base vs fix) under Result (N3).
2. Paste `campaign brief`'s table; CI green on every platform (A3); answer the review by finding id.
3. State "ready for review" (re-review of F1, F2).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <no evidence yet> |
| A2 | <no evidence yet> |
| A3 | <no evidence yet> |

## Result

<pending>

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
- R4 The word layout in docs/mechanisms/catalog.md and docs/design/cold-tier.md: updated by the lead at
  strata:3adfdc27 (file set widened).
