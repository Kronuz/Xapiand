# consensus-sim-loops: journal

State: working (investigating a loop spin at a power cut)
Quest: .campaign/xapiand/quests/consensus-sim-loops.md (the lead's; never edit it)

## Doing

Investigating a loop spin. Repro (consensus `ecb2a37`, Release build in the worktree's
`build/`): `NOCP=1 NORP=1 SIM_DRIVER_SEEDS=1 ./consensus_simulation_test 0 1000 1` (also seed 2)
spins node 2's loop 1 at step 944, in `crash()` (pick 855: `shut(n, true)`), with checkpoints and
repairs off. It appeared once the network delivers one message per group per delivery step (8-10x
the old traffic); the earlier, lighter run passed 10 seeds with 22 crashes. Hypothesis to test
next: after the power cut fails a loop's turn driver, a group still `awaiting_turn()` is
re-watched on the failed driver every step (`Driver::step` -> `watch`), so the loop never idles.
If confirmed it is in `include/consensus/driver.h` (or journal's TurnDriver), outside this file set.

Fixed in the simulation so far: repair replay ordering (the range after a repair may precede the
caller's resume; a cancelled repair still completes), and heal re-proposes when another term's
entry took its placement.

## Next

1. Confirm or refute the re-watch hypothesis at the repro (one diagnostic: count `watch` per
   group during `shut`); if it is the Driver's, report it under Blocked on with the repro.
2. Then seeds with checkpoints and repairs on; strip the DBG/NOCP/NORP/spin_report diagnostics.
3. Measure s/seed, then submit A1 (10,000 seeds x 1,000 steps, split into <=30-minute light jobs),
   A2 (`ack-before-turn-barrier`) and A3 (`repair-below-base`) `--fails-without` jobs (build the
   simulation against `driver_mutants.py NAME build/mutant` as tools/mutants.sh does; run
   `SIM_DRIVER_SEEDS=N consensus_simulation_test 0 1000 1` so only the Driver simulation runs),
   and `campaign verify` for A4.

## Blocked on

Nothing yet (see Doing: the spin may be a Driver bug outside the file set).

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | pending |
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
