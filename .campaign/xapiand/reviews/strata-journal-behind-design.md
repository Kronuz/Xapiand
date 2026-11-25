# strata-journal-behind review: design

Commit: strata:786f710226e861a94b3442506063851d1f32f937 (docs/design/journal-binding.md)
Verdict: CHANGES
Date: 2026-11-25

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The binding is right: (P_H, T_H) checked before Start is the correct identity and the correct
place, D4 and D6 follow from it, and the recommendations on Q1-Q3 hold. One part is not yet a
design: D5's leader-pin table omits that consensus refuses a checkpoint past a flight's bundle
while that flight is Sending (core.h:867). The pinning checkpoint starts at the very
SnapshotNeeded whose flight then Sends and fails, so it often publishes into that refusal. As
written it is not retried. At stable timings the learner then never installs, and each lost round
costs a strata commit, a full make_pin read and a release (D-1). The rest are one text round of
notes.

## Evidence I relied on

- The durability review's probes at strata:31821753 establish F1 and F2 (261125-011917-b11e3f,
  261125-011919-316e27, 261125-011921-926bd8, 261125-011923-0ee55a); I did not rerun them.
  786f7102 adds only the design to 31821753 (`git diff --stat`).
- Code read at strata:786f7102 (group.hh, install.hh, index.hh), consensus:6d710cb, strata's pin
  (core.h, driver.h, worker.h), and journal:9c7e31c (journal.h:585-630). Xapiand's shard group
  at journal-replaces-wal 7b57516.

## What I checked myself

Probes under the worktree's `.scratch/xapiand/review-journal-binding/` (`probe.sh` has the submit
lines):

- 261125-021025-991b02, `busy_probe.cc` (the previous review's world harness, the review-sjb
  slot's build). The setup has 3 voters, a bundle at 7 everywhere, 5 writes past it, and a
  learner added. Every fetch is held, so the flight at 7 stays Sending with the learner fetching
  `7/pin`. The leader's checkpoint (through 13) is **rejected Busy after strata committed**:
  generation 4 to 5, latest bundle 7 to 7. Seeds 77, 3 and 11 all give this. Once the fetches are
  answered, the install completes on today's pin, which is at the head. The probe exits 1 by
  design.
- 261125-021119-1b8f9a, `d5_flight_model.py`: D5's flight cycle against that rule, by timing.
  With fixed timings, D5 as written never installs at 21 of 54 timings. Each is a timing where
  the pinning checkpoint reaches Core inside the failing flight's Sending window, or recurs there.
  Retrying the refused publication after the flight's release installs within 2-3 flights at
  every timing; a pin on every published bundle installs at the first. With jitter (each
  duration x U(0.5, 1.5)), D5 as written averages up to 19.4 flights, and 0.5% of learners do
  not install within 100. The job exits 1 by design. (The scripts' comments cite core.h:864-866
  and :1508; the lines are 867 and 1518.)

Answers to the lead's questions:

1. **Identity.** I found no row where holds(H) passes over a wrong history or refuses a sound
   pair while consensus's stable storage holds:
   - Reused terms need a lost hard state that still holds H, which is Q3. Mixing an index from
     a run abandoned by a full rollback with the journal of the run that replaced it also reuses
     terms. The gate removes the refused open's own reuse (F2).
   - A NoOp at P_H: strata's head reaches one only through capture_work's alignment, which
     carries its term (group.hh:1087-1092).
   - A same-term rewrite: Core throws on one (core.h:1298) and on a conflict below its commit
     (core.h:1308).
   - Compaction past P_H: the base never passes a sound head, because strata commits
     (group.hh:1127) before the checkpoint publishes (:1132). Plain mode on macOS is the
     exception (D-6).
   - Closed records match exactly (core.h:320-327). A witness checks nothing, as today.

   "Index older" is unsound to replay, but a replicated member can repair it in place (D-4).
2. **Gate.** A gate before Start makes the refusal stable: nothing that changes the consensus
   state (hard state, log) runs before it. Three journal writes do precede it, and each leaves
   the state equal (D-7). The over_headroom fence (driver.h:803) shows a smaller API (D-8).
3. **D5.** D5 removes F1: the learner's head is the base it installs, and D2 needs no install
   exception. Its leader-pin flow has the liveness hole above (D-1), and its cost rises with the
   number of pins (D-2). It separates cleanly as step 1 once D-1 is in it: it needs neither
   contract and touches only the checkpoint, pin and check_stage paths.
4. **D6.** D6 is sound and necessary for voters, which lose votes and acknowledgments. It is not
   necessary for learners, nor for "index older" (D-4, D-5). The remedy exists only as
   consensus's ChangeMembership: strata::group and Xapiand have no replace operation, and
   Xapiand's shard groups are lone voters today.
5. **Q1-Q3.** I agree with all three recommendations:
   - Q1 (a): the state repeats exactly. C1 must compare state, not bytes, for a closed group
     (D-7).
   - Q2 (a): (b) cannot tell a re-added incarnation from a same-incarnation journal loss, and
     reinstalling the latter rejoins with lost votes.
   - Q3 (a): the plain-mode row should defer to it (D-6).
6. **Checks.** C1, C3 and C4 test values, and their mutants die. C2 misses D-1's ordering. C5 has
   no threshold and no install (D-2). check_stage's head check has no case (D-3). C1's mutant
   needs a forced ordering (D-7).

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| D-1 | yes | defect | **D5's leader-pin flow omits consensus's Busy refusal, and the omission can stop installs.** The pinning checkpoint starts at SnapshotNeeded. That flight then Sends the image, and the learner fails at the missing pin. Core refuses any checkpoint past a Sending flight's bundle (core.h:867), so a pinning checkpoint that reaches Core inside that window is refused after strata committed, and with it any pin made in the item. The table says the flight "retries at the new bundle", but no new bundle exists. Nothing retries the publication, so the next SnapshotNeeded is again at K without a pin. "While a peer awaits" is also undefined between flights, because `needed` clears at each release (group.hh:506). Each lost round costs a commit, make_pin's read of every segment (install.hh:360) and a release. | 261125-021025-991b02 (refused Busy, generation 4 to 5, bundle 7 to 7, 3 seeds); 261125-021119-1b8f9a (never at 21 of 54 fixed timings; retry 2-3 flights, always-pinned 1) | Options, recommending (a). **(a)** Track a peer that awaits an install across flights, until it installs or leaves. Publish the pinning checkpoint only when no flight is Sending, retrying Busy until it is accepted or superseded. Pin only in a checkpoint whose publication can be accepted. Add the Busy row to the table. **(b)** Keep every published bundle pinned: the fused item always pins, and the previous pin goes once superseded and free. This removes the failed flight, the pinning checkpoint and the extra rpc_timeout, at a pin per checkpoint (make_pin's full read unless it reuses verified footers); C5 decides. Test: a C2 case that holds the learner's pin fetch until the leader's pinning checkpoint reaches Core (busy_probe's order) installs within 2 flights; mutant `no-busy-retry` (as written) fails it. |
| D-2 | no | gap | C5 runs without an install and has no threshold. Under D5, every checkpoint while a peer awaits pins: make_pin reads and CRCs every segment (install.hh:350-360) on the chore, which applies wait behind. A checkpoint during a Sending flight commits and is then refused (busy probe). The design's cost line counts one more commit per pin. | install.hh:360; 261125-021025-991b02 | C5 adds a learner installing during periodic checkpoints and states the p50/p99 regression it accepts, fused against split. |
| D-3 | no | gap | check_stage's head check (D5's last bullet; its term in step 3) has no fails-without case. C2's mutant `pin-at-head` breaks the leader, not the learner's check. | journal-binding.md Checks | A learner case whose source pins past its bundle: validation fails, nothing persists, and a correct source's next flight installs. The mutant drops the check. |
| D-4 | no | nit | D6's premise ("lost entries it held") is false for "index older": a base past P_H means the index lost state, while the journal's votes and acknowledgments are intact. This happens when an index directory is lost or restored, or when a plain-mode cut drops strata's last commit and keeps the journal's checkpoint. A replicated member there can set its index aside and reinstall in place (repair.hh), as a damaged one does; a lone voter cannot. This widens a repair and narrows a refusal: the owner's call. | group.hh:1127-1132; repair.hh | Options: keep the one refusal (today's behavior), or repair a replicated member's "index older" in place. Either way, state it in D6. |
| D-5 | no | gap | D6's remedy cost. A learner neither counts toward commits (core.h:1631) nor campaigns (core.h:1468), and candidates ignore its grants, so refusing it is not needed for safety; a reinstall in place would do (owner's call). Remove-and-add has no operation in strata::group or Xapiand (shard_group.cc:522, `voters = {1}`, at journal-replaces-wal 7b57516). With millions of groups per host, one bad host event refuses many groups, each waiting on a manual membership change. | core.h:1468, 1631; Xapiand shard_group.cc:522 | Name the remedy in the refusal text and in group-open.md. The cluster quest owns the replace operation. Decide refuse versus reinstall for learners. |
| D-6 | no | nit | Three corrections to the States table. (a) The "stage complete" row's "old H below B" is false for a requested reinstall, since core.h:767 admits B at or below commit when asked; this is harmless because case 1 binds the bundle. (b) In the plain-mode row, "any other outcome is the crash row" is false for a replicated member that lost a vote or acknowledgments above P_H; that is Q3. (c) One row is missing: a plain-mode cut that drops strata's commit and keeps the journal's checkpoint is refused as index older, and a lone voter then has no remedy. | core.h:767; group.hh:1127-1132 | Fix (a) and (b), and add (c). The durability docs say plain mode on macOS voids consensus's stable-storage assumption for replicated groups. |
| D-7 | no | nit | D7 and C1: besides a closed group's reopen (journal.h:604-610), two journal writes precede the gate. One is a bootstrap-only group's typed initialization (worker.h:178; ready() at :183 waits for it). The other is the pre-Start reclaim of a pressured store (worker.h:502). Both keep the recovered state. C1's mutant `check-after-start` dies only if the lone voter's election persists before the check runs. | worker.h:178, 183, 502 | D7 lists both writes. C1 and C3 always compare the recovered state (base, last, terms, hard state), and bytes only for a turn-log group without pressure. C1 forces the election-first ordering, as 926bd8's harness did. |
| D-8 | no | nit | consensus:recover-gate can be smaller as a value. recover takes the boundaries the open accepts: H, plus a complete stage's bundle boundary, or none when the replica is damaged. All of these are known before recover. Consensus fences before Start when the log holds none of them, with retains_suffix as the single predicate (core.h:659-661), on the path over_headroom already takes (driver.h:803). A fenced Worker writes no close record (worker.h:689). This needs no GroupState.base, no term_at and no callback. The design's callback form also works. | driver.h:803; worker.h:689; core.h:659-661 | Owner's call. Consensus's tests stay the same. |

## Not run

- No probe of D5 built: the busy probe shows the Busy refusal on today's code, and the model
  stands for D5's timing. The Busy interleaving in group tests (C2 under D5) is unproven either
  way.
- Whether journal's next recover reads a refused closed group's staged checkpoint record back
  unchanged (D7, Q1): read at journal.h:604-610, not run.
- The cost of make_pin, or of the fused item (C5's subject).
- strata:applied-term's manifest encoding (rotation snapshot, install adoption, pre_freeze).
- Linux, FreeBSD, TSan, Debug: the busy probe ran in a macOS Release build.
