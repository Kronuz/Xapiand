# group-heal--repair: journal

State: waiting on jobs 261123-063129-a135ff (group_repair, group_repair_eager at strata:d457da76); after it 261123-063139-f5dbcc (^group_) and 17 mutants 261123-063143-84cfc5 .. 261123-063248-279216
Quest: .campaign/xapiand/quests/group-heal.md (the lead's; never edit it)

## Doing

strata:1d2c820f, against consensus:59ae07f (`--ref consensus=/Users/kronuz/code/Kronuz/consensus@59ae07f`):

- First build 261123-045314-e451c4 (at dc14923f) failed: `[segment, seed 1]` did not reopen
  damaged, and `corrupt_segment: footer checksum` escaped from the test's `same_digests`. Cause:
  strata maps a segment at its first read (cold-tier D2, `eager_segments` off); the open checks
  names and sizes only (D11), so a flipped footer is met at the first read, apply or merge.
  Fix (test and docs; ruling below): the segment case reads every key after the reopen (the read
  answers replica_damaged and marks the replica), ctest `group_repair_eager` runs it under
  `STRATA_EAGER_SEGMENTS=1` (found at open, quarantined); `leader` and `crash` damage a manifest
  log, which the open reads.
- Review F3 (group-heal-durability): `sources::needed` is now peer -> its flight's transfer token;
  every SnapshotReleased of that flight erases it (one release per flight, whatever the reason;
  a retry is a new SnapshotNeeded). Test `deposed` (leader deposed mid-flight names no install;
  none once settled); mutant `needed-kept`.

Jobs: 261123-053709-bfdcc2 (`^group_repair`, task), 261123-053717-611002 (`^group_`, task, after
it), 13 plain `--fails-without` mutants after it: open-rethrows 0f397b, unmarked 889a22,
quarantine-first 989d11, set-damage-fences beb2bb, no-ack e73634, waiters-unanswered 209793,
apply-damage-fences 2e878f, read-damage-passes 46e4b4, activate-unchecked 92b4fc, marker-kept
b32ccb, leader-keeps d4d5f4, no-retry 135fbd, needed-kept c068cc (all 261123-0537xx/0538xx).

Round 2 (job 261123-053709-bfdcc2 at 1d2c820f failed): consensus answers reads NotLeader on a
follower, so the lazy `segment` read and the `read` case got "rejected (reason 1)"; and settle()
returned before any reinstall (a damaged replica at its leader's base matches applied indexes),
so `group_repair_eager` never saw the repair. strata:c7f776ac: `lead()` moves leadership to the
replica before its reads; `healed()` steps until no replica is damaged before every settle in
`repaired()` and the crash sweep; digest reads that meet damage fail a check, not the process.
Mutants (c7f776ac): open-rethrows 9410bb, unmarked a0957d, quarantine-first 62952e,
set-damage-fences 649be6, no-ack 2ba6bf, waiters-unanswered ef2479, apply-damage-fences 596caa,
read-damage-passes ccda65, activate-unchecked d1c5a1, marker-kept 0dffe6, leader-keeps 2746ad,
no-retry 92542a, pin-unchecked-footer 9d2526, needed-kept 59dc8e (all 261123-0611xx/0612xx).

Round 3 (job 261123-061126-fa0f89 at c7f776ac: eager passed, group_repair failed refusal,
staged, source and four crash points). strata:d457da76:
- refusal: the test measured the wrong index. The replica does acknowledge delivered ranges
  unapplied (consensus applied 32 over the replica's 31, the damaging write's range). But once
  the leader admits its install request (consensus core.h replicate: a `requested` peer gets the
  image path only) and while the request is pending (the follower delivers nothing), x's log no
  longer counts toward commits, so with y cut off the later writes cannot commit. The test now
  writes until the first that cannot commit, checks applied >= the damaging write's index, and
  every acked write survives.
- staged: nothing escaped; the refusal happened but the reason stayed the marker's first text
  (mark_damaged keeps an existing marker). adopt passes the refused file's reason to
  adopt_damaged. Mutant reason-kept.
- source: nothing escaped either: strata's lazy map inside make_pin (its checkpoint's commit)
  met the damage, and become_damaged marked the source. The failure was the hand-off:
  applied_past(id, 0) named the other damaged replica, and two damaged replicas passed
  leadership between them (no leader at the end). hand_leadership_away now asks for a follower
  applied through the commit index, and waits when none is named. The pin tags its reason ("the
  pin met damage"). Mutant handoff-any. pin-unchecked-footer may survive if strata's own
  checkpoint always meets the damage first; its job says.
- crash at 89/83 of 91/85: the cut fell after clear_damaged, before the activation was counted
  in either process: the replica was repaired. The check is now unmarked and the damage gone
  (a manifest log again, or the sealed file's CRC32C).
- crash at 53/52: a complete stage stayed after a settled repair. Hypothesis: the leader's owed
  image (requested survives TimedOut) reached the repaired replica, which staged and validated it,
  and consensus refused it CaughtUp (decided at InstallPrepared, after validation; no Sink notice).
  Fix: a sound replica answers validated without staging an image at or below its applied
  index. Mutant stage-caught-up; the failure messages print the stage's position if it stays.

## Next

1. Red case or survived mutant: fix next commit, rerun.
2. Push the branch (CI red until the consensus pin moves to 59ae07f+, the lead's call).

## Blocked on

Nothing. The lead unblocked the task (file set widened to option (a), design approved).

## Design (approved by the lead)

Damage (strata `corrupt_index`/`corrupt_segment`, the set's `file_set_damage`) at open, recovery,
apply or a read writes the durable marker `rd` first, quarantines a damaged index for a deferred
one, fences the group locally (reads, proposals, checkpoints answer `replica_damaged`; delivered
ranges acknowledged unapplied; no pin or serve), and calls `Driver::reinstall` on the loop, again
each election timeout while damaged; a damaged leader hands off first. The image stages and
activates through install's path; activation now checks every staged file's CRC32C, and a
complete activation removes `rd` before its sweep. Files: include/strata/group/{repair.hh,
group.hh,install.hh}, test/group/repair_{test.cc,suite.sh,mutants.py}, CMakeLists.txt (target),
docs/mechanisms/group-install.md (Repair).

## Evidence

| Criterion | Evidence |
|---|---|
| A3 | pending: 261123-063129-a135ff, 261123-063139-f5dbcc (at d457da76; fa0f89 failed at c7f776ac, 053709 at 1d2c820f); fails without: 17 mutants 261123-063143-84cfc5 .. 261123-063248-279216 |

## Result

Not yet.

## Rulings I made

- Unfencing happens when the activation's recovery from the bundle succeeds (`adopt`), which is
  the moment consensus completes the call with RepairCompleted; an unrequested image that
  activates (a lagging replica) repairs too. The marker is the damage, not the call.
- A staged file that fails its check at activation marks the replica damaged and still answers
  `activated`: `activation_failed` fences consensus's Core for good, while consensus holding the
  image as its base is harmless (the replica serves nothing and asks again).
- A live set file has no CRC32C in the bundle, so its damage at rest is not detected (F24, no
  scrubber); the staged check covers live files through the pin's CRC32C.
- The repair retry shares the stall hand-off's timer (one wheel entry per group).
- A damaged leader hands off only to a follower the host names as applied through the commit
  index; none named waits for the timer. Only a host with no view falls back to any voter.
- A sound replica answers `validated` at once, staging nothing, for an image at or below its
  applied index: consensus can only refuse it CaughtUp (asked_by needs a pending request, which
  only a damaged replica makes, and reinstall is Busy while an image is in flight).
- The test's `read` and `apply` cases inject strata's `corrupt_index` through the read function
  and the app's `before_write` hook: deterministic, and the same error strata raises.
- A segment's damage is found where strata maps it: at the first read, apply or merge
  (cold-tier D2 default), or at open under `eager_segments`. The group does not force eager maps
  (per-open cost across many indexes); reads fail closed through read_applied's guard.
- F3: `installs_needed()` names a peer while a flight to it is in progress, not "until its
  install ends": after a Rejected or TimedOut no flight exists until consensus starts another,
  and a leader deposed in that gap gets no LeadershipLost for it, so the review's keep-on-retry
  rule would still leak.
- Pinned segments (lead's msg, strata:3eb092f9): make_pin checks each segment's footer
  (checksum, magic, the manifest's `footer_crc`) before listing it; a mismatch makes pin_for call
  become_damaged (marker, local fence, reinstall; a leader hands off), else fences. Case `source`,
  mutant `pin-unchecked-footer`. Damage inside a section body with a valid footer is still caught
  only at the learner's activation (strata's whole-segment check), which fences that learner.
