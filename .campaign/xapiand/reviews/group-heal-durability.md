# group-heal review: durability

Commit: strata:bea1f5687823bdcb6a35d51750bff6f233945b09
Verdict: CHANGES
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
