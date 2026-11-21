# single-flight-budget: journal

State: working
Quest: .campaign/xapiand/quests/single-flight-budget.md (the lead's; never edit it)

## Doing

Confirm on main: tests/test_single_flight.cc at the mutant commit (run() takes a token and ignores it,
which is main's wait exactly: a joiner awaits the flight's one `done` event, which nothing but the
leader sets).

## Next

1. Confirm job at the mutant (expect FAIL: B keeps waiting), then the fix and its jobs.

## Blocked on

Nothing yet.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | <pending> |

## Result

<pending>

## Rulings I made

- The `--fails-without` commit is a mutant on this branch, not main: the new test passes a token
  to `run()`, which main's `run()` has no parameter for, so at main it would fail to compile rather
  than on its assertion. The mutant adds the parameter and ignores it, the behavior of main.
- The tests go in a new tests/test_single_flight.cc (in the file set); the CMake glob registers it,
  so tests/CMakeLists.txt (which does not exist) is not needed.
