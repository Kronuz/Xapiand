# task-timers-followups: journal

State: working
Quest: .campaign/xapiand/quests/task-timers-followups.md (the lead's; never edit it)

## Doing

Fix at task:2012f28 (quest/xapiand/task-timers-followups). Jobs queued: Debug timers_test
261120-234149-e25be0 (A1, A2), suite 261120-234150-5f88f9 and tsan 261120-234151-7a5529 (A4).
Next: mutant jobs, the x50 mid-fire load job (A3), a TSan x50 of the new concurrency tests.

## Next

1. Submit the mutant jobs (branches mutant/ttf-{no-assert,wait-all,erase-taken,no-ticket} in the task repo, local).

## Blocked on

Docs outside the file set (docs/API.md, docs/mechanisms/lifecycle.md) still state N3 as the
contract ("a later touch may fire a taken key on the loop meanwhile"); they need the lead to
apply or widen the file set. Exact change:

- docs/API.md:116-118, replace "after the fires in
  progress on the loop at the call end; called from a fire of the same debounce, it asserts
  (Debug). Before a stop, a later touch may fire a taken key on the loop meanwhile." with "after
  the fires in progress at the call end; a touch of a taken key meanwhile fires on the loop after
  it, never at once; called from a fire of the same debounce, it asserts (Debug)."
- docs/mechanisms/lifecycle.md:48-49, "waits for the fires in progress on the loop at the call to
  end" becomes "holds them, waits for the fires in progress at the call to end (the loop's and
  earlier calls')".
- lifecycle.md:52, "Each loop fire links a node from its watcher's frame into a list" becomes
  "Each fire in progress, the loop's (a node in its watcher's frame) or a `fire_pending()`
  call's, links a node into a list".
- lifecycle.md:59-64, the "The contract, for a call before the stop: ..." paragraph becomes: "A
  taken entry stays in the map as held: its generation becomes 0, so its watcher ends, and a
  holder count records the calls firing it. A touch meanwhile marks it pending but spawns no
  watcher; when the last holder's fires end, the hand-off that serves a throwing fire gives a
  pending entry a new watcher and erases any other. So no key fires on the loop while a call
  fires it, and two calls taking the same key fire it in ticket order. A fire must not call
  `fire_pending()` of its own debounce, the loop's or a call's: it would wait for its own fire.
  Each fire also pushes its node on a thread-local chain, and `fire_pending()` asserts (Debug)
  that no node on the calling thread's chain is its own; another debounce's fire may call it."

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
| A2 | pending |
| A3 | pending |
| A4 | pending |
| A5 | CI on the pushed branch |

## Result

N1, N2 and N8 were already fixed on task main by task-fire-pending-guard (task:f937bc2: the
Debug assert from a thread-local fire chain, the ticket list, debounce_mid_fire's handshake);
N3 was only documented there. This quest fixes N3 (task:2012f28):

- A key `fire_pending()` takes stays in `keys_` as held (gen 0, no watch, `held` count). A touch
  meanwhile marks it pending without spawning a watch; `hand_off(key, 0)` after the call's fires
  gives it a watch when the last holder ends (the existing throwing-fire hand-off, reused).
- `fire_pending()` files its own ticket in the fires-in-progress list, so a second call taking
  the same key waits for the first; a `fire_pending()` inside its own fire hits the Debug assert
  instead of waiting for itself.
- Tests: debounce-held, debounce-held-twice, debounce-reentry's second case.

## Rulings I made

- N3 fixed, not documented (the quest's A2 asks that no key fire twice at once), with no wait on
  the loop: the held entry has no watch until its holders end, so the loop never blocks.
- The off-loop fires of `fire_pending()` count as fires for the reentry assert: a fire of
  `fire_pending()` calling `fire_pending()` of the same debounce would now wait for its own
  ticket, so it asserts (Debug), tested.
- `fire_pending()` copies the taken keys before changing any entry, so a throwing Key copy leaves
  no entry held forever.
- Off-loop fires still set no throttle (as before: the entry goes unless re-touched).
