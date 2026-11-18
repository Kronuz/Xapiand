# consensus-sim-loops: journal

State: working (attributing a heal stall that needs checkpoints plus repairs)
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

Open: with that, 10 of seeds 1-24 fail heal ("did not apply a new entry ... after healing"; the
leader's heal proposal never completes). Seeds 1 3 7 8 13 18 19 20 21 23. All 10 pass with
checkpoints off (checkpoint() draws no randomness, so the schedule is otherwise the same) and
all pass with repairs off: the stall needs both. Suspect a checkpoint racing a repair (the epoch
moves under an in-flight capture) leaves the Worker's checkpoint phase blocking proposals; not yet
attributed to sim or Worker.

## Next

1. Attribute the heal stall at seed 3: is a checkpoint call live on the stalled leader, and was a
   repair accepted while it was in flight? If the sim issued a repair it should not have, gate it;
   if the Worker stalls on a refused capture, report it like msg-71.
2. Measure s/seed; submit A1 (10,000 seeds x 1,000 steps, split into <=30-minute light jobs),
   A2 (`ack-before-turn-barrier`) and A3 (`repair-below-base`) `--fails-without` jobs (build the
   simulation against `driver_mutants.py NAME build/mutant` as tools/mutants.sh does; run with
   `SIM_DRIVER_SEEDS=N ... 0 1000 1`), and `campaign verify` for A4.

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
