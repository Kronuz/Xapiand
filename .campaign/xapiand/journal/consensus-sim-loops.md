# consensus-sim-loops: journal

State: waiting on jobs 261119-171401-ade910 261119-171404-5defa7 261119-171407-41deb0 261119-171410-efdd96 261119-171427-772c66 (A2, A3 mutant jobs done: both killed at seed 1)
Quest: .campaign/xapiand/quests/consensus-sim-loops.md (the lead's; never edit it)

## Doing

Session 2026-11-19 (consensus `d7caaf3`, `c08afe7`):
- Merged main (31 files; only `test/driver_simulation.cc` conflicted). The quest's per-group sim
  keeps its shape; main's trace points went per-group and its regression-seed list joined `run()`.
- Main's Driver no longer re-watches after shutdown (msg-71 fixed by Detent), so the stopping-loop
  budget and busy-round count are gone: a stopping loop that spins now fails.
- The four 11-18 A1 failures were the simulation's, both kinds:
  - Seeds 129, 5050, 8035 "witness acknowledges": heal stopped once the data voters applied, the
    witness still at committed 0. Heal now waits for the witness to commit and self-acknowledge.
  - Seed 2593 Fenced at a clean shutdown: an instrumented scratch worker.h printed
    `storage op failed: task cancelled` during `shut(n, false)`: stop cancelled own-store IO with
    groups open. DRIVER.md's order is close then stop; the sim's clean shutdown now cancels the
    node's live calls, closes every group with IO completing, then stops.
  - With both, those four seeds and seeds 1-100 pass locally.
- A5: each repair records issue-to-completion wait (simulated ms); after heal the seed runs quiet
  rounds until no repair is pending and fails if one is. Seeds 1-100: 1,171 requested, 886
  completed, 209 refused, 76 cancelled, 0 pending; mean 23.2 ms, longest 780 ms.

## Next

1. When the jobs finish: paste `campaign brief`'s Evidence table with the A1 summaries' repair and
   checkpoint counts; set "ready for review". Push is the lead's call (prompt: don't push), so CI
   for A4 waits on that.

## Blocked on

Nothing; msg-71 (the Driver spin) is Detent's and is counted, not hidden.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261119-171401-ade910, 261119-171404-5defa7, 261119-171407-41deb0, 261119-171410-efdd96: seeds 1-10,000 x 1,000 steps (2,500 each, plus regression seed 399 x 3,000) at consensus c08afe7, power cuts on all seeds, every oracle failing the seed |
| A2 | 261119-171422-30e20c (`--fails-without`): `ack-before-turn-barrier`, Driver sim only, 200 seeds x 1,000 steps; control: the A1 jobs |
| A3 | 261119-171425-e6bd03 (`--fails-without`): `repair-below-base`, Driver sim only, 200 seeds x 1,000 steps; control and coverage: the A1 summaries' "below the checkpoint" and "images restored" |
| A5 | the A1 jobs' second summary line: repairs requested, completed, refused, cancelled, pending at a seed's end (a pending repair fails the seed), mean and longest wait |
| A4 | build-test 261119-171427-772c66; CI not run: the branch is not pushed (lead's call) |

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
- A clean shutdown closes its groups before stop (DRIVER.md's shutdown order); a stop with groups
  open fences any whose own-store IO it cancels, which is the documented consequence, not a bug.
- Heal is done only when the witness of a 2 + witness group has committed the healed entry too.
