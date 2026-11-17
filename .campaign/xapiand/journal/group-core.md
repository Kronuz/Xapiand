# group-core: journal

State: waiting on jobs 261117-112639-0e3525 261117-112642-eee4a9 261117-112645-377e63 261117-112648-9a2926 261117-112650-2e5ea4 261117-112653-d75991 261117-112656-b5bed0 261117-112659-a0922c 261117-112702-5fc7d8 261117-112704-eaa423 261117-112707-8d20d3 261117-112710-55af69 261117-112713-54e643 261117-112713-a33261 261117-112714-70e657 261117-112716-5eea20 261117-112719-032b88
Quest: .campaign/xapiand/quests/group-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Step 3. A2's settle failures diagnosed; D109 (a) built (msg-2).
A3's write path built (`submit.hh`, D106) with its test and mutants; jobs queued at strata:1e88eb3f
(Evidence). Found: the step-2 mutant jobs run with `group_voters_test 1` were vacuous (seed 1
alone never spends space, so "some replica stalled on space" failed every run, mutant or not):
quorum-at-placement 261117-085711-59939a, close-keeps-waiters 261117-085705-587d4d,
read-skips-probe 261117-085718-358fc4 prove nothing. Fixed at strata:94def77f (the check counts
only when a storage schedule ran; `0` runs the named cases alone) and rerun with `0`. Old-commit
jobs cancelled or superseded.

## Next

1. Read step 3's jobs (Evidence); fix failures; fill Evidence for A2-A4. Push for CI (lead)
   once green: the asan job and linux-x86_64-gcc were CI's two failures at 0d64860.
2. A3's remainder. Built at strata:a4e7cf80 (`submit.hh`, `blob::app::prepare`, voters_test
   `write_paths`, mutants ack-at-commit, prepare-before-ready, pending-ignored, lift-the-bound,
   no-forward, resubmit-invalid). Left: a leader stalled on `no_space` hands leadership to a
   follower applied past it (`transfer_leadership`); assert an election replacing an uncommitted
   entry answers `superseded` (seen: 1 and 2 per 8 seeds, not yet required); forward-twice
   mutant. Original plan, for reference. Lead's list:
   quorum answer carries apply's verdict (built: `propose` at `ack::quorum`); refused merge
   resubmitted from the original request up to 3 placements, then a retryable 503; a deposed
   proposer forwards once carrying its placement count; `superseded`. Plan:
   - `include/strata/group/answer.hh` holds the rule (`after`, `max_placements`); add the loop
     there or beside it: `submit(request)`, where the request prepares a command from the
     leader's applied state (a merge chooses `expected_version`: `leader_chose = true`), proposes
     at `quorum`, and on `after(...) == resubmit` prepares again from the original request;
     `unavailable` answers a retryable 503; `rejected{NotLeader}` at a deposed proposer forwards
     once through a host hook on `loop` (like `send`), carrying `placements`; `superseded`
     answers as is, never resubmitted. Keep it isolated so a different ruling edits one file.
   - blob app: add a merge command (`M|key|value`, prepared to `C|key|value|<version>` at the
     leader) and an invalid one (strata `invalid`, answered 400, never resubmitted); a test hook
     that makes the predictor miss the fields mark (stale version) so the bound is reached.
   - voters_test cases: two merges to one id, a conditional write and an invalid document with
     an election between, every schedule: no acknowledged success refused, no merge answered
     409, the conditional's answer matches its effect, invalid answered 400 once, no request past
     3 placements (503 after the third), a deposed proposer's merge forwarded once and answered
     by the leader; a leader stalled on `no_space` hands leadership to a follower applied past
     it (`transfer_leadership`); an election replacing an uncommitted entry answers `superseded`.
   - Mutants (`mutants.py`, `--target=group_voters_test`, each a `--fails-without` job):
     acknowledge-at-commit, prepare-before-ready, lift-the-bound (`max_placements` unbounded),
     forward-twice or no-forward, resubmit-invalid.
3. A4's applied-before-write mutant (see Rulings), A5 close racing `Committed`, A6, A10; A7 after
   `detent:consensus-quiescence`.

## Messages

- msg-2 (lead, D109): (a). Built: A2's gated schedules checkpoint only when every node is up,
  none stalled or short of space, and the checkpointing node's commit is at or below every
  node's; its loop and chore then run alone (IO held, no message in) so the capture lands at or
  below that commit (`world::leaves_none_behind`, `world::capture`); the settle check is
  unchanged. Seeds 8, 12, 16, 18, 23, 24, 27 (3 voters) run ungated as fixed schedules that
  assert the cluster does not settle and every replica behind is named in `installs_needed()`.
  Root cause (for group-install): a replica cut off while another checkpoints past its log end
  gets `SnapshotNeeded` (consensus core.h:1164-1168) and, with no install, stays behind; 7 of 80
  schedules, all failing, none failing without it (`~/.scratch/group-core/census-40-seeds.txt`).
- msg-1 (lead, journal-host-guards): done in strata:1068c00d. journal pinned at cc95ba6 (landed);
  `jn::group_closed` from the Driver's propose maps to `cs::closed` (`group::place`); consensus's
  own `durable()` watch (driver.h:928) swallows it, so the group's wait is where it lands: a
  close (and a fence) refuses every `quorum` waiter, linked or not yet (sticky `refused_`).
  Tests: lone voter's script proposes then closes, the write answered (verdict or `closed`), the
  group reopened from its bundle at every crash point; voters_test `close_with_uncommitted`: a
  leader cut off from its followers places a write and closes, the waiter answers `closed`
  (mutant close-keeps-waiters, job 261117-085705-587d4d), the group reopens and the cluster
  settles to equal digests. Not testable yet: an in-process reopen and recovery over a retired
  sequence gap. At consensus 57745e8, `Driver::close` drains the worker but never calls
  `Host::close_group`, so the journal group stays `stored` and an in-process reopen is refused
  ("the group was recovered on this host"); the gap appears only once
  detent:consensus-quiescence closes the journal group. The tests restart the process instead
  and say so; add the in-process case with A7.
- Lead, mid-step: keep consensus at 57745e8 (main e4e394b red). Kept; only journal moved.

## Blocked on

- A2's settle failures: ruled (a), PLAN.md D109 (lead, msg-2); group-install takes the seven
  seeds as its A8. Root cause and proof under Messages, msg-2.
- detent:consensus-quiescence: in-process reopen after close and recovery over the retired
  sequence gap (msg-1), and A7.
- Q10: decided (b), D106. Built on it, kept in one place
  (strata `include/strata/group/answer.hh`: `ack`, `answer`, `max_placements = 3`, `after()`).
- `strata-nospace-position` (not built): the group relies on its contract (a `no_space` answer
  leaves strata's position unmoved). At 491a1bdc the reservation path keeps it and the allowance
  path moves it; the group fences on the `skipped` that follows (`group.hh`, `write()`), and A1's
  `no_space` case uses the reservation path. When it lands, add the allowance-path case.

## Evidence

| Criterion | Evidence |
|---|---|
| A1 | queued: build-test 261117-041324-e0772a, crash (ctest `group_crash_lone_voter`) 261117-041324-825bcd, TSan 261117-041429-1aa7a2; `--fails-without` mutants: replay-from-ph 261117-041329-e493ed, unlink-at-apply 261117-041335-5055d0, publish-before-sync 261117-041341-82b222, capture-mid-entry 261117-041346-d08891, effects-gated 261117-041351-01ad6c, appstate-gated 261117-041356-b72595, no-mark 261117-041401-a4bc5e, commit-after-publish 261117-041406-4aaf54, append-on-retry 261117-041412-e13f97, sync-retry 261117-041417-101200. Locally at strata:e7bb6c70: 174 script events, 175 crash points, 2,527 second crashes, 0 failures; all ten mutants killed |
| A1 (step 2, strata:466dbece) | queued: build-test 261117-085649-7369d7, crash 261117-085649-593d4f, ASan+LSan (Homebrew LLVM, lone voter every 10th crash point plus seconds, voters 4) 261117-085742-96a255. Local ASan+LSan at every 25th crash point: 196 script events, 8 crash points, 112 second crashes, ok, no leaks |
| A2 | queued: 40 schedules per voter count 261117-085749-e0c12d, TSan 261117-085735-f77e18; `--fails-without` append-on-retry 261117-085651-94cdb1; hunt row (default limits) admission-waits 261117-085659-d135d4. Local probe at strata:466dbece's tree: 4 schedules, 4 crashes, 113 acknowledged, 129 entries applied, 201 stalled chore steps, ok |
| A3 (part) | ack levels (none at ProposalPlaced, quorum waits with appends held): quorum-at-placement 261117-085711-59939a; close answers `closed`: close-keeps-waiters 261117-085705-587d4d |
| A4 (part) | deposed leader's `read_applied` never answers the older state: read-skips-probe 261117-085718-358fc4 |
| A9 | strata `docs/mechanisms/replication-group.md` at strata:466dbece: lone voter, close and lifetime, the admission bound, the voters proof |
| A2 (step 3, strata:1e88eb3f, D109) | queued: voters 40 schedules, gated 261117-112714-70e657; TSan 261117-112716-5eea20; ASan+LSan 261117-112719-032b88; gate build-test 261117-112713-54e643, crash 261117-112713-a33261; `--fails-without` install-silent 261117-112707-8d20d3, checkpoint-ungated (14 schedules) 261117-112710-55af69. Local at ef4bde08: seeds 1-4 at 3 and 5 voters ok, 47 gated checkpoints, the seven install seeds named. Diagnosis: `~/.scratch/group-core/census-40-seeds.txt`, `~/.scratch/group-core/seed27-installs.txt` |
| A3 (step 3, strata:1e88eb3f) | queued: named cases alone (`group_voters_test 0`) 261117-112639-0e3525; `--fails-without` ack-at-commit 261117-112642-eee4a9, prepare-before-ready 261117-112645-377e63, pending-ignored 261117-112648-9a2926, lift-the-bound 261117-112650-2e5ea4, no-forward 261117-112653-d75991, resubmit-invalid 261117-112656-b5bed0, quorum-at-placement 261117-112659-a0922c, close-keeps-waiters 261117-112702-5fc7d8. Local: write path 3 voters 32 requests, 19 placements, 19 forwards, 1 superseded, 12 answered 503 (stale: 39 placements, 22 answered 503); 5 voters 32, 24, 19, 2, 6 (stale 48, 18) |
| A4 (step 3) | `--fails-without` read-skips-probe 261117-112704-eaa423 (step 2's was vacuous) |
| A5-A8, A10 | none yet |
| Models, rounds 1-3 | `bundle_model.py` jobs 261117-024523-dd3b4d, 261117-025944-a43484, 261117-030454-a1f4da; `verdict_model.py` 261117-024526-a92827, 261117-025949-8a87ce |

## Result

Step 1 (strata): `include/strata/group/{group,bundle,file_set,answer}.hh`, target `strata::group`;
`test/group/{lone_voter_test.cc,blob_app.hh,host_io.hh,mutants.py}`, ctest `group_lone_voter`;
`docs/mechanisms/replication-group.md`; `docs/REPLICATION.md` "Checkpoints and repair" takes the
proposed text. strata pins consensus 57745e8, journal 555b69f, task 95798a0, completion 7731315.

## Rulings I made

- A3 (D106) split: the group gives the ready point (prepare runs on the chore once applied
  reaches `Driver::read`'s index) and the leader's pending commands (its term's, prepared and
  not applied, in prepare order; a placed one drops out once applied passes it); the predictor
  is the application's (`blob::app::prepare`, Xapiand's later). `submit.hh` holds the loop.
- A read refused `NotReady` or `Busy` (a leader before its term's first entry commits) answers a
  retryable 503 with nothing placed: preparing there could miss the prior term's entries.
- A client's own condition (`C|key|value|v`) is checked by apply, its 409 apply's verdict; the
  model's 409-at-prepare is an option the application may take (`prepared::refused`).
- `propose` takes a `placed` hook (the writer learns the placement before apply answers); the
  `quorum-at-placement` mutant's anchor moved with it.
- The test's forward goes to the leader of the highest term (a cut-off leader still believes it
  leads).

- Step 2: the CI leak was the test destroying a group with its chore and waiters suspended.
  Rule: a group is destroyed with its chore idle (after `close()`, or fenced and drained); a
  fence drops the item `drain()` kept; `applied(through)` posts hold loop and id, not the group.
- A checkpoint with nothing applied past the last bundle answers that bundle (`published_`):
  consensus refuses a capture at or below its base (`InvalidCheckpoint`), which made `close()`
  right after a checkpoint fail.
- Hunt row "checkpoint admission at default limits": the group fails such a checkpoint with
  `length_error` when `maximum_size(limits) + bundle > replacement_pool.logical_bytes` (necessary,
  not sufficient: journal's plan peak is private). Sizing the pool stays quest 10's.
- Hunt row "faultfs lacks lock_owner": the fix is in `strata/core/faultfs.hh`, outside this file
  set; the voters test reuses `test/group/host_io.hh` per node. Left on the hunt list.
- `read_applied` runs its function on the chore between two entries once applied reaches the
  Driver's read index, so A4's "publish applied before strata's write" mutant cannot make it read
  early: the read never runs mid-entry. That mutant needs a loop-side read; not built.
- A2's budget refusal uses the reservation path (as A1): the allowance path moves strata's
  position until strata-nospace-position lands, and the group fences on the `skipped` after.
- The voters test's ctest runs 12 schedules per voter count; the evidence job runs 40.

- strata pins consensus at 57745e8 and moves journal to 555b69f, task to 95798a0 and completion to
  7731315 (consensus's and journal's own pins, each a descendant of strata's old one; framing.h
  unchanged), and turns on `COMPLETION_TASK`, which journal now links.
- Strata position of entry i is `(i, 1)`; the capture's mark is an empty transaction at `(P_b, 1)`.
- One queue per group on a host-supplied chore scheduler, one item per resume, so every entry,
  capture, sync-and-commit, unlink and recovery is a crash point. Recovery goes to the queue's
  front and the queue holds until then, so ranges Core delivers during `recover` wait behind it.
- strata is created after the journal group's create (open-or-create on the chore), so a strata
  index never holds entries its journal lacks.
- Apply keeps a phase: a stall after the file effects retries only the strata write; a stall in
  the effects (ENOSPC) re-runs them from the entry's start, positionally.
- A `quorum` proposal not yet registered may be any entry applied meanwhile: apply keeps outcomes
  above the oldest unregistered proposal's starting applied index, pruned as they register.
- The journal host's faultfs goes through `test/group/host_io.hh`, a forwarding IO that adds
  `lock_owner` (faultfs::io implements only the legacy lock calls; core is outside this file set).
- Oracle: the test application records every entry it applies (index, term, kind, command); the
  replay runs those through the application alone on a fresh strata index and an in-memory file
  set, sharing no code with the group's apply, checkpoint or recovery.
- The consensus limits in the test are small (log 64 KiB): at the defaults a checkpoint's
  reservation exceeds the admission's 64 MiB replacement pool and waits forever. Xapiand's group
  options must size the pool for `maximum_size(limits)`; noted for quest 10.
- "Capture inside an entry's apply" is reached through the test application's `after_effects`
  hook, which starts a checkpoint while an upload's final entry applies.

## Design result (rounds 1-3)

Round 3 (B10, N16, N17): file effects are positional from the entry's starting application state
(the current volume's length is in it), so a failed push (ENOSPC) stalls and rewrites the same
bytes; a failed checkpoint sync fences and reopens through recovery; a forwarded re-submission
carries its placement count. D3, D16 step 2, quest 5 A1-A3. `bundle_model.py`, 11 scripts:
`fixed` 0 of 1,746, `sync-late` 0 of 1,752, `retry-whole` 0 of 1,758 (now safe); `append-on-retry`
fails 34, `sync-retry` 6, the other twelve broken variants fail as before.


Round 2 (re-check B7, B8, N10-N15), on top of round 1's revision 2 (B1-B6):

- B7: retryable answers (`no_space`, owed verdicts, a stage-1 throw) keep strata's position
  (`strata-nospace-position`, which quest 5 lands after) and retry the strata write alone; the
  file effects ran once and the application state updates after the definite answer; a third
  throw at one entry fences the replica. Definite, position-moving answers: `accepted`,
  `version_conflict`, `schema_conflict`, `fields_conflict`, `invalid` (N12: replicas share
  strata's caps). D3, D18.
- B8: re-submission from the original request for leader-chosen expectations only, at most three
  placements, a deposed proposer forwards once to the leader, then a retryable 503. D3, Q10.
- N11: the mark retries like an entry, so a capture and a close wait out `no_space`. N13: the
  application state is verdict-independent. N15: a stalled leader holds its group's `quorum`
  answers and hands leadership to a follower applied past the entry after one election timeout.

Models (`docs/design/cluster-release/`), jobs below:

- `bundle_model.py`: 9 scripts; `fixed` and `sync-late` fail 0 of 1,416 crash points each; every
  broken variant fails some: a1-literal 80, eager-delete 13, counter-from-dir 26, publish-first
  34, commit-after-publish 30, capture-mid 212, verdict-gated 8, read-early 74, no-mark 86,
  nospace-moves 36, retry-whole 40, appstate-gated 74.
- `verdict_model.py`, two voters with their own applied state: (a) acknowledges a refused write
  and fences in 2,442 / 1,092 / 0 / 1,813 of 2,520 schedules (applied, pending, ready point, the
  fields-mark gap); (b) over 20,160 schedules under all four rules: 0 false acknowledgements, 0
  fences, 0 merges answered 409, 0 unanswered, 0 divergent verdicts, `invalid` never
  re-submitted, no request past three placements; re-submissions 35,296 / 6,328 / 0 / 24,040,
  deposed-proposer forwards 9,128 / 0 / 0 / 9,296, merges answered 503 2,152 / 112 / 0 / 1,680.

## Design rulings

- Criteria in the design's section 5 follow the quest file's numbering (A1-A9, plus A10) so the two
  stop drifting.
- Models live in `docs/design/cluster-release/` (as `sortable-encoding/sizes.py` does), not in the
  reviewer's scratch, so the review reruns them at this commit.
- Added the `docs/design` paths to this worktree's sparse checkout to edit the design.
- Three placements per request: the third covers a leader change plus one predictor miss; with
  the ready point and pending versions the model never needs a second.
- A stage-1 throw retries twice, the third fences: a transient allocation failure clears, a
  damaged segment repairs by install.
