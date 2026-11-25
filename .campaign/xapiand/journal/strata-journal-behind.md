# strata-journal-behind: journal

State: waiting on jobs 261125-033043-326a1d 261125-033050-247c98 261125-033054-178f63 261125-033057-a58c2b 261125-033101-eb5028 261125-033114-38a2d6 261125-033117-9af144 261125-033120-14f397 261125-033123-d47653 261125-033126-adce3d 261125-033134-b6ba15
Quest: .campaign/xapiand/quests/strata-journal-behind.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Session 3 (2026-11-25). Design review CHANGES at strata:786f7102 (D-1 blocking, D-2..D-8); the lead's
rulings (msg-1, below). Revised docs/design/journal-binding.md at strata:4c5536f7, pushed: D5 tracks
peers awaiting an install across flights, pins each checkpoint while one waits, retries a Busy
publication after the next release (D-1); make_pin and check_stage at the bundle (D-3, C7); D6 split
by refusal, marked owner default (D-4, D-5); D7's three pre-gate writes (D-7); States rows (D-6);
consensus:recover-gate as a value API with recover_refused (D-8); C5 with an install in flight and a
10% p99 threshold (D-2). Building D5 now.

msg-1 (lead, 2026-11-25), answered: the binding (D1-D4, D3's gate) approved; Q1 (a), Q2 (a), Q3 (a).
D-1: retry the pinning checkpoint after the flight's release, not pin every bundle; C2's busy case in
the probe's order; the model 1b8f9a installs at all 54 timings. D-4/D-5 owner default: index older
and a refused learner reinstall in place, journal older or another history on a voter refused and
replaced. D-2 C5 threshold; D-3 a learner check_stage case with a mutant; D-6, D-7 fixed; D-8 the
value API, exact signature (Contracts). Done: all in the doc at strata:4c5536f7; D5 builds here, the
gate and applied-term stay other quests'.

D5 built at strata:e084e32d, pushed (step 1). group.hh: fused checkpoint item (checkpoint_work: capture,
sync, commit or pin), `awaiting` peers across flights (cleared by Installed/CaughtUp/Removed/
LeadershipLost/Fenced, and by forget_awaiting at each checkpoint for a non-leader or a non-member),
pin_wanted, start_pinning/pinning_checkpoint (one at a time, counted as a task), the Busy retry
(releases_ counter, release_awaiter woken by install_released, fence and teardown). install.hh:
list_pin factored out of make_pin; make_pin refuses P_H != P_b; check_stage refuses a staged head
!= the bundle's position (install_failed). Found while making simulation_group_corruption pass: a
replica repaired by a requested reinstall checkpointed with epoch 0 while consensus's epoch had
advanced, so every later checkpoint was refused InvalidCheckpoint (the Driver reports any discarded
capture so); D5 made it fatal once that replica led. Fixed: reinstall_once keeps RepairCompleted's
epoch, the capture names it; dispatch forwards an unawaited RepairCompleted too.

Local (quiet light, macOS Release): group_install_bundle_test restart (6 cases: P_H = B after
activation; 0 writes past: B 7; 5 past: B 13) and busy (3 seeds: installs at 13 after 2 flights held
in the Sending order); group_install_test all cases; simulation_group_corruption ok; group suite 33
of 33 after the fixes below. Mutants killed locally: no-busy-retry (busy: never installs in 6
flights), pin-at-head (restart: never installs), stage-head-unchecked, repair-epoch-dropped (sim
corruption: the cluster never settles).

Jobs at strata:e084e32d: tests 261125-033043-326a1d passed (install-bundle, install, sim corruption, repair,
voters, journal-behind); the rest queued when this session ended.

Model (job adce3d's script .scratch/xapiand/strata-journal-behind/d5_retry_model.py over the
reviewer's d5_flight_model.py): 54 of 54 fixed timings install under the retry, 16000 of 16000
jittered learners. Worst 49 flights at rpc_timeout 40 ms and c = 2000 ms: the flights before the
pinning checkpoint's own duration elapses, not Busy losses (flights ~ c / (rpc_timeout + 1.5 s)).

Session 2 notes follow.
Session 2 notes follow.

Session 2 (2026-11-25), design round after the durability review (CHANGES: F1, F2, N1-N4). The
position check at strata:31821753 is wrong twice: E moves (a refused lone voter elects at Start and
appends a NoOp, so the refusal erodes: F2) and an install lands P_H past E on purpose (F1). Design:
strata docs/design/journal-binding.md at strata:786f7102, pushed. No code this round: it needs a
consensus API (Blocked on 2), so the assignment stops at the design.

The model. The index's head is an identity, H = (P_H, T_H): the index and term of the entry strata
applied last, kept in strata's manifest with the applied position. The pair is sound iff the
recovered log holds H (its base boundary, or an entry at P_H of term T_H; consensus's own
`retains_suffix`): by log matching that says the journal's history through P_H is the one the index
applied. One comparison covers index older (P_H below the base), journal older (P_H past last) and
another history (another term at P_H). It runs in a consensus gate before Start, so a refused
journal never elects, votes or appends: every retry refuses the same way, and no term is issued
twice. An install pins strata exactly at its bundle, so the rule holds in every state.

Decisions (the doc has the alternatives):

- D1 H = (P_H, T_H) in strata's manifest (strata:applied-term). Lost: positions; a group term file
  (strata commits outside the group: catalog.hh:385-388; one more file per open); a metadata key
  (in the digest).
- D2 open rule holds(H). Binds: a complete stage for the recovered bundle, its boundary; else a
  damaged-marked replica, nothing (reinstalls); else H ((0, 0) held only by base 0).
- D3 the check in `recover`'s gate before Start (consensus:recover-gate).
- D4 the table holds no group and H != (0, 0): refused before `create`, nothing created.
- D5 install at the bundle: checkpoint = capture, align, sync, commit in one chore item; a pin is
  that commit (made while a peer awaits); SnapshotNeeded with strata past the bundle starts a
  pinning checkpoint and the flight retries after rpc_timeout; `check_stage` refuses a staged head
  that is not the bundle's boundary. Lost: the reviewer's F1 (a) (a floor, a second rule, a
  lone-voter clause).
- D6 a replicated member whose log does not hold H is refused, not repaired from the quorum: it
  lost entries it may have acknowledged and its votes; refused before Start it stays out of every
  quorum; repair = remove and re-add (installs). Under D5 no sound member has a short log.
- D7 a refusal leaves the index and set files byte-equal, the journal's state equal; journal bytes
  equal except a closed group's first recovery (reopens its table entry, journal.h:604-610) (Q1).

States (the doc's table has the index, journal and compare columns):

| State | Open compares | Outcome |
|---|---|---|
| clean stop | H = base (P_b, T_b) | opens; closed record resumes its term |
| crash with a tail; checkpoint plus tail | term at P_H = T_H (last >= P_H: strata applies only the durable commit index, core.h:1640-1644) | opens, replays from P_b |
| power cut, full flush | as the crash | opens |
| power cut, plain mode on macOS (journal tail lost, strata kept) | holds(H) fails | refused before Start, nothing written |
| closed record | H = base | opens, no election |
| install: stage complete and persisted; half-activated crash | the bundle's boundary = base | resume activates; opens at B |
| install activation | none (image replaces the index; check_stage checked its head) | H = (B, T_B) |
| restart after activation before the log reaches P_G (P_G = B under D5) | holds(H) | opens (F1's refusal gone) |
| member removed and re-added, directory kept | table holds no group, H != (0, 0) | refused before create (Q2) |
| lone voter / replicated member | holds(H), the same rule | refused: own journal back / replaced (D6) |
| journal put back older; copy of a running data root (journal first) | holds(H) fails | refused before Start, nothing written |
| copy of a running data root (index first); index put back older | holds(H), or base past P_H | opens and replays more, or refused (index older) |
| refused open retried N times | the same | the same refusal each time (D3) |
| index damaged at open | nothing (marked) | opens damaged, reinstalls |
| fresh index, no group | - | creates |
| log damaged (bounded) | - | recover fails inside consensus |

## Next

1. Read jobs above; on green, ready for review (durability re-review of D5 at e084e32d).
2. C5 (D-2): not built this launch. No group latency bench exists (bench/ has none for strata::group);
   it needs a real-disk, threaded 3-voter harness with a learner installing during periodic
   checkpoints, fused vs split, under `quiet exclusive`. Proposed as the next step on this branch.
3. Step 3 once consensus:recover-gate and strata:applied-term land (other quests).

## Blocked on

6. (lead) docs/mechanisms/group-install.md and replication-group.md still say the pin is "at or past"
   the bundle (D16); both are outside this file set. Options: (a) widen the file set to them and I
   update them to D5 (recommend); (b) the lead edits them at landing.
7. (lead, not this quest) Pre-existing, seen while choosing voters seeds: ungated storage schedules
   28, 46 and 52 (outside the suite's seeds) fail "the cluster settles" at the merge base too (a
   leader fenced by a short write on its manifest log); 14 joins them under D5's interleaving. And 8
   mutants in test/group/mutants.py no longer apply at the merge base (first-commit-unsynced, four
   catalog ones, read-on-loop, bounded-as-stored, bounded-at-bound). For hunt.md.

1. (lead, not blocking this quest) Consensus 6d710cb's own pins (task 477a7d3, completion 3061516,
   journal f66c16e, faultlab 1ac0b17, crc32c 52889db) fail strata's build: completion's PosixIO now
   takes a required FlushMode (`PosixIO(dir, PosixIOOptions{FlushMode, ...}, mode)`), and 15 files
   construct it the old way: bench/{catalog_tiers,eager_open,hybrid,idle_index,kv,kv/probes,kv/proto,
   manifest-log}, test/{counted_hooks.hh,counted_io_test.cc,directory_test.cc}, README.md,
   include/strata/core/index.hh (a comment), test/group/footprint_test.cc. Only the last is in this
   file set. Strata declares task, completion and journal first, so its pins win and consensus
   6d710cb builds and passes against strata's (journal 9c7e31c): group_tests built, the targeted
   group tests pass. Options: (a) land as is (consensus pin only) and move the wave in its own quest
   that picks each bench's flush mode; (b) widen this file set to the 15 paths. Recommend (a): the
   wave is mechanical but separate, and each bench's mode is a measurement choice.

1 answered (lead, 2026-11-25): agreed; only the consensus pin moves here. strata's move to the rest of the pin wave (completion's FlushMode in 15 files) is its own quest, filed in hunt.md.

2. (lead: a consensus quest) `consensus:recover-gate`, needed for D3 (F2's fix and a refused
   member's vote). Exact API, in consensus docs/DRIVER.md:
   ```cpp
   struct GroupState { Role role; Term term; NodeId leader; Index committed, applied, last; Membership membership; bool resting = false; LogBoundary base; };
   // On the loop: the term of the open group's entry at `index` (the base's term at base.index);
   // nullopt outside [base.index, last], at 0 with base 0, or for a group not open (Core::term_at).
   std::optional<Term> term_at(GroupId, Index) const;
   // recover, then `admit()` on the loop once the log is recovered (and `closed` applied), before
   // Start; state() and term_at() read the recovered log there. A throw out of admit completes the
   // call with it: the Worker never starts (no election, vote, append or checkpoint), and the row's
   // close releases the group without a close record, so the next recover finds the same log.
   template <class Restore, std::invocable<> Admit>
   task::task<void> recover(GroupId, Restore, std::optional<std::string> closed, Admit admit, task::cancel_token = {});
   ```
   Tests there: a lone voter refused at admit persists no term and appends no NoOp, and the next
   recover finds the same base, last and terms; turn log and host table byte-equal for a group
   recovered from the turn log; a follower refused at admit grants no vote. Today `recover`
   completes after Start (driver.h:891-897) and Driver exposes neither base nor term_at (Worker and
   Core have both: worker.h:212, core.h:221, 329). Options: (a) this API (recommend); (b) a term
   check after Start: refusals stable but the refused journal still elects (a lone voter's new term
   and NoOp per retry) and a member can vote: not byte-equal, rejected by D3.
3. (lead) `strata:applied-term` in strata's core: `verdict apply(const transaction&, files::position
   at, std::uint64_t term)`, `applied_term()`, the manifest log's `applied_term` scalar after
   `applied_count` (`files::pre_freeze` moves), install adopts it; files outside this file set
   (include/strata/core/{index.hh,index_apply.hh,index_checkpoint.hh,manifest_log.hh,files.hh}, core
   tests). Options: (a) a small strata quest of its own, which this quest consumes (recommend: a
   manifest format change with its own tests, lands independently); (b) widen this file set.
4. (lead) D5 changes `strata:group-install`: the pin moves from "at or past the bundle" (install.hh
   make_pin, group-install.md D16) to "exactly at the bundle", and every checkpoint fuses capture
   and commit. Options: (a) D5 (recommend: one rule in every state; no floor; cost one rpc_timeout
   per install that finds its leader past its bundle, one more commit per checkpoint while an
   install waits, the fused order measured by C5); (b) the reviewer's F1 (a): persist B and P_G and
   accept P_H > E inside (B, P_G], a second rule with a floor each catch-up checkpoint moves and a
   lone-voter clause. D5 needs neither 2 nor 3 and can build first; it fixes F1 and the build-test
   failure under today's check.
5. (owner, defaults in the doc) Q1 closed group's first refused recovery changes journal bytes:
   accept (a). Q2 (review N4) member re-added over a kept directory: refuse (a). Q3 a journal rolled
   back to a copy taken after the index's last commit passes: consensus's stable-storage
   assumption, documented (a).

## Evidence

Jobs prove criteria by their tags (`--for A1`); paste `campaign brief`'s generated table here.
Write the rows of [artifact], [decision] and [manual] criteria (a `path`, a decision id, who
checked what), name every job you examined, failures included (`campaign brief` lists failed
jobs no journal names), and answer each message by its id (msg-N).

| Criterion | Evidence |
|---|---|
| A1 | test 261125-004551-0bc60c (journal_behind_test: clean-stop, crash, missing, sound); mutant journal-behind-unchecked 261125-004553-aab292 (--fails-without, clean-stop); superseded by the design's C1-C4 at the next code round |
| A2 | build-test 261125-004550-50832e at strata:31821753: failed, simulation_group_corruption (F1) |
| A3 | docs/mechanisms/group-open.md (three refusals, what each leaves on disk, the sound paths); CHANGELOG.md 2026-11-25 entry; rewritten from the design at step 3 (N3) |

Examined this round (the review's probes, all at strata:31821753, failing as F1 and F2 say):
261125-011910-39eaa9 (probe build); 261125-011917-b11e3f (F1: a learner restarted after an install
with 5 writes past the bundle is refused "P_H = 12 ... E = 7" by kill, power cut and close; 0 writes
past opens); 261125-011919-316e27 (control at the merge base: opens, digests equal);
261125-011921-926bd8 (F2: P_H = E + 1, the second open serves "ee" for c; P_H = E + 2, refused at E 3
then E 4, the third open reads past the end of vol-0); 261125-011923-0ee55a (simulation_group_corruption
segment seed 3: "install activated: bundle 2, strata P_H 3", the restart refused at E 2). GitHub CI
run 45579169240 at strata:31821753: red on freebsd-x86_64 and linux-tsan, simulation_group_corruption
only (the same F1 refusal, "P_H = 3 ... E = 2"); strata:786f7102 adds docs only.

## Result

Design: strata docs/design/journal-binding.md at strata:786f7102 (D1-D7, the states table, contracts
consensus:recover-gate and strata:applied-term, checks C1-C6, Q1-Q3, steps). The code at
strata:31821753 (the position check in group.hh's adopt) stands refused by review F1 and F2; step 3
replaces it.

## Rulings I made

- Session 3: the Busy retry holds the checkpoint serializer while it waits (bounded by the flight's
  release, snapshot_timeout at the latest); it retries only when a flight explains the refusal (one in
  progress or released since the attempt began), else throws as before.
- Session 3: a refused learner check (check_stage head) throws install_failed, not damage: the source
  is wrong, not the learner's bytes; the stage is swept as for any refused complete stage.
- Session 3: the repair epoch fix is in this quest (group.hh, file set): D5 cannot pass its sim
  without it. Its class: every capture after a repair; the checkpoint is the only capture path.
- Session 3: repair_test refusal's oracle reads a write below the install's boundary from the source's
  log (an install past the writes is D5's normal outcome); voters_test's D109 seeds are re-chosen
  (3, 24, 27, 45, 49, 51, 60): which schedules leave a replica behind depends on the chore's item
  count, and 8, 12, 16, 18, 23 now catch up by log (a checkpoint pending at 1 until its node's cut
  holds the schedule's one checkpoint slot; the merge base shows the same pending-then-cancelled
  checkpoint on another seed).
- Session 3: install_test's leader pins at its bundle before applying past it (make_pin refuses
  otherwise).

- The check runs on create too (recover() serves both): a journal holding no group over an index
  with state is P_H > E with the new log's E (0), the "journal missing" refusal. Before, replay
  skipped new entries at or below P_H. Superseded by D4: refused before `create`, nothing created.
- Checked before `resume` unconditionally (session 1). Superseded by D2: a complete stage for the
  recovered bundle binds its boundary instead of the old index's head.
- E is read in open's continuation right after recover/create returns (session 1). Superseded by D3:
  Start has run by then.
- CHANGELOG.md did not exist in strata: created in consensus's format with this entry.
- Pins: consensus only (Blocked on 1).
- Session 2: the binding is the head's identity (P_H, T_H), not positions (D1, D2); one rule for the
  three refusals; a replicated member's short log is refused, not repaired (D6); a damaged-marked
  replica binds nothing and reinstalls (D2). D5 and the two contracts are the lead's (Blocked on
  2-4).
