# group-heal: journal

State: waiting on jobs 261123-161742-05b8a3 261123-161744-632918 261123-132929-4c9a48 261123-132932-93c39f 261123-132934-042a5a 261123-132937-cc0e98 261123-132940-e656bf 261123-132942-4fa30d 261123-132944-c23175 261123-132947-02dbc5 261123-132949-99dceb 261123-132951-a56998 261123-132954-64a995 261123-132957-e926bf 261123-132959-5cf8ec 261123-133001-5ae453 261123-133003-d59cd0 261123-133007-86835e 261123-133009-a71050 261123-133011-aa75d3 261123-133013-aee60d 261123-133015-0d2a4b 261123-133017-eb0d5e 261123-133019-70f204 261123-133021-74c673 261123-133023-485f6d 261123-133026-0cb903 261123-133028-4d4a77 261123-133030-f99812 261123-133032-e5a45a 261123-133034-06f388 261123-133037-f2372e 261123-133039-33dff2 261123-133041-3dedb8 261123-133044-fef7e2 261123-133046-dd03db 261123-133048-0efca1 261123-133050-acbf8c 261123-133052-66d55b 261123-133054-9fd18a 261123-133056-eaf411 261123-133059-2c8b41 261123-133101-37c462 261123-133103-36b0aa 261123-133107-84c367 261123-133109-38aa74 261123-133111-9b826a (evidence at strata:378130635e74, consensus by CMakeLists.txt's pin, main 6e1122c)
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Results at strata:870ac9f5 (2026-11-23): sim rows 261123-115259-ccf2c8 pass
(simulation_group_corruption included); suite `^group_` 261123-115301-3839f5 passes;
fails-without killed: settle-keeps-isolation 261123-115303-42b792 (complete build, then
"FAIL [3 voters, a settle with the leader cut off]: the settle heals the cut-off leader"), sim
marker-kept 261123-115305-fdca6d, install-damage-escapes 261123-115307-1cf54a. Reviewer probes
261123-115309-f3b6ea failed B4 only, on the probe's stale name filter, not on lost bytes: its
own log lists iq.manifest-1 and iq.seg-1.sx after the reopen, the 2 strata names it counted
before. Probe fixed (reviewer's .scratch/xapiand/review-group-heal-durability/probes2.inc; the
old one kept as probes2.inc.orig-bea1f568): B4 reads each damaged strata file's bytes before
the reopen and requires each whole under its own name or `iq.<name>` after the reopen and
after 50,000 steps. Rerun 261123-123604-a54cf2: B1 ok, B3 ok, B4 ok ("2 strata files before
the reopen, 2 kept whole after it, 2 after 50000 steps"). What fails without F9 is the suite's
`lone` case under mutant quarantine-deletes (261123-092949-ccaf0c).

Settle fix (2026-11-23), after sim rows 261123-092900-8f83e5 at c76c68f9 failed
simulation_group_corruption, file seed 3 ("the cluster settles": node 2 at applied 34, term 6;
nodes 1 and 3 at 35, term 7). Probes (unversioned, .scratch/group-heal/probe2: traced group
answers, consensus core receives and ticks, driver-loop ends): 261123-104515-c0102d,
261123-105909-ff62ab, 261123-114429-9415d8. Read: node 1 is the damaged follower and
reinstalls at 32 from node 2 (term 6 leader); from then node 2 neither receives nor sends
(no core of the three hears it, it hears nobody), while its core ticks at term 6, not busy, its
driver loop alive. Cause, in the test harness: sim::poll_upload isolates the leader when a cut
upload's first chunk is acknowledged, and finish()'s run_until (fault-free steps) polls the
upload, so node 2 was isolated as the reinstall ended; world::settle boots nodes that are down
but kept `isolated`, so the settle ran with node 2 cut off. finish() healed again after the
drain for the same cause, not after run_until. Not product code, and it predates c76c68f9 (the
row never ran before: afeea5 was skipped). Fix strata:870ac9f5: settle clears `isolated` as it
boots the nodes; finish()'s second heal is gone. Test voters_test `settle_heals(3)` (the leader
isolated, the others elect and commit a write, settle must bring it back); mutant
`settle-keeps-isolation` (mutants.py, voters) restores the old settle.

Jobs at 870ac9f5 (no --after): sim rows 115259-ccf2c8 (A3, A5); suite `^group_`
115301-3839f5 (A1-A4); fails-without settle-keeps-isolation 115303-42b792 (A5), sim
marker-kept 115305-fdca6d (A5; e20bce at c76c68f9 cancelled, the row was red), install
install-damage-escapes 115307-1cf54a (A3; also killed at c76c68f9 by aca03e); reviewer probes
115309-f3b6ea. At c76c68f9 (lead, 2026-11-23): suite a41353 passes; mutants b51e5e, acb4e2,
49bd41, 94c61e, 4f156c, ccaf0c, 100baf, 937377, 4d8dfd, aca03e killed. Reviewer probes
b40b0c: B1 and B3 healed in 4 of 4 runs; B4 exit 1 only because it counts strata names, which
sit under `iq.` (its log lists iq.manifest-1 and iq.seg-1.sx kept after the reopen): F9 is
proved by the suite's `lone` case, not by B4.

F7 refusal fix (2026-11-23), after suite 261123-091118-f25b39 at e256792b failed repair `rot`
(seeds 1, 2: the source never damaged, reason empty; the learner never installed). Cause, in
consensus 59ae07f: a learner's `validation_failed` disposes its checkpoint as InvalidApplication
with no receiver term (worker.h dispose_checkpoint), so snapshot_channel.h:141 sends
TransferOutcome::Failed, the source's channel calls `source_failed`, and core.h:829 releases the
flight `Unavailable`. `Rejected` comes only from the learner's core refusing bytes it validated
(install_rejection: Busy, Invalid, StaleTerm; it carries a term). check_refused_pin ran on
Rejected only, so it never ran for a refused stage. Fix strata:c76c68f9: the pin is read again on
every `Unavailable` release (the code comment and group-install.md say why). Fails without: the
reviewed rule is e256792b itself (f25b39 above); mutant `refusal-rejected` restores it at
c76c68f9 (job b51e5e). Nothing built locally.

Jobs at c76c68f9 (all 261123-09xxxx, no --after so one red does not skip the rest): suite
`^group_` 092858-a41353 (A1-A4); sim rows 092900-8f83e5 (A3, A5); reviewer probes
092916-b40b0c; fails-without (A3): refusal-rejected 092932-b51e5e, pin-unchecked
092935-acb4e2, refusal-unchecked 092939-49bd41, handoff-first 092942-94c61e, handoff-fixed
092946-4f156c, quarantine-deletes 092949-ccaf0c, quarantine-kept 092953-100baf, needed-kept
092957-937377, stage-unchecked 093000-4d8dfd, install-damage-escapes 093004-aca03e; (A5) sim
marker-kept 093008-e20bce. They replace round 2's skipped jobs (091128-afeea5 ... 091154-52a6af).

Durability fix round 2 (2026-11-23), review group-heal-durability at strata:bea1f568 (CHANGES:
F7, F8, F9, F12 blocking; F11 nit). One commit, strata:e256792b (pushed); nothing built locally,
the jobs are the first compile. Consensus 59ae07f by --ref.

- F7 (body rot behind a valid footer wedges the learner). Cause: make_pin took each segment's
  CRC32C from its current bytes and checked only its footer, so rot shipped with a matching CRC;
  the learner's activation then met strata's refusal (`ix.install` corrupt_segment), which
  fenced its activation and never reached the source. Fix: (1) make_pin checks each segment
  whole from the bytes it ships (`checked_segment`: footer against the manifest entry, then
  `basic_segment<Layout>(bytes).verify_all()`), a failure is the source's damage (become_damaged,
  hand-off); (2) the learner checks the staged checkpoint the same way before `validated`
  (`check_stage`), a failure answers validation_failed and sweeps; (3) a flight released
  `Rejected` makes the source read its pin's files again against their CRC32Cs (`check_pin`):
  bytes that rotted after the pin are its damage; (4) an activation whose `ix.install` throws
  damage marks the replica damaged, sweeps and answers `activated` damaged (as a staged file
  failing its CRC32C does). Tests: repair `body` (B1 rested, seeds 1, 2), repair `rot` (rot
  after the pin was made and served), install `rotted` (each mechanism unit-level). Mutants:
  pin-unchecked (body), refusal-unchecked (rot), stage-unchecked and install-damage-escapes
  (install rotted).
- F8 (two damaged leaders ping-pong without applied_past). Fix: hand_leadership_away uses no
  host view: each try goes to the next data voter after itself in incarnation order, one further
  per try (a one-byte count in existing padding). Tests: repair `two-damaged` (B3 with the view
  off), `handoff-down` (the ring's next voter is down). Mutants handoff-first (the reviewed rule),
  handoff-fixed (always the next voter). `handoff-any` retired with the view branch.
- F9 (quarantine deletes a lone voter's bytes). Fix: quarantine_strata renames strata names to
  `iq.<name>` (parse_name ignores them); `release_quarantine` removes them in adopt once the
  replica is unmarked (after an activation, or a reopen after one). Tests: repair `lone` (B4:
  every strata file set aside byte for byte, the voter stays damaged); staging_left counts `iq.`,
  so every repaired case and both crash sweeps check none stays. Mutants quarantine-deletes
  (lone), quarantine-kept (manifest).
- F11: the caller guard `if (!log_holding(dir)) ix.commit();` removed; both install-onto-deferred
  mutants removed (mutants.py, install_mutants.py); the deferred tests now guard strata's own.
- F12: `simulation_group_corruption` built: 3 seeds per class (segment, manifest, sealed set
  file), 1,500 faults-row events, a follower damaged while stopped, 1,500 more, then finish().
  finish() now waits for every damaged replica to reinstall before its checks (all rows). The
  row sets STRATA_EAGER_SEGMENTS=1 (segment damage met at the reopen) and runs by name only;
  its world sets no host view, so F8's ring runs there. Mutant sim marker-kept.
- msg from the lead (the suite): test/group/suite.sh replaces install_suite.sh, repair_suite.sh
  and sim_suite.sh. CMake target `group_tests` collects every `group_*_test` of the top
  directory (BUILDSYSTEM_TARGETS) plus group_sim_test, so a new test cannot be left out again.
  Default regex `^(group_|simulation_group_)`; the jobs split it in two for the 30-minute cap.
- needed-kept mutant: its first hunk named `reason`, which the code now does; it keeps the second.

Jobs at e256792b: suite `^group_` 261123-091118-f25b39 (A1-A4); sim rows 261123-091128-afeea5
(A3, A5); fails-without: pin-unchecked 4dfb18, refusal-unchecked 514300, handoff-first 07e7fa,
handoff-fixed d9d2bd, quarantine-deletes 79be76, quarantine-kept 8d3005, needed-kept a0ef3e,
stage-unchecked 71b1c2, install-damage-escapes c71a59, sim marker-kept 1b98a3 (all
261123-0911xx). Reviewer probes rerun 261123-091154-52a6af (plain: B1 and B3 must heal; B4's
check counts parseable strata names, which now sit under `iq.`, so it reports 0 by design: the
`lone` case is its replacement).

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

1. Read the jobs at 378130635e74 (State); then CI at 378130635e74 (asan, tsan, build-test).

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

At strata:378130635e74 (2026-11-23): the durability fix rounds as accepted at 870ac9f5 plus the
lead's consensus pin (main 6e1122c, CMakeLists.txt only). Every job builds a fresh `build-pin`
on the pin, no CONSENSUS_DIR. Earlier rows (a1e786f4, 870ac9f5) are superseded by these.

| Criterion | Evidence |
|---|---|
| A1 | suite 261123-161742-05b8a3 (suite.sh `^group_`: group_install, group_voters incl. install_gap, grow_and_heal 3/5, reopen_short_of_space, deferred_learner_crash, settle_heals; group_file_set, group_catalog*, group_footprint, ...); fails without (install_mutants.py): skip-file-set 261123-133021-74c673, keep-unchecked 261123-133023-485f6d, write-past-failure 261123-133026-0cb903, marker-before-sync 261123-133028-4d4a77, holder-unordered 261123-133030-f99812, no-resume 261123-133032-e5a45a, pin-names-unchecked 261123-133034-06f388, serve-past-length 261123-133037-f2372e; (mutants.py, group_voters_test): release-at-open 261123-133046-dd03db, release-fences 261123-133048-0efca1, pin-per-flight 261123-133050-acbf8c, close-skips-install-tasks 261123-133052-66d55b |
| A2 | suite 261123-161742-05b8a3 (group_voters: seeds 8, 12, 16, 18, 23, 24, 27 settle by install, 40 ungated seeds 101-140); fails without: install-silent 261123-133044-fef7e2 |
| A3 | suite 261123-161742-05b8a3 (group_repair: segment, manifest, file, apply, read, refusal, staged, leader, source, body, rot, deposed, two-damaged, handoff-down, lone, crash sweeps; group_repair_eager; group_install rotted) and sim rows 261123-161744-632918 (simulation_group_corruption: segment, manifest and set-file damage, 3 seeds each; device rows: non-prefix logs); fails without (repair_mutants.py): open-rethrows 261123-132929-4c9a48, unmarked 261123-132932-93c39f, quarantine-first 261123-132934-042a5a, set-damage-fences 261123-132937-cc0e98, no-ack 261123-132940-e656bf, waiters-unanswered 261123-132942-4fa30d, apply-damage-fences 261123-132944-c23175, read-damage-passes 261123-132947-02dbc5, activate-unchecked 261123-132949-99dceb, marker-kept 261123-132951-a56998, leader-keeps 261123-132954-64a995, no-retry 261123-132957-e926bf, pin-unchecked 261123-132959-5cf8ec, refusal-unchecked 261123-133001-5ae453, refusal-rejected 261123-133003-d59cd0, reason-kept 261123-133007-86835e, handoff-first 261123-133009-a71050, handoff-fixed 261123-133011-aa75d3, quarantine-deletes 261123-133013-aee60d, quarantine-kept 261123-133015-0d2a4b, stage-caught-up 261123-133017-eb0d5e, needed-kept 261123-133019-70f204; (install_mutants.py): stage-unchecked 261123-133039-33dff2, install-damage-escapes 261123-133041-3dedb8 |
| A4 | suite 261123-161742-05b8a3 (group_witness: loss of each member, join); fails without: witness-holds-range 261123-133056-eaf411, witness-refuses-image 261123-133059-2c8b41 |
| A5 | sim rows 261123-161744-632918 (suite.sh `^simulation_group_`: faults, barriers, membership 3 and 5, growth 1, device 3, corruption); fails without (sim_mutants.py): apply-skips-entry 261123-133101-37c462, read-skips-probe 261123-133103-36b0aa, apply-abandoned-chunk 261123-133107-84c367, recovery-as-found 261123-133109-38aa74, marker-kept 261123-133111-9b826a; (mutants.py) settle-keeps-isolation 261123-133054-9fd18a |
| A6 | `docs/mechanisms/group-install.md`: "Status: built (quest group-heal" |

A6 also: `docs/mechanisms/simulator.md` at strata:378130635e74 (Status: CURRENT, sim-port and sim-rows). The tool checks one path per row.
Job 261123-131741-b1afdd failed with exit 127: its command named test/group/repair_suite.sh, renamed to test/group/suite.sh; the repair cases ran in 261123-161742-05b8a3.


Reviews: durability accepted by the lead at 870ac9f5 (APPROVE, F7/F8/F9/F12 closed); reviewer
probes at 870ac9f5: 261123-123604-a54cf2 (B1, B3, B4 ok).

## Result

<When ready for review: what changed, its measured effect with numbers and the jobs behind them,
and what it leaves open. Results and how it works go in the docs of the repository it changed
(docs/perf/, docs/mechanisms/, a library's own); name them here. `campaign map` shows this.>

## Rulings I made

<Decisions inside the quest's scope, so the reviewer can check them.>

- F7 round 2: three checks, one per place bytes can rot: the source checks what it ships
  (whole, at the pin), the learner what it staged (whole, before validated), and a rejected
  flight makes the source re-read its pin (CRC32C only: make_pin's whole check vouched for those
  bytes, so any change since is a CRC mismatch). Cost: one more read of each segment at the pin
  (with the CRC pass, one map), one more read of the staged segments per install, and one read of
  the pin per failed flight (the pin race's included). A refused stage is swept whole.
- F7 refusal (c76c68f9): the source cannot tell a learner's refusal from a lost transfer, since
  consensus reports both as `Unavailable`; it reads its pin again on each (a retried flight waits
  rpc_timeout, so at most one CRC pass of the pin per retry). Asking consensus to report a failed
  validation as Rejected was the alternative: a consensus change for a cost the ruling above
  already accepts.
- Settle (870ac9f5): a world settle heals isolation as it heals crashes (it boots every node),
  so no fault-free stretch can leave a fault in force into a settle; the alternative, keeping a
  cut upload from isolating outside fault steps, would change the faults rows' coverage.
- F8: one hand-off mechanism, no host view, applied_past kept only for the stall hand-off (D3).
- F9: set-aside names go at adopt once unmarked (every recovery scans the directory once for
  them); a name already set aside keeps its first copy.
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
