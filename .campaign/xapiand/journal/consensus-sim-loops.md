# consensus-sim-loops: journal

State: waiting on jobs 261119-195629-82eb70 261119-195631-e40a04 261119-195633-cd71f4 261119-195636-1b8937 261119-195638-0e5689 261119-195639-9537ee 261119-195641-9da6b4 261119-195703-b690ba 261119-195708-8e4000
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

Round 2 (lead, after c08afe7: A1 failed seeds 4270, 6197, 8247; build-test's mutants timed out):
- The three seeds were the simulation's again (consensus `98298fb`): each stuck group's leader
  had checkpointed past its record of a peer's match and emitted one SnapshotNeeded, which the
  sim never serves. The Core releases it after `snapshot_timeout` (60 s) and probes at its base
  (`core.h` tick, `release_snapshot(..., TimedOut, probe)`); the heal ended at 4,000 rounds
  (16-20 s). With heal bounded at twice the timeout on the slowest clock, all three pass at the
  production 60 s timeout; at seed 4270 the peer commits at heal round 14,880. Not a Driver bug.
  Observation for Detent: a peer that holds the prefix waits a full snapshot_timeout when the
  leader's match for it is stale and no install source answers.
- GitHub CI at c08afe7: only macos-x86_64 failed, `consensus_transport_groups_upload` ("a follower
  would start an election"). Main's run 44613773204 fails the same test on the same runner:
  pre-existing, the lead's (Detent's groups-upload quests).
- consensus_mutants: 1800.33 s timeout at load ~50; 1250 s at e0715bb (14 mutants, now 15).
  Idle-host timing jobs at the head (b690ba) and main (8e4000), exclusive, before any change.

## Next

1. When the jobs finish: paste `campaign brief`'s Evidence table with the A1 summaries' repair and
   checkpoint counts; set "ready for review". Push is the lead's call (prompt: don't push), so CI
   for A4 waits on that.

## Blocked on

Nothing; msg-71 (the Driver spin) is Detent's and is counted, not hidden.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | jobs 261119-195629-82eb70, 261119-195631-e40a04, 261119-195633-cd71f4, 261119-195636-1b8937: seeds 1-10,000 x 1,000 steps (2,500 each, plus regression seed 399 x 3,000) at consensus 98298fb, power cuts on all seeds, every oracle failing the seed |
| A2 | 261119-195638-0e5689 (`--fails-without`): `ack-before-turn-barrier`, Driver sim only, 200 seeds x 1,000 steps; control: the A1 jobs |
| A3 | 261119-195639-9537ee (`--fails-without`): `repair-below-base`, Driver sim only, 200 seeds x 1,000 steps; control and coverage: the A1 summaries' "below the checkpoint" and "images restored" |
| A5 | the A1 jobs' second summary line: repairs requested, completed, refused, cancelled, pending at a seed's end (a pending repair fails the seed), mean and longest wait |
| A4 | build-test 261119-195641-9da6b4; mutants idle timing 261119-195703-b690ba (head), 261119-195708-8e4000 (main); CI at c08afe7 red only on the pre-existing macos-x86_64 groups_upload failure (main run 44613773204) |

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
- Heal runs up to twice `snapshot_timeout` (slowest clock), since the sim serves no installs.
- Heal is done only when the witness of a 2 + witness group has committed the healed entry too.
