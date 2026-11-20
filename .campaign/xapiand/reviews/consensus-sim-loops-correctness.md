# consensus-sim-loops review: correctness

Commit: consensus:61e2f3adcea830cdb9c0f7943f143f0ad4156b66 (code at consensus:98298fb; the commits since are merges of main)
Verdict: APPROVE
Date: 2026-11-20

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

## Evidence I relied on

- A1/A5: 261119-195629-82eb70, -195631-e40a04, -195633-cd71f4, -195636-1b8937 (seeds 1-10,000 x 1,000
  steps plus regression seed 399, all oracles, passed at 98298fb). My probe reran the same four
  ranges and printed identical totals (hosts booted 4160/3968/4104 times with 2/3/4 loops in the first
  range, same elections and proposals), so the runs are deterministic and the A1 jobs are the runs I
  analyzed below. The repair counts add up: 27,177 requested = 20,929 completed + 4,812 refused +
  1,436 cancelled, 0 pending.
- A2/A3 mutants: 261119-195638-0e5689 and -195639-9537ee fail with the mutants built in (the
  compile is a separate step that exits 126, so exit 1 is the simulation's). Read their logs: A2 first
  fails with "node 2 fenced group: conflict with committed log prefix", A3 with "node 3 fenced group".
- Build-test 261119-195641-9da6b4 (69 of 69 passed, `consensus_simulation` 3.16 s).

## What I checked myself

Probes under `.scratch/xapiand/review-consensus-sim-loops-correctness/` (`.purpose` there; `patch.py`
patches a scratch copy of `test/driver_simulation.cc`, never the quest's file):

1. Q1, heal bound. Read `heal()` (`driver_simulation.cc:262-320`): it runs `4000 + 2 * snapshot_timeout / 4`
   quiet rounds (34,000; a round is 4 ms on the slowest clock, so 120 s simulated, twice the 60 s
   timeout), breaks early once every group is healed, and then fails every group that is not healed
   (`fail(...did not commit a new entry...)`). The bound is finite and a stuck group fails the seed.
   Probe 261120-000443-3f163d (`rounds.sh`) records, per group, the round it healed at, across seeds
   1-10,000: mean 28.6 rounds per seed; 3 of 10,004 runs needed more than 1,000 rounds, and all three
   needed more than 4,000: seed 4270 at round 14,881, seed 6197 at 13,154, seed 8247 at 10,498, each
   with exactly 1 SnapshotNeeded for the late group. No group healed late with 0 SnapshotNeeded. So
   the longer window only absorbs the documented case (the leader checkpointed past its record of a
   peer's match, released after `snapshot_timeout`). A group that never converges still fails at the
   end of the 34,000 rounds.
2. Q2, property checked directly or by proxy. Probes 261120-000445-40439b and 261120-000446-efd05c
   (`nofence.sh`) rebuild each mutant with the new "Fenced outside a power cut" oracle off.
   A2 (`ack-before-turn-barrier`): still killed at seed 1, first by "state machine safety: group's
   index 1 differs between nodes" (an acknowledged record lost to the cut, another entry committed
   at its index): the direct property, not only the Fenced proxy. A3 (`repair-below-base`): with
   Fenced off it is killed only by the heal liveness failure (see F1).
3. Q3, loops. `journal/host.h` maps a slot to a loop as `slot * loops >> 12` over 4,096 slots, and the
   simulation places one group in each 512-slot eighth, so every loop of a 2-, 3- or 4-loop host gets
   at least one group (4 loops: two eighths each; 3 loops: loop 1 holds eighths 3-4 and loop 2 holds 6-7
   whole) and at least one loop holds several. `check_placement` runs at every boot and fails a seed
   that has no sharing or only one loop. Failure dumps in the mutant logs show groups on loops 0, 1 and
   3. IO is completed per loop (`complete_io` picks a random loop), each loop has its own scheduler,
   Driver and turn driver, and the A1 line shows 4160/3968/4104 boots at 2/3/4 loops with 767
   re-partitions per 2,500 seeds.
4. Q4, runtime. `consensus_simulation` takes 3.16 s in build-test (timeout 300 s); an A1 range of 2,500
   seeds takes about 75-170 s on a loaded host and is not in CI. `tools/mutants.sh` runs the Driver
   simulation (40 seeds x 3,000 steps) only for a mutant the core simulation does not kill, and a
   failing seed ends the run, so heal's 34,000-round ceiling is paid only on a failure.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | gap | A3's mutant is killed through the Driver, but by the Fenced oracle (the mutated Core accepts the repair and later fences, "log index outside retained suffix" in `tools/mutants.sh`'s run of the same fault) and, with Fenced off, only by the heal liveness failure. The direct oracle (`on_repaired` below `r.base`, `driver_simulation.cc:372`) never fired in the mutant runs, so its liveness is unproven. The criterion ("killed through the Driver") is met. | 261119-195639-9537ee log; probe 261120-000446-efd05c | Optional: a mutant that accepts a repair below base without a later fence, or a one-line unit check that `on_repaired` fails below base. |
| F2 | no | gap | The A1 summary does not report the heal length, so a regression that makes more groups wait out `snapshot_timeout` (a 60 s stall in production) passes silently inside the 34,000-round window. Today 3 of 10,004 seed runs do, each with a SnapshotNeeded. | probe 261120-000443-3f163d | Add to the summary the count of groups healed after 4,000 rounds and the longest heal, and fail a group that heals after 4,000 rounds with 0 SnapshotNeeded. The leader-side stall itself is Detent's (the journal's observation for Detent: a peer holding the prefix waits a full `snapshot_timeout` when the leader's match for it is stale). |
| F3 | no | gap | `consensus_mutants` took 1,477.6 s of its 1,800 s timeout in build-test (82%), and the CMake comment beside it still says "14 faults ... about 800 s" (there are 15). I could not attribute the time to this quest: the idle-host timing jobs 261119-195703-b690ba (head) and -195708-8e4000 (main) were still waiting on the exclusive hold when I finished. This quest touches the cost only by recompiling the larger `driver_simulation.cc` 15 times and the Driver run of surviving mutants. | build-test log; `campaign job list` | The lead reads the two timing jobs and raises the timeout or fixes the comment in a file outside this quest's set if head is slower than main. |
| F4 | no | nit | `check_placement` requires at least two loops and one shared loop, not that every loop holds a group. The eighth placement makes the latter structurally true (see item 3), so the check is weaker than the claim but not wrong. | `driver_simulation.cc:393-399` | None needed. |

Blocking: yes or no. Kind: defect (wrong behavior), gap (unproven claim or missing test), nit. A
finding that would remove or narrow user-visible behavior offers options, including keeping it,
and is never blocking.

## Not run

- CI at the merge commit 61e2f3ad (running when I started); I relied on green CI at 98298fb as given.
- The idle-host `consensus_mutants` timings (F3).
- Sanitizer builds, other platforms, and seeds beyond 1-10,000 or steps beyond 1,000 (3,000 only for the
  regression seed).
- A line-by-line read of every oracle in `driver_simulation.cc`; I read heal, the Fenced, repair and
  placement oracles, and the checkpoint and recovery paths they depend on.
