# group-heal: journal

State: waiting on CI at strata:a1e786f4 (no job of mine running; 261123-025901-9a4aba failed exit 127, a probe, not a criterion). Remaining: A3 needs task repair and A5 needs task sim-rows, both unbuilt, after consensus-request-install; landing after consensus-request-install
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Durability fix round 1 (2026-11-23), review at strata:908b58c0 (CHANGES, F1 and F2 blocking).
Fix at strata:a1e786f4; nothing built locally, the jobs are the first compile.

- F1 (a pin held, reopen on a full disk fails). Cause: recover() released the manifest's pins
  synchronously, and strata's release commits, so ENOSPC failed the open. Fix: recovery reads
  the pinned generations (`held_pins`, install.hh) into the install state's `stale` list and
  queues release_superseded, which releases them first and stalls on ENOSPC as before
  (cc2d0b3f's rule); release_pins_held at close releases them too. Test
  `reopen_short_of_space(3)` (voters_test, R2's case: opens on a full disk, stalls unfenced
  with the pin kept, resume() releases it, settles, digests equal). Mutant `release-at-open`
  releases at recovery (the reviewed behavior).
- F2 (crash in a deferred learner's activation, `no manifest log`). Cause: the ordering, not
  strata's open. install's crash contract ("old content or installed") presumes the old content
  is committed; a fresh learner's index is deferred, and `index::install` writes its segment
  copies before its first manifest, which strata's open must read as damage (a committed index
  that lost its log looks the same). Fix in group's activation order (install.hh `activate`):
  a deferred index (no manifest log) commits its empty state before it installs, so a crash
  opens the old generation and resume() finishes; a crash in that commit leaves
  `manifest-1.tmp` alone, D3-C's uncreated residue. Tests: install_test `deferred` (crash at
  every mutating operation from the stage through the activating recovery, process and power
  seeds 1-4, persisted or not: empty replica or complete new one) and voters_test
  `deferred_learner_crash(3)` (R1d's sweep: a power cut on the learner's directory disk at
  every mutation from its rejoin through the settled install; each reboot settles, digests
  equal, no staging name; the premise "index deferred" is asserted). Mutant
  `install-onto-deferred` (both mutant files) drops the commit.
- Not fixed, pre-existing (D112, strata core, outside the file set): a deferred index's first
  commit *with documents* writes its segments before its first manifest, so a crash between
  them leaves the same `no manifest log` directory. That is any group's first checkpoint, and
  F2's fix inherits it when a deferred learner had applied documents (a voter that never
  checkpointed, then fell behind); a fresh learner applies nothing before its install. Probe
  at strata main 00eb018d: job 261123-025901-9a4aba (.scratch/xapiand/group-heal-first-commit).
  The fix belongs in strata's open of a never-committed directory (residue a first commit
  leaves, marked before its first segment); for the lead.
- F3-F6 untouched. F3's `trim_sources` gains the `stale` condition (same line, no F3 change).

Jobs at a1e786f4 (priority 3): confirmations, the reviewer's probes unchanged but for the
commit (.scratch/xapiand/group-heal-fix1): F1 = 261123-025733-6d601b (R2, R3), F2 =
261123-025741-994ce2 (R1c, R1d, R1h). Suite 261123-025745-1cfc1e. Fails-without (mutants that
restore the reviewed code): release-at-open 3c979b, install-onto-deferred (voters) 60799b,
install-onto-deferred (install_test deferred) 4a4f12. Touched-code mutants: release-fences
7e8f02, pin-per-flight 5e3091, install-silent 3c291c, close-skips-install-tasks b0df66,
install holder-unordered 68b83b, no-resume 5422c2.

Round 5 (2026-11-23): strata main taken (group-catalog, strata:00eb018d). Merge at
strata:11672618, then one fix at strata:908b58c0; pushed. Nothing built locally: the jobs below
are the first compile. Jobs at 908b58c0 (priority 3): suite a25890 (install_suite.sh, every
group_ test: catalog, install, voters, witness, lifetime, lone_voter, file_set, footprint);
voters mutants release-fences 01f061, pin-per-flight f94527, install-silent 2fb770,
close-skips-install-tasks f55c10 (new); witness 711463, witness-holds-range 94e042,
witness-refuses-image dfeb44.

Conflict hunks and their resolution (for the durability review):
- group.hh includes: both (`<charconv>` for serve's parse, `<condition_variable>` for
  open_blocking/close_blocking).
- group.hh close(): main's (checkpoint loop unless fenced, row closed, leave(), group_fenced on a
  fenced close). The quest's `release_pins_held()` moved from the old teardown item to an
  `on_chore` after `driver().close()` and before `leave()`, unfenced only: no notice pins after
  the row closes, and the index stays open (the catalog's), so the release commits on the
  chore under its own lease as before. A fenced close leaves its pins to the next recover.
- group.hh recover(): main's (no index open; the borrowed `*ix_`) plus the quest's
  `release_pins` and `adopt()`. `release_pins` now returns when the directory holds no manifest
  (a deferred index: before, it ran only inside the open's try, past `index_not_created`).
- group.hh members: main's (no `dir_`). The quest's `shared_ptr<jn::IO> dir_` existed so a serve
  that took a pin read on past the teardown; with a borrowed directory that is unsound, so
  `pinned` carries no directory, every use reads `files_.io()` (new accessor in file_set.hh),
  and `serve()` counts itself (`serving_`, a 4-byte atomic): the teardown, after `closed_` and
  the chore idle, waits for serves in flight, so close returns with no read of the directory
  left. `!ix_` guards in pin_for, release_superseded and release_pins_held dropped (never null).
- voters_test.cc: the quest's (the world lives in world.hh); main's `default_limits` borrow
  edit auto-merged. world.hh gets main's migration: `borrowed<group_type>` per data node, config
  without uuid/strata options (the uuid moves to `emplace`).
- mutants.py: main's side. close-keeps-directory (`dir_.reset()`) and destroy-keeps-member
  (detach in the destructor) target code group-catalog removed; main's close-keeps-member,
  attach-at-construction, failed-open-stays, payload-unmarked and the catalog mutants replace
  them. Every pattern in mutants.py and install_mutants.py applies exactly once on the merge
  (checked by script). Docstring names both quests.
- Not a conflict: install_suite.sh builds group_catalog_test (ctest `^group_` runs its cases).

Fix found by the merge, strata:908b58c0: a learner's stage and activation are tasks the group
spawns on the loop, outside the chore's `running_`. Main's teardown waits only for the chore,
so a close returned while a stage awaited its pin fetch, and the host (the catalog) could then
close the index and destroy the group under it. The idle wait now also counts install tasks
(`tasks_` under mu_, `task_scope` at each task's end); an activation a close refused answers
and fences nothing. Test `close_mid_install` (voters_test, 3 voters): a fenced learner closes
with its pin fetch pending; the close must not end until the fetch fails. Mutant
close-skips-install-tasks restores the chore-only wait (job f55c10).

grow_and_heal's and witness join's changed expectation (rounds 4, carried into the merge): the
replaced voter stays one entry behind (applied = committed = N-1, the others N) after the core
removes it. consensus at 8780e44: core.h:284-286 (`removal()`, D3: the leader's next admitted
Tick proposes the removal of a committed replacement's leaving member), core.h:1305-1309 (D3:
whichever leader holds the committed membership removes it; D4: a member whose removal
committed is no longer replicated to), core.h:468-471 (`replicas()`: the committed membership's
members until their removal commits, D4). So the removed voter never learns the commit of its
own removal; the tests wait for the next leader's membership to drop it, retire it, and check
the remaining members' digests.

Round 4 (2026-11-23). Suite 261123-005919-bdfe69 at 75b5ac9b: installs complete; grow_and_heal
(3, 5) failed "the cluster settles after the promotion" with the leaving voter one entry behind
(applied = committed = 22, the rest 23). A test expectation that no longer fits, not a stall:
consensus's core removes a committed replacement's leaving voter on the leader's next Tick (D3,
core.h removal()) and stops replicating to it once that commits (D4), so the removed voter never
learns its removal's commit; the test then proposed the removal itself. Fixed at strata:9ccddbf7:
grow_and_heal cuts the leader once the promotion commits, waits for the next leader's membership
to drop the voter (removed_by_core), retires it, settles, checks every node's membership. The old
expectation failing is bdfe69 (and CI 45132116434 on every platform).
CI 45132116434 also failed group_footprint on Linux only (own heap 1053, 1054, 1070 B > 1024;
already at cd2a4a40, main green): install's members on libstdc++. Fixed at strata:82ea744c: the
leader's installs needed and pins live in one struct made at the first install notice and freed
when empty (saves 88 B of the object on libstdc++). Proof is CI's Linux footprint rows at
82ea744c (macOS passes either way).
Jobs at 82ea744c: suite b1fede (A1, A2), voters mutants release-fences 375c2a, pin-per-flight
a2527e, install-silent 1871b5. (Suite 1eec9c and install 7941e0 were cancelled by the tool's
same-role supersede, see below; b1fede replaces both: the suite runs group_install.)
Witness: quest head merged into quest/xapiand/group-heal--witness at strata:e4828884 (world.hh
carries the uploads counter, cut_leader, holds, removed_by_core; witness_test join waits for the
core's removal as grow_and_heal does). Jobs: witness 0ebc24, mutants witness-holds-range e0f143,
witness-refuses-image 73ffe0, group suite at the witness head 499a3a.

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

1. CI at strata:a1e786f4 (asan, tsan, build-test); a red case is fixed next commit.
2. Report the first-commit window to the lead (probe 261123-025901-9a4aba failed exit 127, so the
   probe needs a rerun or the lead's call; the cause is in the Doing section).
3. After consensus-request-install lands: repair (A3), then sim-rows (A5); then the When-done
   review gate rows. The quest lands after consensus-request-install.

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

At strata:a1e786f4 (2026-11-23; the code is final, its durability review accepted there: F1 and
F2 confirmed by suite 261123-025745-1cfc1e). `campaign brief`'s rows, with the task rows:

| Criterion | Evidence |
|---|---|
| A1 | 261123-025745-1cfc1e (install_suite.sh, 16 of 16 `^group_` tests pass: group_install, group_voters, group_file_set, group_catalog*, group_footprint, ...; replaces a25890); fails without: install mutants skip-file-set 261121-210245-1aac19, keep-unchecked 261121-210246-3f5652, write-past-failure 261121-210246-ad33f8, marker-before-sync 261121-210247-fbfca9, holder-unordered 261121-210247-218fb3 and at a1e786f4 261123-025822-68b83b, no-resume 261121-210247-d363e5 and at a1e786f4 261123-025825-5422c2, serve-past-length 261121-210248-1c5575, pin-names-unchecked 261121-210248-b81f77; at a1e786f4 release-at-open 261123-025756-3c979b, install-onto-deferred 261123-025800-60799b and 261123-025818-4a4f12, release-fences 261123-025803-7e8f02, pin-per-flight 261123-025807-5e3091, install-silent 261123-025811-3c291c, close-skips-install-tasks 261123-025815-b0df66. Task install: merged at cd2a4a40, its step-1 jobs superseded by the above (261121-210245-1aac19 replaces 261121-173210-076626). Confirmations of F1 261123-025733-6d601b and F2 261123-025741-994ce2 |
| A2 | 261123-025745-1cfc1e (the ungated schedules, install_gap, grow_and_heal 3 and 5 are group_voters cases); fails without: install-silent 261123-025811-3c291c at a1e786f4, 261121-210236-dca37d (earlier), 261121-133923-ba5a0d |
| A3 | none. Task `repair` is unbuilt: it waits on consensus-request-install (ruled (c), Decisions 2026-11-23). Related, not the criterion: install_test `damaged` (a sealed file with a flipped byte is fetched again, 261123-025745-1cfc1e) and file_set_test (a flipped byte fails recovery on its CRC32C); neither fences and reinstalls through a request, and no non-prefix device runs |
| A4 | 261123-041202-998ec3 (group_witness at a1e786f4); fails without: witness-holds-range 261123-021427-94e042 and 261123-014651-e0f143, witness-refuses-image 261123-021429-dfeb44 and 261123-014655-73ffe0, 261121-214749-38a541, 261121-214752-5dffc3 (task witness: merged at e48288841e96; its mutant rows are brief's at 908b58c0d2ee). 261123-021425-711463 (at 908b58c0) is replaced by 998ec3 |
| A5 | none. Task `sim-rows` is unbuilt (order 3, after repair). The port is merged (task sim-port, 6d870c0536f2; sizing 261120-232059-b57cfe, determinism 261120-180334-1b82c5, lost-write mutant 261120-180342-65db6c); no job at a1e786f4 runs `simulation_*`, and the group's fault rows, checker rows and four mutants do not exist yet |
| A6 | [artifact] `docs/mechanisms/group-install.md` and `docs/mechanisms/simulator.md` in strata at a1e786f4 (last changed by strata:a1e786f4; nothing to update: the fix round is described). group-install.md, Activation: "A deferred index (a fresh learner's, which never committed) commits its empty state first: install writes its segment copies before its manifest, and a directory holding segments with no manifest log is damage to strata's open, not an interrupted create." (F2); the binding: "pins a crash or a fenced close left in strata's manifest go after the next open: recovery reads them (`held_pins`) and queues their release on the chore, where short of space it stalls as above, so a full disk never fails the open." (F1); Proof names `reopen_short_of_space`, `deferred_learner_crash` and mutants `release-at-open`, `install-onto-deferred`. simulator.md: "`sim/` runs a cluster on one thread in simulated time. A seed fixes every draw, so a run is a function of its seed", its Tests table, and "The group's fault rows are `sim-rows`" (Status) |

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

- F2 (a1e786f4): the fix is group's activation order, not strata's open: strata cannot tell an
  interrupted first install from a committed index that lost its log, so the group makes the
  old state committed before install, as install's contract presumes. The general first-commit
  window with documents (above) is strata's and the lead's.
- F1 (a1e786f4): a pin an earlier process left is released on the chore after recovery, not
  in it; with space short the replica stalls (and a stalled leader hands off, D3) until
  resume(). The fails-without mutants restore the reviewed code's behavior in place (F1 releases
  after adopt instead of before; same commit on a full disk).
- Install pins live per bundle until superseded (75b5ac9b), not per flight: the latest bundle's
  pin keeps its generation and the set's retired files one checkpoint longer. A learner-side
  retry of the pin fetch was the alternative; it needs a timer on the learner and still loses
  when the leader's chore is behind, so the cached pin is the one mechanism.
- grow_and_heal and witness join no longer propose the leaving voter's removal: consensus's
  core does it (D3), and the test checks it does, across a leader change (A1's "remove").
- The leader's install state (installs needed, pins) is made on demand, so an idle group keeps
  the 1024 B own-heap bound on libstdc++; the bound stays as it is.
- A pin release short of space stalls the chore (as an apply does) rather than fencing or
  deferring (cc2d0b3f).
- Merge (11672618): pins are released at close on the chore after the row closes, unfenced
  only; a fenced close's pins wait for the next recover's release_pins (strata commits on a
  fenced replica are not attempted).
- Merge: serve's directory guard is a count the teardown waits on, not a shared directory: the
  catalog owns the directory (D169). No deterministic test: the world calls serve on the loop
  thread, so no serve is ever in flight at a teardown there; TSan in CI covers the threads.
- 908b58c0: close waits for in-flight install tasks; a stage waiting on its pin fetch delays a
  close until the fetch answers (the fetch lane's own deadline bounds it).
