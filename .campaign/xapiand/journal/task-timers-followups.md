# task-timers-followups: journal

State: waiting on jobs 261121-004731-d3f93a 261121-004734-c4ee7e 261121-004734-b8ec84 261121-004736-a76f3d 261121-004739-df20cc 261121-004741-f7a445 261121-004744-3c233d 261121-004747-973743 261121-004749-c4fc38 261121-004752-269b3f 261121-004754-b167fb 261121-004756-0173e0 261121-004757-b2ddf1
Quest: .campaign/xapiand/quests/task-timers-followups.md (the lead's; never edit it)

## Doing

Fix round 1 (review F1, CHANGES) at task:dc2be96, pushed. Lead ruling: narrow the call-to-call
wait to earlier calls holding a key this call took. Each entry counts turns issued/served (a
ticket lock per key); calls left the loop's ticket list but keep a node on the thread's fire
chain (the assert). Jobs at dc2be96:

- Debug timers_test 261121-004731-d3f93a (A1, A2); suite 261121-004734-c4ee7e, tsan
  261121-004734-b8ec84 (A4)
- mutants, `--fails-without`: no-assert 261121-004736-a76f3d (reentry), wait-all
  261121-004739-df20cc (overlap), erase-taken 261121-004741-f7a445 (held), no-turn
  261121-004744-3c233d (held-twice)
- F1 tests at 340746a's header with the new tests (mutant/ttf1-wait-calls): debounce-cross
  261121-004747-973743, debounce-lock 261121-004749-c4fc38 (each must DEADLOCK-fail at 10 s)
- A3 mid-fire x50 at 8 on 4 cores 261121-004752-269b3f; TSan x50 of seven concurrent subtests
  261121-004754-b167fb
- reviewer stress (probe.cc vs model): Debug x4 261121-004756-0173e0, TSan x3 261121-004757-b2ddf1

Round-0 jobs at 2012f28 (all passed per the review) are superseded for the code.

## Next

1. When the jobs end: each mutant log fails on its CHECK (DEADLOCK for the two F1 tests), not a
   build error; fill Evidence; answer F1 in the review reply; State "ready for review". Mutant
   branches mutant/ttf1-* are local in the task repo.

## Blocked on

Nothing. msg (lead, docs unblocked): applied at task:340746a; round 1 (task:dc2be96) replaces its
"never on the loop thread while another call's fire waits" rule, which F1 showed too narrow, with
the narrowed wait the lead ruled.

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
- Round 1 (task:dc2be96): per-key turns (issued/served) order calls taking the same key; a call
  waits for no call holding none of its keys (debounce-cross, debounce-lock). A `fire_pending()`
  inside its own call's fire hits the Debug assert.
- Tests: debounce-held, debounce-held-twice, debounce-reentry's second case.
- Docs (task:340746a): docs/API.md, docs/mechanisms/lifecycle.md "periodic and debounce", with
  the rule: never call `fire_pending()` on the loop thread while another call's fire waits for the loop.

## Rulings I made

- N3 fixed, not documented (the quest's A2 asks that no key fire twice at once), with no wait on
  the loop: the held entry has no watch until its holders end, so the loop never blocks.
- The off-loop fires of `fire_pending()` count as fires for the reentry assert: a fire of
  `fire_pending()` calling `fire_pending()` of the same debounce would now wait for its own
  ticket, so it asserts (Debug), tested.
- `fire_pending()` copies the taken keys before changing any entry, so a throwing Key copy leaves
  no entry held forever.
- (Round 1, F1, lead's ruling) A call waits only for earlier calls holding a key it took, per key
  in take order, not for every earlier call; "every touch before it has fired" becomes "has fired,
  or is firing in an earlier call". Waits point only to earlier takes, so they form no cycle; a
  fire that blocks on what a later call holding its key holds can still deadlock, inherent to
  preventing the double fire.
- F3 (spawn throwing in the hand-off loop) and F2 (adopt the stress) not taken: nits; F4, F5
  folded into docs/API.md.
- Off-loop fires still set no throttle (as before: the entry goes unless re-touched).
