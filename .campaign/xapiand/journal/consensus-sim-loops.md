# consensus-sim-loops: journal

State: working (heal stall fixed; A1-A4 jobs next)
Quest: .campaign/xapiand/quests/consensus-sim-loops.md (the lead's; never edit it)

## Doing

Spin confirmed and reported (Detent msg-71). Repro at consensus `ecb2a37`:
`SIM_DRIVER_SEEDS=1 build/consensus_simulation_test 0 1000 2` -> "node 3 loop 0 spins".
Cause: `include/consensus/driver.h:647` (`step` re-watches while `awaiting_turn()`) with `:928`
(`watch` waits on `turns_.durable(..., shutdown_.token())`): after `shutdown()` (`:942`) every
watch throws `task::cancelled` at once and its callback re-queues the group, so the loop never
idles until the barrier lands. Shown with an instrumented scratch driver.h: groups at IOPending,
awaiting_turn 1, turn driver not failed, watch woken 600,000 times with task cancelled. It hit
16 of seeds 1-24, so a seed list would exclude most seeds. Ruling instead (consensus `a8304d4`):
a stopping loop gets a 10,000-step budget and its busy rounds are counted in the report
("rounds a stopping loop stayed busy (Detent msg-71)"), so power cuts stay on every seed.

Heal stall understood; both causes were the simulation's (consensus `e0715bb`):
- Seed 3: no checkpoint was ever reserved. `Worker::reserve_checkpoint` -> `reserve_replacement`
  returned nullopt every time (scratch-instrumented worker.h): the sim's replacement pool
  (256 KiB) is below a checkpoint's peak (bundle bound 1,141,236 B from log_bytes, plus the 1 MiB
  application cap). `Driver::admit` stops at a checkpoint it cannot reserve, so the leader's later
  proposals were never admitted. Fixed by a 4 MiB pool and a 64 KiB application cap. Side
  observation for the lead: a checkpoint that can never be reserved waits forever and holds every
  later call of its group (reserve's nullopt means both "not now" and "never").
- Seed 13: heal proposed on a deposed term-6 leader; the placed entry was dropped without another
  term committing at its index, so heal waited forever. It now re-proposes once a later term leads.
With both fixed, seeds 1-60 pass (1,000 steps). Mutants at 60 seeds: `repair-below-base` killed at
seed 1 (a fence, through Driver::repair; the only change is the Core's check), and
`ack-before-turn-barrier` killed at seed 1 ("conflict with committed log prefix", then heal).

## Next

1. Measure s/seed; submit A1 (10,000 seeds x 1,000 steps, split into <=30-minute light jobs),
   A2 and A3 `--fails-without` jobs (`driver_mutants.py NAME build/mutant`, compile as
   tools/mutants.sh does, run `SIM_DRIVER_SEEDS=N ... 0 1000 1`), and `campaign verify` for A4.

## Blocked on

Nothing; msg-71 (the Driver spin) is Detent's and is counted, not hidden.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending; power cuts on all seeds, busy stopping rounds counted (Detent msg-71) |
| A2 | pending |
| A3 | pending |
| A4 | pending |

## Result

`consensus` `test/driver_simulation.cc`: three hosts of 2-4 loops (per node per seed; a reboot
re-asks a quarter of the time, so hosts re-partition), 8-10 three-member groups placed one per
eighth of the slot space (every host has a loop shared by several groups and groups on other
loops; checked at every boot), IO held and completed per loop. Adds checkpoints (application
image, restored on recovery) and repairs at and below the commit cursor and below the latest
checkpoint, through `Driver::repair`. `test/driver_mutants.py` gains
`ack-before-turn-barrier` (worker.h) and `repair-below-base` (core.h, tools/mutants.sh's).

## Rulings I made

- A2's mutant lives in `worker.h`: a shared append steps `Persisted` into the Core once staged,
  before its loop's turn barrier. Consensus cannot ack earlier anywhere else; the barrier itself
  is journal's `TurnDriver`, outside this file set.
- Checkpoints only through an index every data voter applied and the witness committed: the
  simulation sends no snapshot installs.
- A Fenced notice outside a power cut is a failure (new oracle).
