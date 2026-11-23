# group-heal review: durability

Commit: strata:908b58c0d2ee3512744c68be8a083daea72715ec
Verdict: CHANGES
Date: 2026-11-23

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

Lens: durability, carrying correctness for the install bytes and the binding. Consensus at
8780e44 (the jobs' ref).

## Evidence I relied on

- Install level, at 75b5ac9b (install.hh has changed by 3 lines since then, release_pins's
  deferred-index return; install_test.cc is unchanged): install_test 261123-005922-93c9ac passed
  (values, retry, held, damaged, threads, crash at every mutating operation from the stage through
  the activating recovery). Its mutants were killed: marker-before-sync 261123-005951-972f5c,
  write-past-failure 261123-005942-72e95b, skip-file-set e11cfa, keep-unchecked a51a18,
  holder-unordered 6cfb33, no-resume d1a515, serve-past-length 4adb58, pin-names-unchecked 9f5825.
  This is enough for install.hh's own crash claim.
- At the head 908b58c0: suite 261123-021413-a25890 passed (catalog, install, voters,
  witness, footprint). Mutants release-fences 261123-021416-01f061, pin-per-flight
  261123-021418-f94527, install-silent 261123-021420-2fb770 and close-skips-install-tasks
  261123-021423-f55c10 were killed (exit 1).
- Witness before the merge, at e4828884: witness 261123-014646-0ebc24 passed, and
  witness-holds-range 261123-014651-e0f143 and witness-refuses-image 261123-014655-73ffe0 were
  killed. witness.hh and witness_test.cc did not change in the merge.
- Not yet run at the head (queued when I wrote this): witness 711463, its mutants 94e042 and
  dfeb44, and CI. I relied on none of them.

## What I checked myself

Probes live in `.scratch/xapiand/review-group-heal-durability/` (probes.inc is inserted into
voters_test.cc in an exported copy of 908b58c0; `run.sh build`, then `run.sh run R1c R1d R1h R2 R3`).

- R2, reopen on a full disk (job 261123-022652-f2e3bc): a leader that served an install keeps the
  latest bundle's pin in strata's manifest (1 pin). It restarts with its directory's disk full,
  and the open fails with "faultfs write: No space left on device". The control arm, the same
  leader with no pin, opens on the same full disk. The difference is recover()'s
  `release_pins`, which commits strata (group.hh:1085, install.hh:306-309). Before this quest,
  recover committed nothing. Finding F1.
- R3, pins across a leader change (job 261123-022652-f2e3bc): after an install the leader holds 1
  pin. The leader is deposed and every node checkpoints twice more, and then every node's manifest
  holds 0 pins, with digests equal to the replay. No pin leaks across leader changes or
  checkpoints. consensus sends SnapshotReleased(LeadershipLost) for each flight on step-down
  (core.h:648, :778), so `flights` empties.
- R1 is a power cut on the learner at every mutation from its rejoin through the settled install
  (job 261123-023620-b636fa, install_gap's world at 3 voters, seed 55). On the host disk (the
  consensus journal), all 28 crash points (k 0-27) settle, with equal digests and no staging name
  left. On the directory disk, 31 of 38 points settle. At k 17-23, the reboot throws
  `corrupt_index: no manifest log` from the host's `open_index`. The control cuts the same
  replica's power while it is still cut off, before any install, and it reopens and settles
  (R1c). Finding F2.
- The ENOSPC release path (cc2d0b3f): `release_superseded` stalls and keeps the pin, and
  release-fences was killed at the head. `release_pins_held` at close swallows every error and
  leaves the pin for the next recover. Under a full disk, that recover is where F1 bites.
- The lazily created install state (82ea744c): `trim_sources` frees it only when `needed` and
  `pins` are both empty. `needed` is erased only on Installed or CaughtUp (group.hh:461-466).
  Finding F3.
- Release while a learner still fetches: a pin goes only when its bundle is superseded and no
  flight holds it. A learner still staging after its flight was released fails its next fetch
  with not-found and answers validation_failed, with its staged bytes kept and no marker. It never
  reads other bytes: segment ids and manifest generations are never reused, a set name is never
  written again (the replica fences on that), and every file is checked by CRC32C.
- Learner pins after an install: strata's install drops the learner's pins (`drop_pins_`), and a
  later `release` of a dropped generation is `std::erase` of an absent value (index.hh:5405),
  which is harmless.
- Risk 3, the changed expectation: this is correct protocol behavior. A removed member never
  learns the commit of its own removal: core.h:468-471 replicates to the committed membership
  until the removal commits, and core.h:1305-1316 then drops the peer. grow_and_heal and the
  witness join now retire the leaving voter only after a leader other than it holds a membership
  without it (world.hh:607). They then settle without it and check every survivor's membership and
  digests. The test is stronger than before: the remaining members commit and settle without the
  removed voter. Nothing is masked. Finding F5 is an optional assertion.
- Risk 4, the merge: the close waits for install tasks (`tasks_` in idle_awaiter and drain,
  group.hh:618-627, :760-763). close_mid_install exercises this, and its mutant f55c10 was killed.
  activate_image skips its answer once the group is torn down, and a reopen finishes the
  activation through adopt's resume, because consensus persisted the install before
  ActivateImage. release_pins_held runs after `driver().close`, so no notice can pin after it.
  Notices queued before it run first. `release_pins` returns on a deferred index (no manifest).
  `serve` counts itself before it checks `closed_` under mu_, so the teardown's wait cannot miss a
  serve that took a pin (Finding F4 covers its notify and its missing test).
- Risk 5, the witness: witness.hh acknowledges ranges without applying them and pins nothing. It
  answers `validated` for an image while it joins: consensus keeps the bundle as its log's base,
  and the witness stores none of the files. The brief says "never accepts an image", but A4 does
  not ask for that, and the witness-refuses-image mutant shows the join needs it. witness_test's
  loss case power-cuts each of the three members at three seeds, and carry covers the witness
  leading only to hand off. Evidence at e4828884 (above). The merge did not touch either file.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | A replica that holds an install pin cannot open on a full disk. recover() calls `release_pins`, which commits strata, so the ENOSPC fails the open. Every leader that served an install keeps its latest bundle's pin by design (75b5ac9b), so the case is common, and a fenced close or crash with a full disk always reaches it. The same replica with no pin opens. This contradicts the quest's own ruling (cc2d0b3f): a release short of space stalls and never fences. | Probe job 261123-022652-f2e3bc (R2: control opens, pin held fails with "No space left on device"); group.hh:1085, install.hh:306-309 | Do not fail the open on a release short of space. Keep the pins and release them from a chore notice that stalls on ENOSPC, as release_superseded does. Test: R2's case in voters_test (a leader holding a pin, restarted on a full disk, opens; once space is freed and resume() runs, the pin is released and the cluster settles). |
| F2 | yes | defect | A crash during the activation of a learner whose strata index is still deferred leaves a replica that cannot open: neither the old replica nor the installed one. `ix.install` writes its segment copies (`seg-1.sx`) and then `manifest-1.tmp`, before any manifest log exists. After a crash in that window, strata's open finds strata names and no log, so it is not `uncreated`, and it throws `corrupt_index` (index.hh:5828-5830). `open_index` catches only `index_not_created` (group.hh:193-199), so the group is never built, and resume(), which would finish the activation from the complete stage (`ip` and `is.*` are still there), never runs. A fresh learner, the grow and replace path, always installs onto a deferred index. At k=17 the directory is the same before and after the cut, so a plain process crash at that point fails the same way. install_test's crash case does not reach this because its learner is always `create`d and checkpointed (install_test.cc:84-86, :398-403), never deferred. | Probe job 261123-023620-b636fa (R1d: k 17-23 throw; listing at k=17: `ia.vol-0 ip is.manifest-1 is.seg-1.sx seg-1.sx(0)`, at k=23 also `manifest-1.tmp`); control R1c settles | Make the deferred index's old state a committed one before it installs (for example, commit the deferred index at the start of activation, so a crash opens the old empty generation and resume() finishes). Alternatively, strata's install on a deferred index could write its copies so that a crash leaves an `uncreated` directory. Tests: install_test's crash case with the learner's index deferred, and R1d's group-level sweep (a power cut at every directory mutation of a fresh learner's install: every point reopens and settles). |
| F3 | no | defect | `installs_needed()` and the lazily made install state outlive the leadership. `needed` is erased only on Installed or CaughtUp, so a leader that loses its leadership (or whose peer is Removed) mid-flight names the peer forever as a follower and never frees `sources_`. That idle group then gives back the 88 B that 82ea744c saved. | group.hh:461-466; consensus core.h:648 sends LeadershipLost per flight | Erase the peer on LeadershipLost, Removed, Fenced and Stale too (keep it on TimedOut, Rejected and Unavailable, which consensus retries). Then trim. A test: depose the leader mid-flight in install_gap and check `installs_needed()` is empty on every node once settled. |
| F4 | no | gap | serve's guard against the teardown (`serving_`) has no test off the loop thread. The ruling "TSan in CI covers the threads" has nothing to cover: world.hh:288 calls serve on the loop, and install_test's `threads` case calls install.hh's serve, not group::serve. Also, serve_scope calls notify_all after its decrement (group.hh, serve_scope). A teardown that sees 0 may return, and the host may destroy the group before notify_all runs on that freed atomic. In practice it is a futex wake on a stale address, which is harmless, but it is still undefined behavior. | group.hh serve(), serve_scope, tear_down; world.hh:288 | Decrement and notify under mu_ with a condition variable, or keep the counter outside the group. Add a test that calls serve from a second thread across close() (TSan). |
| F5 | no | nit | grow_and_heal and witness join retire the removed voter without checking what it holds. | voters_test.cc:650-651, witness_test.cc:187-188 | Optional: before retiring it, assert that the leaving voter applied the removal index minus one and that its applied log is a prefix of the others'. |
| F6 | no | nit | `release_pins` at recover releases every pin in the borrowed index's manifest. Install is the only pinner today (I grepped strata and Xapiand), but the index is the catalog's (D169), so a future pinner (backup, cold tier) would lose its pins at every group open. | install.hh:306-309 | Document that the group owns every strata pin on its index, or record the install's generations so only those are released. |

## Not run

- TSan or ASan: nothing under a sanitizer. CI's linux-asan-ubsan and linux-tsan at 908b58c0 were
  still running.
- witness and its mutants at 908b58c0: queued, not relied on (the e4828884 runs stand for the
  unchanged files).
- A process crash (kill without power loss) sweep at the group level. R1 uses power loss only, and
  install_test's crash case covers both at the install.hh level.
- A3 (repair), A5 (simulator rows) and the simulator port: not in this review's risks.
- Unsynced-tail and bit-rot damage to staged `ia.` files between the stage and the activation:
  activate moves them in without a CRC32C check, and that belongs to A3's fencing.
- The serve-after-destroy contract on the host side (the catalog's fetch routing stopping before it
  destroys a group).
