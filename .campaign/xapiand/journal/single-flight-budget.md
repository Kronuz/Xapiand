# single-flight-budget: journal

State: waiting on jobs 261121-133028-a36e8e 261121-133137-9c63f2 261121-133202-0764e5
Quest: .campaign/xapiand/quests/single-flight-budget.md (the lead's; never edit it)

## Doing

Jobs queued: the new test at the mutant (expect fail), at the fix (with test_control_plane's
SingleFlight tests), and under TSan (5 runs).

## Next

1. Read the three jobs; on the lead's file-set ruling (Blocked on), wire the two schema call sites
   and update the Limits bullet, then `campaign verify single-flight-budget` and push.

## Blocked on

The lead: the file set misses the two files the outcome needs.

1. `src/database/schemas_lru.cc`: `SingleFlight::run` now takes the joiner's token (4th argument,
   default empty), but its two schema callers (`metadata_loads.run` at line 371, `foreign_loads.run`
   at line 616) must pass it, `budget ? budget->token() : task::cancel_token{}` (`budget` is the
   `task::budget*` already given to the load). Without it, production joiners still wait token-less.
   `index_resolver_lru.cc`'s `settings_loads.run` has no budget in scope; it stays token-less.
2. `docs/mechanisms/cancellation.md`: the Limits bullet lives there; `docs/dev/cancellation.md` (the
   file set's and A3's path) does not exist.
Recommendation: add both paths to the file set; the change is two one-line arguments and one doc
bullet. Alternative considered: a cancellable `task::event` wait in the task library (as
`async_mutex::lock(token)`), which would need a task bump; not needed for one user.

## Evidence

Cause on main (src/single_flight.h at 70ad01dd6): a joiner `co_await flight->done`, an event only
`Lead::end()` sets, and `run()` takes no token, so nothing of the joiner's own can end its wait.

| Criterion | Evidence |
|---|---|
| A1 | fails at mutant 16a783020: 261121-133028-a36e8e (pending); passes at 35d03172c: 261121-133137-9c63f2 (pending); TSan 5 runs: 261121-133202-0764e5 (pending) |
| A2 | not run yet (waits for the call-site wiring) |
| A3 | blocked: path |
| A4 | not pushed: held so CI runs once, after the call-site wiring (Blocked on) |

Cancelled (mistyped target or command, superseded): 261121-133017-722cfb, 261121-133148-972c79.

## Result

src/single_flight.h: each joiner awaits its own wake (`std::shared_ptr<task::event>` in the flight's
list, under the map's lock), set by the flight's end or by a `task::cancel_callback` on its token,
whichever is first. A woken joiner checks `Flight::ended` under the lock: not ended means its token
woke it, so it leaves the list and throws `task::cancelled`. The leader, its value and the one load
per key are unchanged; a joiner whose leader was cancelled loads again only while its token is not
requested. Tests: tests/test_single_flight.cc (3 cases).

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
