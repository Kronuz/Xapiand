# group-heal: journal

State: waiting on jobs 261122-214426-f15731 (probe: where every install flight fails; 35th of 65 queued at 21:27)
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Install stall, round 2 (lead's note 2026-11-22). cd2a4a40's fix (frames in order, snapshot_timeout
3600) did not settle anything: plain 261121-210217-362497 at cd2a4a40 fails install_gap (3, 5),
grow_and_heal and seeds 16, 103, 108, 122, 123, 132, 140 the same way as before (leader names the
peer in installs_needed, the peer stays at its old applied); CI run 44987690334 (asan, tsan,
FreeBSD, macOS) the same. In a settle every message, fetch, IO and chore drains each 5 ms tick,
so simulated time is not the cause: something fails every flight. Probe 261122-214426-f15731
(unversioned, .scratch/group-heal/probe.sh + probe-src, traced; consensus 8780e44 by --ref) shows
where: install_gap(3), grow_and_heal(3) and seed 132, at frame order kept and 3600 ms; probe-src
now also traces a chore item that throws (its type and reason). Installs do work sometimes (13 of
the ungated seeds settle by install, grow_and_heal(5) round 0 installs), so the failure is
state-dependent, not the whole binding. Unconfirmed leads, for the trace to decide:
- seed 132's leader fences on "faultfs write: No space left on device": no install-free path
  fences on ENOSPC (main's seeds pass short of space), and unpin_for's release_pin commits strata
  uncaught on the chore (make_pin's commit is caught), so a release under ENOSPC fences the leader.
- consensus's own driver_install_test checkpoints node 3 before cutting it off in every case;
  install_gap's replica behind never checkpointed (strata's index still deferred).
- the leader's source starts only when its Worker is idle (worker.h source_runnable: no output,
  completion, persist, application or cutover debt); a flight stuck in AwaitSource times out and
  repeats, which matches "names the peer" with no frames. The earlier probe
261121-205709-52ed84 never built (JOB_REF_CONSENSUS unset: submitted without --ref).

Separate cause, fixed at strata:74da850c: grow_and_heal(5) fenced every replica on "a retired
file written again": write_and_compact reused upload ids 104 and 109 across calls, and a leader
change between calls retires the earlier term's open uploads. The world now hands out upload ids
once.

Install task jobs (lead's note): 261121-225628-12c599 ran the retired `test/group/install_build.sh`
(exit 127, no such file at f05b6672): a stale command; its rerun is install_suite.sh's
`^group_install$`. 261121-225628-ca81f0 at f05b6672 fails group_voters: the stall above.

Main merged at strata:44a0d295 (README.md). strata 44a0d295 and 74da850c not pushed: CI would only
repeat the stall; push with the stall fix.

Witness jobs at 7db919a5 ended: plain 214745-036576 fails group_witness (loss: the lost voter
never settles back; join: after the promotion), the install stall again (witness_test runs on the
same harness, moved to test/group/world.hh). Mutants 38a541, 5dffc3 exited 1, which means nothing
while the plain fails. The witness branch needs the stall fix merged in (and 74da850c's
voters_test.cc world edit carried into world.hh at that merge).

## Next

1. Read probe 261122-214426-f15731 (its log, and .scratch/group-heal/{gap3,heal3}-trace.out in
   the strata worktree): which step of the flight fails every time (pin_for no pin, fetch none,
   validation_failed, Begin Busy, SnapshotTransferFailed). Fix it with a test that fails without.
2. At the new head: install_suite.sh (A1, A2), install_test all cases (the install task's rerun),
   install mutants, install-silent; push for CI.
3. witness: merge the quest head into quest/xapiand/group-heal--witness, rerun its three jobs.
4. repair: ruled (lead, 2026-11-23, quest Decisions): option (c), through consensus-request-install; built after that quest lands. sim-rows (order 3) after repair and witness.

## Blocked on

Nothing. Ruled by the lead 2026-11-23: (c). The question as asked, kept for the record: how a damaged replica reinstalls (A3, task `repair`). install's transfer
starts from the source's bundle: `fetch_pin` needs it, `serve` answers only pinned positions,
and pins are made on the source's chore at `SnapshotNeeded`. consensus's `Driver::repair(b)`
rewinds the apply index to any b in [own base, own commit] (Core::handle(Repair)), so a fenced
replica that refetches state must land at or above its own checkpoint. Options:

- (a) In place, a new strata fetch name: the fenced replica reopens, acknowledges its delivered
  ranges unapplied, asks a data voter for `repair/pin` (the source pins its latest bundle on its
  chore and answers bundle and pin), stages and activates as install does (files whose CRC32C
  matches kept, damaged ones fetched whole), then `repair(P_source)`; needs P_source in
  [own base, own commit], else retry. New wire (format-freeze reviews it); Repair is refused
  Busy while a range is delivered, so it races sustained commits.
- (b) Membership, existing contracts only: the host replaces the fenced incarnation by a fresh
  one on the node (learner installs, promoted beside it, the old one removed). A membership
  change and a whole-replica transfer per repair; host orchestration (placement).
- (c) consensus surface: a follower asks its leader for an install (a Driver call that makes the
  leader treat it as below its base). Install's path end to end, the image its new base, no
  new strata wire; Detent's library.

Recommendation: (c), one mechanism for a replica that needs state; (a) if Detent declines.

Finding for Detent (from witness, in its journal): the Driver does not expose
`Core::witness_bound`, so a witness cannot compact its log.

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

<Decisions inside the quest's scope, so the reviewer can check them.>
