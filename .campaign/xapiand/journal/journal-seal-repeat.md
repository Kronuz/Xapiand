# journal-seal-repeat: journal

State: waiting on jobs CI 44148860137 (every campaign job at journal:2c0b920 is green)
Quest: .campaign/xapiand/quests/journal-seal-repeat.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

journal:2c0b920 makes repeat_threads deterministic (lead: ASan job 261117-094025-c1a38e failed its
calls-per-turn count, a timing check, not a sanitizer report). The third call of a turn now starts
the unseal and waits until it has dropped the group; the test asserts three records in one turn,
no fourth call, and an unseal that returns only after the calls. Everything else was green at
journal:3c6a468 (CI 44139371364, TSan, six mutants).

## Next

1. Jobs green at journal:2c0b920 (build-test, A1, A2, A3, TSan, ASan all exit 0). When CI run
   44148860137 is green, State: ready for review.

## Blocked on

Nothing.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | 261117-103233-d90dfc (repeat case: three records at staged+1..+3, rooms checked by value, room met and one byte over); mutants 261117-094035-f25876 (one-call), 261117-094044-57a827 (framing), 261117-094052-389c68 (staged), each failing its case at journal:3c6a468 |
| A2 | 261117-103233-7f09bc (repeat_turns: first across groups, two-group alternation 1,2,1,2); mutant 261117-094103-c463ab (rotation) |
| A3 | 261117-103201-cfa134 (repeat_throw, repeat_unseal, repeat_threads); 261117-103206-61c5cc (turn tests under TSan, the gated repeat_threads included); mutants 261117-094114-d5bc25 (unsealed-again), 261117-094124-60920c (repeat-throw) |
| A4 | 261117-103158-aa11a7 (build-test: whole suite, crash fixtures included); 261117-103206-61c5cc (TSan); 261117-103212-a8b3aa (ASan, turn_seal); CI run 44148860137 at journal:2c0b920 |
| A5 | [artifact] `docs/mechanisms/turn-seal.md` at journal:2c0b920: sections Repeat calls, What it gets (room accounting and the first flag), A failing repeat call, lifetime under One per group, Later sealed records, and Fairness (no per-group cap); CHANGELOG.md 2026-11-17 entry |

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
  stops sealing for the turn, writes and acknowledges the earlier records, and the next turn's
  `start()` fails the driver with that error before calling any sealer. `idle()` is unchanged, so
  the failure comes when a next turn is asked for, exactly as a separate turn's call would throw;
  a waiter on the earlier records is acknowledged (shown under the blocking entry). A throw or
  refusal on a group's first call still fails its turn unwritten.
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
