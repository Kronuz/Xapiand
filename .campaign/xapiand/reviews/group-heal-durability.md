# group-heal review: durability

Commit: strata:870ac9f57c12bcb0534e4c4f288d01dd0bd0984e
Verdict: APPROVE
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Lens: durability, carrying correctness for repair. This is a re-review of what changed since the
accepted durability review at strata:a1e786f4: `git diff a1e786f4 bea1f568 -- include sim test
CMakeLists.txt` (task repair: repair.hh, group.hh, install.hh, test/group/repair_*; task sim-rows:
sim/**, test/group/sim_*). The first round's review was at strata:908b58c0 (F1-F6 below keep
their ids). Consensus at 59ae07f (the repair jobs' ref).

## Evidence I relied on

- Repair at 06bfb2f5 (task tip; the merge into bea1f568 changes only CMakeLists.txt and docs
  around it): group_repair and group_repair_eager 261123-080614-8b99ae and 261123-082123-99f90f
  passed (segment, manifest, file at open; apply; read; refusal; staged; leader; source unread and
  at rest; deposed; the four crash sweeps). The whole group suite at d457da76 code
  (261123-082124-87d11c) passed. Mutants killed at d457da76 (261123-0631xx-0632xx: open-rethrows,
  unmarked, quarantine-first, set-damage-fences, waiters-unanswered, apply-damage-fences,
  read-damage-passes, activate-unchecked, marker-kept, leader-keeps, no-retry, reason-kept,
  handoff-any, stage-caught-up, needed-kept) and at 06bfb2f5 (no-ack 261123-080623-cab898,
  pin-unchecked-footer 261123-080626-53f862). Enough for the cases they name; the reruns at
  bea1f568 (261123-0835xx) were queued and I relied on none.
- The first round's fixes (F1, F2) at a1e786f4: suite 261123-025745-1cfc1e and the reviewer's
  probes rerun (261123-025733-6d601b, 261123-025741-994ce2), mutants release-at-open and
  install-onto-deferred killed (both before main's install() guard merged; see risk 5).

## What I checked myself

Probes live in `.scratch/xapiand/review-group-heal-durability/` (`run2.sh probes B1 B3 B4`
inserts `probes2.inc` into an exported repair_test.cc; `run2.sh guard` removes install.hh's caller
guard; both need `CONSENSUS_DIR`, a consensus 59ae07f checkout). Probe jobs 261123-084426-3eb719
(B1, B3, B4; exit 1 because B1-rested, B3-off and B4 fail their checks) and 261123-084430-3b9e96
(guard; exit 0).

- Risk 1, body damage behind a valid footer. make_pin computes each segment's CRC32C from the
  source's current bytes (install.hh:326) and checks only its footer (check_footer). So rot in a
  section body passes the pin, the transfer and the learner's staged CRC check. It is caught by
  the learner's `ix.install`, which loads the source whole (`verify_all`). Probe B1 flips one byte
  in the middle of the leader's segment while the leader runs, after it mapped the segment. It
  then damages a follower's manifest. The learner never repairs: after 200,000 steps it is still
  damaged with 0 installs, its directory's mutation count is frozen at 123, the source is not
  damaged, and commit stays at 31 (seeds 1 and 2). From the code, the corrupt_segment escapes
  from the activation's chore item. drain() answers damage with `step::more` (group.hh:840).
  Install items are never refused while damaged (group.hh:817), so the activation reruns and
  never answers consensus. Nothing tells the source. When the source was rebooted first ("unread"),
  its pin's checkpoint maps the segment and meets the damage, the source is marked damaged and
  hands off, and the cluster heals (B1 unread, both seeds). Bytes that rot after the pin fail the
  learner's stage CRC, and the stage is retried from the same pin until the next bundle. That pin
  is then B1's case. Finding F7.
- Risk 2, the hand-off. With the test world's host view (`applied_past`), two damaged replicas
  hand to the sound one: B3 "view on" moves leadership 1 -> 3 and both heal (seeds 1, 2). No
  production host sets `applied_past` (grep: only test/group/world.hh:245; neither sim/, the
  catalog nor Xapiand set it). Without it, hand_leadership_away takes the first voter that is not
  the leader (group.hh:1572). B3 "view off": node 1 hands to node 2 and node 2 hands back to node 1,
  125 leadership changes in 200,000 steps (`1 0 2 0 1 0 2 ...`). Neither ever reaches the sound
  node 3, and neither heals. Every leader is damaged, so propose throws replica_damaged and the
  group takes no write. This is round 3's ping-pong, still present on the path every host uses.
  The leadership test the brief asks for exists: `leader` (repair_test.cc:449) transfers
  leadership onto a damaged replica and checks it hands off and repairs, but only with the host
  view. While it leads, a damaged replica serves nothing: read_applied, propose, serve and
  pin_for all check `damaged_`, and prefix readers gate on `readable()`, which enter_damaged
  closes. Finding F8.
- Risk 3, `bounded`. group.hh:252 recovers only `stored` and `closed`, and sends everything else
  to `driver.create`. Today's journal pin (9e11713) has no `bounded`. On journal main, `bounded`
  reaches `Journal::create_shared` -> `Host::create_group`, which throws logic_error when the status
  is not `none` (journal main host.h:521-522). The open fails and the group fences locally. A
  bounded replica therefore never rejoins with a hole that consensus treats as complete. It does
  not rejoin at all: `lost`, which exists today, takes the same branch. journal main's own
  recover loses a bounded group and throws Corruption (journal.h:595-597), because a consensus
  Store holds a group only through its latest checkpoint record. The right answer for strata::group
  is "lost: rejoin under a new id" (a replace), not create. Under today's pin this cannot arise.
  It belongs to strata-journal-pin, whose device row will meet it if a non-prefix cut yields
  `bounded`. Finding F10, not blocking here.
- Risk 4, crash at each step. The marker goes first (mark_damaged: tmp, sync, rename, directory
  sync). Activation checks the staged files, `ix.install` runs, set files are replaced, the
  directory is synced, and only then does clear_damaged run (install.hh:575-605). recovery's
  `resume` repeats a complete activation. The four crash sweeps (manifest and file, directory and
  host disk, 91 and 85 mutations) assert "damaged exactly while marked" and "never unfenced over
  damaged bytes" at every cut, and passed at 06bfb2f5. A crash mid-quarantine reopens through
  open_index again (still damage, marker present). After the copy and before the ack, consensus
  persisted the image before ActivateImage and resume finishes. A crash after the install but
  before the sweep repeats an idempotent activation. That is enough for the marker protocol. The
  sweep does not cover segment-class damage or B1's path, which F7 covers.
- Risk 5, the install guard. Removing install.hh:589 (`if (!log_holding(dir)) ix.commit();`)
  leaves strata's own guard (`if (!m_.generation) commit_empty()`, index.hh:1964). Job
  261123-084430-3b9e96 passes without the guard: install `deferred` (760 runs over 75 operations),
  voters' `deferred_learner_crash` (42 power cuts over 41 mutations), and repair's manifest crash
  sweeps on the directory and host disks (92 and 33 cuts), which cross quarantine's deferred
  index. For an index built by open_index, "no manifest log on disk" and "generation 0" are the
  same state, so the caller guard closes no extra case. It is redundant, and the
  install-onto-deferred mutants now survive. Finding F11.
- B4, a lone voter (D7's short group) whose manifest log is damaged. On reopen, open_index marks it
  and `quarantine_strata` deletes every strata name (repair.hh:93): 2 strata names before, 2
  after the damage, 0 after the reopen. The replica stays damaged forever, because there is no
  replica to reinstall from. Before this quest the open failed and the bytes stayed. The same
  deletion happens on every replica when damage is correlated (a strata bug that writes a bad
  manifest or segment on every node), and nothing is left to install. Finding F9.
- Sim rows: `simulation_group_corruption` still exits 77 (skipped) at bea1f568
  (sim/CMakeLists.txt:58-60, test/group/sim_test.cc:1050). Its skip text says it waits for task
  repair, which has merged. Finding F12.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | no | defect | Closed. A held pin no longer fails the open on a full disk: recovery queues the release, which stalls on ENOSPC. | reopen_short_of_space; mutant release-at-open killed (round 1 jobs above) | - |
| F2 | no | defect | Closed. A deferred learner's activation crash reopens: the activation commits first. strata's own install() guard now does it too (F11). | install `deferred`, voters `deferred_learner_crash` (passed again in 261123-084430-3b9e96) | - |
| F3 | no | defect | Closed. `needed` maps a peer to its flight's transfer, and every release of that flight erases it. | test `deposed`; mutant needed-kept killed 261123-063248-279216 | - |
| F4 | no | gap | Open, unchanged. serve_scope still calls notify_all after its decrement (group.hh:1356-1358), and nothing calls group::serve off the loop thread. Already in hunt.md. | group.hh:458, :1353-1358 | As in round 1. |
| F5 | no | nit | Open, unchanged (in hunt.md). | voters_test, witness_test | As in round 1. |
| F6 | no | nit | Open, unchanged: recovery treats every pin in the borrowed index's manifest as the group's (`held_pins`). | install.hh:349 | As in round 1. |
| F7 | yes | defect | A source segment whose body rotted behind a valid footer is never repaired from, and the learner wedges. make_pin's CRC32C is taken from the rotted bytes, so the stage and its check pass, and the learner's `ix.install` throws corrupt_segment. drain() keeps an install item that throws damage (`step::more`) and never refuses install items, so the activation reruns and never answers consensus. The source is never told and stays leader with the damage. The quest's ruling "caught only at the learner's activation, which fences that learner" is not what happens. | Probe B1 rested, job 261123-084426-3eb719: learner damaged with 0 installs after 200k steps, dirmut frozen, source undamaged, commit stuck (seeds 1, 2); group.hh:817, :840; install.hh:326 | (a) The source checks what it ships: make_pin verifies each pinned segment whole (strata's verify path, or `load_segment(..., whole=true)`) before taking its CRC, and a failure is its damage, as the footer's already is. This costs one read per segment per bundle, which the CRC pass already pays. (b) In any case, an install item that throws damage must end its item: answer `activated` damaged (as a staged-file failure does) or `activation_failed`, never `step::more`. Test: B1's rested case in repair_test (rotted body behind a valid footer on the running leader; the learner reinstalls and the source repairs), and a mutant that drops (a). |
| F8 | yes | defect | Two damaged replicas pass leadership between them forever on every production host. Without `applied_past`, which no host sets (not the catalog, Xapiand or sim/), hand_leadership_away picks the membership's first other voter. Two damaged voters pick each other and never reach the sound one. The group then takes no writes, since every leader is damaged. Round 3 fixed this only for hosts with a view, which only test/group/world.hh has. | Probe B3 view off, job 261123-084426-3eb719: 125 leadership changes `1 0 2 0 1 0 2 ...`, never node 3, neither heals (seeds 1, 2); view on heals (1 -> 3). group.hh:1564-1576 | Without a view, rotate: each hand-off picks the next voter after the previous target, so the sound one is reached within one rotation. Or make the view mandatory for a group with repair (the catalog and Xapiand then set it). Test: B3 with `world(..., false)` (two damaged, the damaged one leading, no host view): both heal. |
| F9 | yes | defect | open_index deletes a damaged index outright (`quarantine_strata` removes every strata name) before any replacement exists. On a lone voter (D7), or when every replica holds the damage (a correlated bug), there is no source, so the group's only bytes are gone and the replica stays damaged forever. Before this quest the open failed and the bytes stayed. | Probe B4, job 261123-084426-3eb719: strata names 2 -> 0 at reopen, damaged after 50k steps; repair.hh:85-94, group.hh:200-205 | Set the bytes aside instead of deleting them: rename each strata name under a quarantine prefix that parse_name ignores, and remove them in the activation's sweep after clear_damaged, so they go only once a verified image replaced them. Install admission then needs room for both; a full disk stalls the repair rather than losing data. Test: B4 (lone voter, damaged manifest: the bytes remain after reopen, under the quarantine names), and the crash sweeps extended to see the set-aside names gone after the repair. |
| F10 | no | gap | Under journal main, a `bounded` (or `lost`) group goes to `driver.create`, which throws logic_error. That is safe: no hole is taken for complete. But the replica can never rejoin, and the right path is lose and replace. It cannot arise at today's pin (9e11713 has no `bounded`). | group.hh:252; journal main host.h:521-522, journal.h:595-597, docs/mechanisms/lost-bound.md | For strata-journal-pin: treat `bounded` like `lost` explicitly (Host::lose, then refuse the open with a typed error the placement layer answers with a replace). Test there: the device row at a cut that yields `bounded`. |
| F11 | no | nit | install.hh:589's caller guard is redundant with strata's install() guard (index.hh:1964). Removing it, the deferred, voters and repair crash cases all pass. Its two install-onto-deferred mutants now survive, so they prove nothing. | Job 261123-084430-3b9e96 (exit 0) | Remove the line and both install-onto-deferred mutants. The deferred tests then guard strata's own guard. |
| F12 | yes | gap | A3's simulator half and A5's "corruption of every file class" are unbuilt: `simulation_group_corruption` still skips with 77, although task repair has merged. | sim/CMakeLists.txt:58-60; sim_test.cc:1050-1052; docs/gates/phase-3r.md:36 | Build the row (segment, manifest and set damage under the simulator's faults, checker green), and run its mutant as `--fails-without`. |

## Not run

- Sanitizers: nothing under TSan or ASan. CI at bea1f568 had not run (the branch was not pushed).
- The repair reruns at bea1f568 (261123-0835xx): queued when I wrote this, and I relied on the
  06bfb2f5 and d457da76 runs.
- B1 with a set file or manifest log rotted behind the source's own checks: only segment bodies.
- Whether F7's wedged activation also blocks close(): I did not close the learner in B1.
- Five-voter repair, and repair during a membership change.
- F10 against a built journal main: reasoned from journal main's source, not run.

## Re-check (round 2 fixes)

Commit: strata:870ac9f57c12bcb0534e4c4f288d01dd0bd0984e
Verdict: APPROVE

Range strata:bea1f568..870ac9f5: e256792b, c76c68f9, 870ac9f5. The round 2 review above was at
strata:bea1f568.

Evidence relied on, not rebuilt: simulator rows 261123-115259-ccf2c8 (corruption row passed, not
skipped), suite `^group_` 261123-115301-3839f5, mutants killed 42b792 (settle-keeps-isolation),
fdca6d (marker-kept, corruption row), 1cf54a (install-damage-escapes), and at c76c68f9 b51e5e
(refusal-rejected), acb4e2 (pin-unchecked), 49bd41 (refusal-unchecked), 94c61e (handoff-first),
4f156c (handoff-fixed), ccaf0c (quarantine-deletes), 100baf (quarantine-kept). My round 2 probes
B1, B3 and B4 at 870ac9f5: 261123-123604-a54cf2, all pass (B1 rested and unread heal on both
seeds; B3 view off: `1 0 2 0 3`, `1 0 2 0 1 0 3`, healed).

The B4 rewrite (`probes2.inc`, original kept as `probes2.inc.orig-bea1f568`) is fair and stronger
than mine. Mine counted strata names, which the fix now moves under `iq.`, so it would fail a
correct fix. The rewrite reads every strata file's bytes after the damage and before the reopen,
then requires each one byte-equal under its own name or `iq.<name>`, both after the reopen and
after 50,000 steps, with a non-empty set.

New probes (`run3.sh`, `probes3.inc` in `.scratch/xapiand/review-group-heal-durability/`):
job 261123-125154-78bf1e (R5, R6, R7; exit 0) and 261123-125156-b3a6de (falsepos).

- F7, closed. make_pin now reads each pinned segment whole (`checked_segment`: footer, size, then
  `verify_all` on every section) and takes the CRC32C from those bytes (install.hh:150-166, :360).
  So body rot behind a valid footer, present when the pin is made, is the source's damage ("the
  pin met damage"; B1 rested and unread, test `body`). Rot after the pin: the learner's stage
  fails the pin's CRC32C, or `check_stage` refuses the staged checkpoint (group.hh:1430). Either
  way it answers validation_failed, which consensus 59ae07f releases as `Unavailable`
  (core.h:829; Rejected is only a SnapshotReply refusal, core.h:1374). Every `Unavailable`
  release now re-reads the pin against its CRC32Cs on the chore (group.hh:511, :1284-1300;
  install.hh:376-389). That covers a learner refusing mid-stream and rot that starts after the
  flight did: test `rot`, mutant refusal-rejected b51e5e. Rot after the learner has already
  fetched that file is not the learner's damage. The source then meets it at its next pin or
  read, which is enough. An activation whose `ix.install` throws damage now marks, sweeps and
  answers `activated` damaged (install.hh:643-652, group.hh:1449-1468), and the wedge is gone:
  mutant install-damage-escapes 1cf54a.
  The false-positive question, from code: a pinned file changes only by rot. Segments and the
  holder manifest log are kept while the generation is pinned. A log is append-only up to its
  pinned length, and a rollover writes a new name (index.hh:5569-5600). Set files keep their
  bundle-length prefix (group-install.md:34-36; retired files stop unlinking below the lowest pin,
  group.hh:1406-1408). `open_if` maps only ENOENT to "missing" (install.hh:95-102), and an EIO or
  EMFILE is not `damage()` (repair.hh:43-55), so a transient I/O error marks nothing. Job
  261123-125156-b3a6de instruments check_refused_pin and runs every simulator row and the group
  tests with no source damage injected. It was still queued behind two exclusive holds when I
  wrote this (see Not run). The sim rows and `two-damaged` pass, but they would not show a false
  positive: a sound source wrongly marked damaged reinstalls and the row still settles. So the
  false-positive answer rests on the code above, not on a run.
- F8, closed. hand_leadership_away no longer reads a host view. Each try goes to the next voter
  in the sorted ring of `MemberRole::Voter` members (witnesses are `MemberRole::Witness`,
  consensus types.h:73), one step further per try (`handoffs_`, group.hh:1596-1609). So every
  host behaves as the test world does. Each damaged leader moves its target on by one per try.
  When leadership only passes among damaged voters, one of them leads infinitely often and so
  names every other voter, the sound one included. B3 view off heals on both seeds (a54cf2), test
  `two-damaged` and `handoff-down` pass (3839f5), and mutants handoff-first and handoff-fixed are
  killed. R6, five voters with 1, 2 and 3 damaged (the ring's +1 hops cross damaged voters) and
  damaged node 1 leading, heals on seeds 1-3 (`1 0 2 0 3 0 4`) with equal digests (78bf1e).
- F9, closed. open_index marks the replica, then renames each strata name to `iq.<name>`
  (repair.hh:90-104). A name already set aside keeps its first copy. The aside names go only in
  `adopt` once the replica is unmarked (group.hh:1178): at recovery, or after an activation
  cleared the marker (group.hh:1456). B4 and test `lone` keep the lone voter's bytes. R5 cuts power
  after each of the 8 directory mutations of the lone voter's damaged reopen (9 cuts). After the
  cut and after the reboot, both strata files are byte-equal under their names or `iq.`, and the
  replica reboots damaged and marked. R7 damages the manifest log on every replica of three (no
  source; seeds 1, 2). Each keeps its 2 strata files whole and stays damaged through 100,000
  steps, then a power cut of all three and 100,000 more. The repair crash sweeps' `staging_left`
  now counts `iq.` (repair_test.cc:66), so a repaired replica keeps no aside name (3839f5).
- F12, closed. `simulation_group_corruption` runs (sim/CMakeLists.txt:59-60, no skip code). It
  damages a stopped follower's segment footer, every manifest log or a sealed set file, 3 seeds
  each, between two stretches of the faults row's traffic and faults with no host view. Then
  `finish` waits for every damaged replica to reinstall before it settles (sim_test.cc:1054-1101,
  :662). Those are A3's three classes and the ones A5 names. It passed in ccf2c8, and marker-kept
  fails it (fdca6d). The settle change hides no stall. `isolated` is a test fault, and the sim's own
  step clears it at `isolated_until` (sim_test.cc:584). `world::settle` drives world steps, which
  never advance that clock, so an isolation left by a cut upload could never end inside a settle.
  Clearing it there is the sim's own heal, as booting the down nodes already was (world.hh:438-443).
  The check that matters, "every damaged replica reinstalls", runs before the settle, under the
  sim's own steps. No group test isolates a node and then calls settle expecting it to stay cut
  off (each clears `isolated` first: voters_test.cc:292, :332, :384, :412, :830;
  repair_test.cc:346). settle_heals and mutant settle-keeps-isolation (42b792) pin the new
  behavior.

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F7 | no | defect | Closed: the pin checks each segment whole; every Unavailable release re-reads the pin; a refused activation answers damaged. | install.hh:150-166, :360, :376-389, :643-652; group.hh:511, :1284-1300, :1430; a54cf2 (B1), 3839f5 (`body`, `rot`), b51e5e, 1cf54a; false positives: code only, b3a6de queued | - |
| F8 | no | defect | Closed: hand-offs go round the voter ring with no host view. | group.hh:1596-1609; a54cf2 (B3 view off), 78bf1e (R6, five voters), 94c61e, 4f156c | - |
| F9 | no | defect | Closed: a damaged index's names are set aside under `iq.` and go only once the replica is unmarked. | repair.hh:90-115; group.hh:206, :1178; a54cf2 (B4), 78bf1e (R5 9 cuts, R7), ccaf0c, 100baf | - |
| F12 | no | gap | Closed: the corruption row runs on segment, manifest and set files and waits for every reinstall; settle's heal masks no stall. | sim/CMakeLists.txt:59-60; sim_test.cc:584, :662, :1054-1101; world.hh:438-443; ccf2c8, fdca6d, 42b792 | - |
| F13 | no | gap | Every `Unavailable` release re-reads the whole pin on the chore, and so stalls apply for the read of the pinned index. That includes a learner crash, a lost link, a learner short of space, and each retry of a flight that keeps failing. A learner failing over and over costs the leader one full read of its index per flight. It is correct, but the cost scales with index size, not with the failure. | group.hh:511, :1284-1300; install.hh:376-389 | Re-check a pin once per pin (a flag on `pinned`, set at its first check), or only when the learner reports a CRC32C or check failure. That needs a refusal reason from consensus, which is the owner's call. |
| F14 | no | gap | Pre-existing, outside this range: the stalled leader's hand-off (D3) still runs only with a host view (`r == step::stall && loop_->applied_past`, group.hh:857). It is the gap F8 had, on the stall path, and no production host sets `applied_past`. | group.hh:857, :114 | The same ring, or drop the view from D3 as well, in a later quest. |
| F15 | no | gap | With every replica damaged (R7), damaged leaders hand leadership round the ring forever. The bytes are kept (F9), but the group churns leadership with no end and nothing tells the host it needs an operator or a restore. | 78bf1e (R7) | A host-visible state ("no sound replica") for placement or an operator, in the placement quest. |
| F16 | no | nit | Two copies each of `open_if` (install.hh:95, repair.hh:31) and `error_text` (install.hh:167, group.hh:1474). | file:line | One of each. |

Not run (re-check):

- CI at strata:870ac9f5 is red on every platform: run 45253641038 fails to compile (`no member
  named 'reinstall' in Driver`, group.hh:1586) against the consensus pin in CMakeLists.txt. That is
  the pending consensus-request-install, not this range. No ASan, TSan or platform result covers
  these fixes yet.
- R5 cuts only the directory disk and only the manifest class. The segment class at open (eager
  segments) and the host disk were not cut on a lone voter.
- F13's cost is not measured.
- Job 261123-125156-b3a6de (`run3.sh falsepos`: check_refused_pin instrumented, every sim row and
  the group tests; any `RVW-PIN-DAMAGE` line is a sound source marked damaged) was queued behind
  the exclusive jobs 261123-005923-f76b83 and 261123-060708-4a7840. It is exit 0 when no false
  positive occurs; the lead can read its result when it runs.
