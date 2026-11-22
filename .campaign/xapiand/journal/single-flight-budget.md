# single-flight-budget: journal

State: ready to land: every criterion has its job at 19f1e0c, correctness approved; the landing loop merges master and lands on CI.
Quest: .campaign/xapiand/quests/single-flight-budget.md (the lead's; never edit it)

## Doing

All code is in (eea2bee55); jobs queued: the mutant (fails-without), the test at head, TSan at head,
the suite.

## Next

1. Read the four jobs and CI at the pushed head; on all green, set State to ready for review.

## Blocked on

Nothing. msg (lead, file set): answered: both callers in src/database/schemas_lru.cc pass
`budget ? budget->token() : task::cancel_token{}` (eea2bee55); Limits bullet moved into How in
docs/mechanisms/cancellation.md; A1 jobs rerun at the head; suite queued; branch pushed once.
`index_resolver_lru.cc`'s `settings_loads.run` has no budget in scope and stays token-less.

## Evidence

Cause on main (src/single_flight.h at 70ad01dd6): a joiner `co_await flight->done`, an event only
`Lead::end()` sets, and `run()` takes no token, so nothing of the joiner's own can end its wait.

| Criterion | Evidence |
|---|---|
| A1 | fails at mutant 16a783020: 261121-133028-a36e8e (exit 8: test_joiners_that_leave_do_not_disturb_the_one_that_stays, 11 of 27 checks); passes at eea2bee55: 261121-195712-fa1b83 (passed); TSan 5 runs at eea2bee55: 261121-195712-6bdd67 (pending) |
| A2 | suite at eea2bee55: 261121-195713-32dfdb (pending) |
| A3 | docs/mechanisms/cancellation.md: the joiner is out of Limits; How step 1 says a joiner waits under its own token |
| A4 | CI at the pushed head (pending) |

Cancelled (mistyped target or command, superseded): 261121-133017-722cfb, 261121-133148-972c79; superseded by the reruns at eea2bee55: 261121-133137-9c63f2, 261121-133202-0764e5.

## Result

src/single_flight.h: each joiner awaits its own wake (`std::shared_ptr<task::event>` in the flight's
list, under the map's lock), set by the flight's end or by a `task::cancel_callback` on its token,
whichever is first. A woken joiner checks `Flight::ended` under the lock: not ended means its token
woke it, so it leaves the list and throws `task::cancelled`. The leader, its value and the one load
per key are unchanged; a joiner whose leader was cancelled loads again only while its token is not
requested. src/database/schemas_lru.cc passes the budget's token at both callers (metadata and
foreign schema loads). Tests: tests/test_single_flight.cc (3 cases). Doc: docs/mechanisms/cancellation.md.

## Rulings I made

- The `--fails-without` commit is a mutant on this branch, not main: the new test passes a token
  to `run()`, which main's `run()` has no parameter for, so at main it would fail to compile rather
  than on its assertion. The mutant (16a783020) adds the parameter and ignores it, main's behavior.
- The tests go in a new tests/test_single_flight.cc (in the file set); the CMake glob registers it,
  so tests/CMakeLists.txt (which does not exist) is not needed.
- One wake mechanism for every joiner (token or none), replacing the shared `done` event: one
  allocation per joiner, which only concurrent first uses of a key pay.
- A woken joiner whose flight ended returns the value even if its token was requested in between:
  the flight's end decides, under the lock.
