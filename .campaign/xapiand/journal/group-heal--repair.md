# group-heal--repair: journal

State: waiting on jobs 261123-061126-fa0f89 (group_repair, group_repair_eager at strata:c7f776ac); after it 261123-061134-aacfd8 (^group_) and 14 mutants 261123-061136-9410bb .. 261123-061205-59dc8e (pin-unchecked-footer 261123-061203-9d2526); pushed
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
| A3 | pending: 261123-061126-fa0f89, 261123-061134-aacfd8 (at c7f776ac; 053709 failed at 1d2c820f); fails without: the 13 mutants above (first build 261123-045314-e451c4 failed, fixed at 1d2c820f) |

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
