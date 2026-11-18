# consensus-sim-loops: journal

State: waiting on jobs 261118-043759-fb1514 261118-043801-c7b62e 261118-043804-897f01 261118-043806-d63cec 261118-043818-1a72cb 261118-043821-13551b 261118-043823-ba706f
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

1. When the jobs finish: paste `campaign brief`'s Evidence table; from the four A1 logs, sum
   "images restored" and "below the checkpoint" (both must be > 0; at 60 seeds locally they were
   10 and 26, at seeds 1001-1050 8 and 14) and the busy-at-stop rounds (msg-71).
2. Landing waits on detent:consensus-close-fenced (the spin fix); then rerun the full seed set,
   expecting 0 busy-at-stop rounds.

## Blocked on

Nothing; msg-71 (the Driver spin) is Detent's and is counted, not hidden.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261118-043759-fb1514, 261118-043801-c7b62e, 261118-043804-897f01, 261118-043806-d63cec: seeds 1-10,000 x 1,000 steps (2,500 each) at consensus e0715bb; power cuts on all seeds; stopping-loop busy rounds counted, not failed (Detent msg-71) |
| A2 | job 261118-043818-1a72cb (`--fails-without`): `ack-before-turn-barrier`, Driver sim only, 200 seeds x 1,000 steps |
| A3 | job 261118-043821-13551b (`--fails-without`): `repair-below-base`, Driver sim only, 200 seeds x 1,000 steps; coverage: A1 summaries' "images restored" and "below the checkpoint" |
| A4 | build-test 261118-043823-ba706f; CI on the quest branch |

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
