# turn-log-parity--cohort-wait: journal

State: working
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Building the cohort wait (`include/strata/core/turn_seal.hh`: `wake()` counts arrivals, `turn()`
records the released groups and the barrier's length, `serve()` waits up to 1/16 of it) and its
test `test/test_barrier_cohort.cc` (K=4 writers, 64 ms barriers, turns per acknowledgement).

## Next

1. Local build and run; commit; mutant commit (wait dropped) on a side ref.
2. Jobs: the suite (light), the test --for A4, the test at the mutant --for A4 --fails-without.
3. Optional light smoke of probe B (K=4, K=8 against RocksDB).

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| <A1> | <from `campaign brief`> |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

- The cohort counts released *groups*, not writers: `wake()` fires once per opened record, and a
  record is per group, so writers and arrivals are in one unit. A released writer whose group's
  record another writer already opened costs at most the bound (1/16 of the last barrier).
- "Barrier" is the duration of `TurnDriver::commit()` (write and flush) of the last turn that wrote.
- `waiting_` became a list of parked waiters (`waiters_`), so the turn's end can tell which it
  released: `await()` by its sequence, `await_sealed()` by its predicate (counted once its record
  sealed, as D5 says).
- Registered the new test in `CMakeLists.txt` (three lines beside `standalone_turn_log`): outside
  the task's globs, but a test file the globs name cannot run without it.
- Added `turn_sealer::turns_written()` (tests), beside `waiting()`: the test counts turns per
  acknowledgement without racing the driver's stats.
