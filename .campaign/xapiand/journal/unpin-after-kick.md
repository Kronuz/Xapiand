# unpin-after-kick: journal

State: waiting on jobs 261112-051834-5fd5e6 261112-051837-327568 261112-051840-b0a9ca 261112-051843-666637 261112-051846-b357a7 261112-051905-9ea016
Quest: .campaign/xapiand/quests/unpin-after-kick.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Fix and test committed at strata:da983792 and pushed (CI on quest/xapiand/unpin-after-kick). Jobs:
- 261112-051834-5fd5e6 A1 TSan: catalog_teardown_test + catalog_sweep_test
- 261112-051837-327568 A1 ASan+UBSan: same
- 261112-051840-b0a9ca A1 --fails-without: catalog.hh from ab0e26a6, ASan, `catalog_teardown_test unpin`
  must report heap-use-after-free (locally: the fix passes it under TSan)
- 261112-051843-666637 A2 --fails-without: unpin without over_soft(), soft_caps_post's hot check fails
- 261112-051846-b357a7 build-test (A2, A3)
- 261112-051903-f5de71 / 261112-051905-9ea016 pin_smoke A/B build and strict run (hot-path cost)

## Next

1. Read the jobs; paste `campaign brief`'s table; CI green on every platform for A3.
2. Result with the pin_smoke numbers; State "ready for review".

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
- R3 Other paths checked (access after an event that may let the host destroy the catalog, on a thread
  ~catalog does not wait for):
  - chores (run_close, finish_close, erase_failed from the pass, run_open/run_wake ends with kick):
    ~catalog waits on group_ first, so their tails are covered.
  - join_shutdown and its kicks run on the owner's thread, which is the destroyer: sequenced.
  - drain_unpin: the counter is bumped before the drain decrement; the last decrement's post is last.
  - reopen(): its in-lock writes after the hand-back exchange happen before finish_close, which takes
    the same shard mutex before it frees the slot; the post (holders == 0) is last.
  - acquire_impl's miss path (maybe_complete_shutdown, then throw/return), abandon(), cancel_wait()
    (counters after unlink_waiter's uncount): calls and frames the host awaits; the catalog's contract
    is that it outlives every frame that awaits it, so not this race. Left unchanged.
- R4 The word layout is also described in docs/mechanisms/catalog.md:14 and docs/design/cold-tier.md:220,
  outside this quest's file set: they still say `state:4`. Named for the lead.
