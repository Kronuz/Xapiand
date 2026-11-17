# journal-seal-repeat: journal

State: waiting on jobs 261117-110859-91c046 261117-110901-9d1761 261117-110904-7f8b43 261117-110908-fa600f 261117-110912-4385ec 261117-110915-52232d 261117-110919-da8738 261117-110923-a1e7a9 261117-110926-0a82cc 261117-110930-622d76 261117-110934-39fbbf 261117-110937-84fdcd 261117-110941-e5fab6 261117-110945-9afd4e
Quest: .campaign/xapiand/quests/journal-seal-repeat.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Correctness review (Xapiand:5a9f323) CHANGES; fixed at journal:12c5425 and journal:13592a1:
- F1 (defect): completed() now acknowledges the turn's records, then fails the driver with the
  parked repeat-call error and throws it (commit() and run() throw it too); idle() is false while
  it is parked; start()'s next-turn check is gone. New case repeat_latent (the review's latent:
  commit(), begin()/completed(), run() with stop requested all throw and failed() is true) and
  mutant deferred-ignored. The review's probe reruns at the fix (261117-110919-da8738).
- F2 (nit): docs Fairness says one turn per group ahead in the rotation, while staged records
  leave the room.
- F3 (nit): CHANGELOG line that unseal() waits for all of a group's calls in the turn.

## Next

1. All jobs green at journal:13592a1 and CI green there, then State: ready for review
   (re-review of F1).

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261117-110901-9d1761 (repeat case: three records at staged+1..+3, rooms checked by value, room met and one byte over); mutants 261117-110923-a1e7a9 (one-call), 261117-110926-0a82cc (framing), 261117-110930-622d76 (staged) |
| A2 | 261117-110904-7f8b43 (repeat_turns: first across groups, two-group alternation 1,2,1,2); mutant 261117-110934-39fbbf (rotation) |
| A3 | 261117-110908-fa600f (repeat_throw, repeat_latent, repeat_unseal, repeat_threads); 261117-110912-4385ec (turn tests under TSan, the gated repeat_threads included); review probe latent 261117-110919-da8738; mutants 261117-110937-84fdcd (unsealed-again), 261117-110941-e5fab6 (repeat-throw), 261117-110945-9afd4e (deferred-ignored) |
| A4 | 261117-110859-91c046 (build-test: whole suite, crash fixtures included); 261117-110912-4385ec (TSan); 261117-110915-52232d (ASan, turn_seal); CI on quest/xapiand/journal-seal-repeat at journal:13592a1 |
| A5 | [artifact] `docs/mechanisms/turn-seal.md` at journal:13592a1: sections Repeat calls, What it gets (room accounting and the first flag), A failing repeat call, lifetime under One per group, Later sealed records, and Fairness (no per-group cap); CHANGELOG.md 2026-11-17 entries |

## Result

journal:6187a32, journal:57849ad and journal:3c6a468: `TurnDriver::seal_turn`
(include/journal/turn_driver.h) calls a sealer again with the room left and `first = false` while
it returns records, charging each `record_cost` before the next call; `running_` spans a group's
calls and the table is re-checked before each repeat call. Contract: journal
`docs/mechanisms/turn-seal.md` and CHANGELOG.md. Tests: `test/turn_seal_test.cc` cases `repeat*`;
mutants in `test/turn_seal_mutants.sh`. Existing call-count checks in `budget`, `trigger` and
`forget_in_flight` now count the extra repeat call that answers nothing. Leaves open: telling
Detent's lead to repin on landing.

## Rulings I made

- A repeat call that throws or returns a refused record (wrong group, out of sequence, past room)
  stops sealing for the turn; the turn is written; completed() acknowledges its records, then
  fails the driver with the parked error and throws it (review F1; was: the next turn failed). A
  throw or refusal on a group's first call still fails its turn unwritten.
- For the reviewer: to keep "acknowledges them" true, fail() now resolves waiters with a new
  `failed` state, and Wait::await_resume throws the driver's failure only for a waiter not
  resolved `completed`. So a durable() waiter that completed() answered (its record is durable) is
  acknowledged even when the driver fails right after, here or in any later turn's failure that
  wins the race to the waiter's resumption; before, it saw the failure. Waiters fail() resolves,
  and a reclamation failure inside completed() (which fails before resuming anyone), behave as
  before. CHANGELOG states it.
- Fairness: a group that sealed in the turn claims no `next_seal_` when its repeat call returns
  nothing; the first group not called, or declining its first call under a share, starts the next
  turn. No per-group cap (the room is the cap); a record needing the whole room waits at most one
  extra turn behind a group that sealed first. Stated in the docs.
- For the reviewer: `forget` between repeat calls is refused, not honoured. Reason: `forget`
  already refuses a group with a record staged or in flight (turn_driver.h `forget`, "the group has
  records staged or in flight"), and after a group's first record of the turn seals, that record
  is in `pending_`. Honouring the forget would mean either writing a record for a group the log has
  forgotten (`log_.forget` drops the group, so the turn's record would reopen it or be refused at
  encode) or dropping a record the sealer already handed over as sealed, which loses it without
  failing anyone. Refusing keeps the one rule forget has everywhere: close a group only once
  nothing of it is pending. A consumer closes after the turn's barrier, as `Journal::close` does.
  The quest's "forgotten between repeat calls is not called again" then holds trivially; the
  re-check before each repeat call still covers unseal and re-seal, and a forget during a group's
  first call (nothing of it pending) unregisters it and stops its calls, as before.
- CHANGELOG.md entry added once the lead put it in the file set (journal:3c6a468).
