# task-timers-followups: journal

State: waiting on jobs 261120-234149-e25be0 261120-234150-5f88f9 261120-234151-7a5529 261120-234620-0cc4fe 261120-234623-f45356 261120-234625-726b55 261120-234627-d28a8b 261120-234629-234a67 261120-234631-cf37c8
Quest: .campaign/xapiand/quests/task-timers-followups.md (the lead's; never edit it)

## Doing

Fix at task:2012f28, pushed (CI runs on quest/xapiand/task-timers-followups). Jobs queued:

- Debug timers_test 261120-234149-e25be0 (A1, A2: asserts on, every subtest)
- suite 261120-234150-5f88f9, tsan 261120-234151-7a5529 (A4)
- mutants, `--fails-without`, each the named subtest: no-assert 261120-234620-0cc4fe
  (debounce-reentry, A1), wait-all 261120-234623-f45356 (debounce-overlap, A2 starvation),
  erase-taken 261120-234625-726b55 (debounce-held, A2 double fire), no-ticket
  261120-234627-d28a8b (debounce-held-twice, A2 two calls)
- A3: debounce-mid-fire x50, 8 at once on 4 cores, Debug 261120-234629-234a67
- TSan x50 of mid-fire, overlap, held, held-twice, after-stop 261120-234631-cf37c8

## Next

1. When the jobs end: check each mutant log fails on its CHECK (not a build error), fill Evidence,
   State "ready for review". Mutant branches mutant/ttf-{no-assert,wait-all,erase-taken,no-ticket}
   are local in the task repo (sources in the worktree's .scratch/ttf-mutants).

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
- A `fire_pending()` now also waits for the fires of earlier `fire_pending()` calls, not only the
  loop's: a key an earlier call is still firing was touched before this call, so "every touch
  before it has fired when it returns" needs it. Risk for the reviewer: a call on the loop thread
  then blocks on another thread's fires; a fire that waits for the loop while another call runs
  on the loop would deadlock. No planned caller does both (the lifecycle flushes once).
- Off-loop fires still set no throttle (as before: the entry goes unless re-touched).
