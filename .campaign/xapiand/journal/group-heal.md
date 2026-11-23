# group-heal: journal

State: waiting on jobs 261123-005919-bdfe69 261123-005922-93c9ac 261123-005926-e5b297 261123-005929-f4d961 261123-005932-93ae28 261123-005935-e11cfa 261123-005938-a51a18 261123-005942-72e95b 261123-005945-6cfb33 261123-005949-d1a515 261123-005951-972f5c 261123-005954-4adb58 261123-005957-9f5825 (and CI at strata:75b5ac9b)
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Install stall fixed at strata:75b5ac9b (pushed with cc2d0b3f). Probe 2 (261122-232034-115dd4,
gap3/heal3-chore.out) decided it: the leader's chore runs pin_for every flight ("pin_for:
pinned 13 ... (flights 1)", 500 times), but after the learner's "fetch 4<-3 13/pin: none".
consensus sends the image as the flight starts; the pin is made on the leader's chore; in a
settle tick the network and the fetch drain before any chore, so the fetch always wins, and the
pin went with its flight's end, so the next flight raced again. Not a consensus cause. Fix: one
pin per bundle, kept while its bundle is the latest; a publish or activation queues the release
of superseded pins (release_superseded; ENOSPC stalls, cc2d0b3f's rule). The next flight finds
the pin. Mutant pin-per-flight restores the release at the flight's end; release_short_of_space
now fills the disk after a superseding checkpoint. Jobs at 75b5ac9b: plain suite 0db8ef (A1, A2:
install_gap 3/5, grow_and_heal 3/5, seeds, release_short_of_space), install_test 8f77df (rerun of
12c599), mutants release-fences dbb028, pin-per-flight 9eadac, install-silent 5deea4, install
mutants a90f1a c2dad3 304143 f536b6 717011 d9e958 188ebb 19dc69. Nothing built locally: the jobs
are the first compile.


Round 3 (2026-11-22, this session). Probe 261122-215230-313ac4 read: every failing flight in
install_gap(3) and grow_and_heal(3) stops at the learner's pin fetch ("fetch 2<-1 3/pin: none",
fetch_pin "the source holds no pin at the bundle", validation_failed, SnapshotReleased reason 3,
the next flight the same, 500 times). The leader's pin_for never traces: neither "no pin"
(boundary/recovered mismatch) nor "make_pin threw", and install_needed saw boundary == recovered
(3, 13). Probe 1 logged no pin_for success line, so it could not tell late from never; probe 2
(below) shows the pin is made, but late.

Seed 132's separate cause, fixed at strata:cc2d0b3f: the leader fenced on ENOSPC in
unpin_for's release_pin (an uncaught strata commit on the chore). The release now stalls as an
apply does and repeats after resume(); test release_short_of_space, mutant release-fences.
Found, outside the file set (strata core): index::checkpoint() leaves pending_pin_ set when its
second commit fails, so the next commit pins that generation and nothing releases it before a
reopen (make_pin under ENOSPC).


Install stall, round 2 (lead's note 2026-11-22). cd2a4a40's fix (frames in order, snapshot_timeout
3600) did not settle anything: plain 261121-210217-362497 at cd2a4a40 fails install_gap (3, 5),
grow_and_heal and seeds 16, 103, 108, 122, 123, 132, 140 the same way as before (leader names the
peer in installs_needed, the peer stays at its old applied); CI run 44987690334 (asan, tsan,
FreeBSD, macOS) the same. In a settle every message, fetch, IO and chore drains each 5 ms tick,
so simulated time is not the cause: something fails every flight. Probe 261122-215230-313ac4
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

1. Read the jobs above and CI at strata:75b5ac9b; a compile error or a failing case is fixed at
   the next commit. Paste `campaign brief`'s evidence table.
2. witness: merge the quest head into quest/xapiand/group-heal--witness (voters_test's world is
   world.hh there: carry 74da850c's `uploads` counter, cc2d0b3f/75b5ac9b's leave_behind and
   release_short_of_space), rerun its three jobs.
3. repair: ruled (lead, 2026-11-23, quest Decisions): option (c), through consensus-request-install; built after that quest lands. sim-rows (order 3) after repair and witness.

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

- Install pins live per bundle until superseded (75b5ac9b), not per flight: the latest bundle's
  pin keeps its generation and the set's retired files one checkpoint longer. A learner-side
  retry of the pin fetch was the alternative; it needs a timer on the learner and still loses
  when the leader's chore is behind, so the cached pin is the one mechanism.
- A pin release short of space stalls the chore (as an apply does) rather than fencing or
  deferring (cc2d0b3f).
