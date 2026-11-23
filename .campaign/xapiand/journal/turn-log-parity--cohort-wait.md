# turn-log-parity--cohort-wait: journal

State: waiting on jobs 261120-082443-cbb227 261120-082445-11a61b 261120-082448-f562c7
Quest: .campaign/xapiand/quests/turn-log-parity.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Committed strata:2f5f1033 (mechanism, test, CMake registration). Mutant strata:0ceb74fb (the
`wait_for` in `serve()` dropped), kept at `refs/mutants/turn-log-parity--cohort-wait`, not pushed.

## Next

1. Read the jobs: suite 261120-082443-cbb227, test --for A4 261120-082445-11a61b, mutant --for A4
   --fails-without 261120-082448-f562c7.

## Blocked on

Nothing.

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A4 (task 1 part) | test 261120-082445-11a61b; mutant 261120-082448-f562c7 (`--fails-without`); suite 261120-082443-cbb227 |

## Result

`turn_sealer::serve()` holds each turn until the groups whose writers the last turn released have
opened as many records, or 1/16 of the last barrier passes (D5). `test/test_barrier_cohort.cc`:
K writers over 64 ms barriers; locally every commit after 3 settling rounds rode exactly the next
barrier (K=4: 84 of 84; K=8: 168 of 168; 25 turns for 24 rounds).

Smoke (not a gate; light, one 4 s trial each, macOS host, fillrandom sync/1 100 B, one log):
K=4 strata 181 ops/s p50 21 ms against RocksDB 131 ops/s p50 30 ms; K=8 324 p50 22 ms against
194 p50 42 ms. Same direction as the note's cohort prototype (180 and 327).

Mutant, run locally (job queued): K=4, 19 of 84 commits waited a second barrier, 30 turns for 24
rounds; the test fails (exit 1).

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
