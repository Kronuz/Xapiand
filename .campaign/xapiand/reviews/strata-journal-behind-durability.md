# strata-journal-behind review: durability

Commit: strata:318217532180d9c9ecbdce24a429f6c93a392b65
Verdict: CHANGES
Date: 2026-11-25

APPROVE: lands as is, at this commit's code, with no blocking finding. CHANGES: lands once each
blocking finding's fix and its test pass the suite at the fixed commit; with only nits, an APPROVE.
REJECT: the approach is wrong; the quest is re-planned.

The check is in the right place for strata's files (nothing in the index directory changes before
it), its tests check values and its mutant dies at the refusal. The bound is wrong in two ways:
an install legitimately leaves P_H past E, so sound replicas are refused (F1, the build-test
failure); and the refused open lets a lone voter elect and append to the refused journal, so the
refusal erodes with each retry until the open passes and corrupts (F2). F2's fix likely needs a
consensus contract (a gate before Start); that is the lead's call to plan.

## Evidence I relied on

- 261125-004551-0bc60c (A1 test): clean-stop, crash, missing and sound pass (4 of 4).
- 261125-004553-aab292 (mutant journal-behind-unchecked, `--fails-without`): killed at the
  refusal's own assertions ("the open refuses", P_H and E in the message, byte-equal), then the
  replica reads damaged. The mutant removes only the new check.
- 261125-004550-50832e (build-test, A2): failed, simulation_group_corruption only (142 of 143
  pass). Root-caused below (F1).

## What I checked myself

Probes under the worktree's `.scratch/xapiand/review-strata-journal-behind/` (`probe.sh` names
each case and its submit line; `build_probe.sh` compiles a probe with a group test's flags):
build 261125-011910-39eaa9 (slot review-sjb), then:

- 261125-011917-b11e3f, `install_restart_probe.cc`: 3 voters, 6 writes and a checkpoint at
  every replica (bundle 7), then 5 more writes; a joiner added as a learner installs. At
  activation: installed at 7, P_H 12, E 7. Restarted at once by kill, power cut or clean close
  (with its closed record), it is refused "P_H = 12 ... E = 7" all three ways. With 0 writes past
  the bundle (P_H 7) it opens.
- 261125-011919-316e27, the same at the merge base's group.hh (539fa10): all six open, the
  cluster settles and every digest equals the replay of the merged log. The refused replica is sound.
- 261125-011923-0ee55a, `sim_corruption_probe.cc` with `trace_overlay.py` (stderr traces at each
  recovery and install activation): simulation_group_corruption [segment, seed 3]. Node 2's
  earlier process shows "install activated: bundle 2, strata P_H 3". Its restart shows "recover:
  P_b 2, P_H 3, E 2", refused twice: the build-test failure.
- 261125-011921-926bd8, `erosion_probe.cc` (journal_behind_test's harness): a lone voter's
  running journal is copied, then P_H is one or two entries past it, and the same journal is
  opened again and again (results in F2).

Answers to the lead's questions:

1. The check runs first in `adopt` (group.hh:1173), before `resume` (:1179), the quarantine's
   release (:1187) and the file set's recover (:1191). The index directory is byte-equal after
   every refused attempt (A1 test; erosion probe). The journal is not untouched: a lone voter's
   refused open persists a new term and a NoOp before its row closes (F2).
2. E is read after `driver.recover`/`create` returns. That is at Start (driver.h:891-897),
   after the Worker recovered the log. In the harness the read excludes the election's NoOp:
   the refusal names the copy's end. Strata applies only through the durable commit index
   (core.h:1640-1644), so neither of the lead's hypotheses holds: no follower apply runs ahead
   of its log, and `last` is the recovered end. `last` is still the wrong bound after an install
   (F1).
3. Skipping the check at activation is right, but the activation leaves P_H = P_G > E = B. The
   next recovery refuses that (F1).
4. A first open creates over a deferred index (P_H 0) and passes. A node rebuilt by an install
   passes its create, then is refused at its next restart until its log reaches P_G (F1).
5. Values, yes: positions in the message come from the harness, and every file's bytes are
   compared. The mutant dies at the named check. Gaps are F2's (no test reopens a refused
   journal, and the journal is never compared) and N1-N2.

## Findings

| ID | Blocking | Kind | Finding | Evidence | Fix |
|---|---|---|---|---|---|
| F1 | yes | defect | **A sound replica is refused after an install whose pin is past its bundle.** `make_pin` pins `ix.checkpoint()` (install.hh:344): strata's commit at the leader's head when the flight starts, at or past the bundle B (group-install.md says so). Activation adopts that generation's applied position P_G (index.hh:1440-1450). consensus's install clears the learner's log to base B (core.h:733). Until the learner's log durably reaches P_G, every restart recovers P_H = P_G > E = B, and `adopt` refuses (group.hh:1173). This is the build-test failure. Also derived from code, not run: a crash after strata's install of G and before the set's files move refuses before `resume` can finish. There P_H is P_G, not B as the journal's lifecycle table says. | 261125-011917-b11e3f (refused 12/7 on kill, power cut, close); control 261125-011919-316e27 (merge base opens, digests equal); 261125-011923-0ee55a (sim trace); 261125-004550-50832e | Options, recommending (a). **(a)** Persist the last install's boundary B and adopted head P_G with the index at activation (an index attribute written by strata's install, or beside the stage marker). Refuse only when P_H > E and not (P_b >= B and P_H <= P_G): the gap (E, P_G] is then committed history the leader supplies. A journal from before the install (P_b < B), or an index that applied past G from its own log (P_H > P_G), still refuses. **(b)** Pin exactly at the bundle: the leader pins the generation its checkpoint committed at P_b, with no apply between capture and commit (or strata commits the capture's view). An install then leaves P_H = B <= E and the invariant holds with no exception. The cost is a chore stall during the checkpoint's syncs, or a strata feature. **(c)** Refuse only lone voters (rf = 1). Not recommended alone: a member whose journal went back also rolled back its votes and log. Its vote can elect a candidate missing committed entries, and strata would then skip the replacements at or below P_H (S1 on a replicated group). Test: the probe's three restarts as a group test (fails at 31821753), the half-activated crash, and simulation_group_corruption green. |
| F2 | yes | defect | **The refusal erodes: each refused open of a lone voter appends a NoOp to the refused journal. After P_H - E attempts the open passes and corrupts.** `open()` reads E after `driver.recover`, which returns at Start, and a lone voter's election is due at once (core.h:706-710). Its campaign and NoOp (core.h:1530-1549) persist while the recovery item waits for the chore, before the failed open closes the row (group.hh:257-295). Each later open recovers one more entry. With P_H = E + 1, the 2nd open passes and key c serves e's bytes ("ee"). With P_H = E + 2: refused at E 3, then at E 4, the 3rd open passes and every read fails "file-set read past the end of vol-0"; the index directory changed. That is S1's corruption, reached by retrying an open, which a host retrying requests does. "Journal missing" erodes the same way (the created group recovers on the next attempt). In production this needs the recovery item to wait behind two journal syncs (a busy chore lane); the harness runs that ordering. The A1 test never reopens a refused journal and never compares the journal. | 261125-011921-926bd8 (`erosion_probe` 1 and 2) | Decide the refusal before the group can start or elect. Either a consensus recover gate (the recovered log end handed to the host before Start; a throw refuses with nothing written: a contract change beside consensus:group-last), or compare terms: persist strata's head term with P_H and refuse unless the log holds P_H at that term (Core::term_at through the Driver). The term check also refuses a log whose entries at or below P_H were replaced. Test: open the refused journal P_H - E + 1 times; each refuses with the same E, and the journal's and the index's files stay byte-equal. |
| N1 | no | gap | `missing` reads E from the message under test (journal_behind_test.cc:316-320) and asserts only E < P_H. It should assert E's value (the created log's end, 0 in the local run). | journal_behind_test.cc:316 | Assert E == the created log's end. |
| N2 | no | gap | The sound case is a lone voter only; no follower, learner or install path recovers in the new test. F1's test covers it. | journal_behind_test.cc:325-350 | F1's test. |
| N3 | no | nit | group-open.md's sound row ("B once activated") and the journal's lifecycle row ("replaced by the image's B") are wrong: P_G >= B. The doc says E is read "before the group can elect and append": true of the read, but the election runs before the check (F2). | docs/mechanisms/group-open.md | Correct with F1 and F2's fixes. |
| N4 | no | nit | "Journal missing" also refuses a member removed and re-added over a directory its host kept. That is safer than before (the replay skipped new entries); a reinstall would serve better. The owner's call: keep the refusal or quarantine and reinstall. | not run | Owner's call. |

## Not run

- The half-activated crash (strata's install of G committed, the set's files not yet moved) as a
  run: derived from group.hh:1173 preceding :1179.
- F2 under production threads and real fsync timing: shown in the harness's ordering only; whether
  Xapiand's catalog retries a failed open on the next request.
- Linux and FreeBSD; TSan; Debug asserts. Every run was macOS, Release.
- Whether Xapiand keeps a removed member's directory (N4).
- The witness path: `witness.hh` holds no index and calls no `adopt`, so it has no check (read,
  not run).
