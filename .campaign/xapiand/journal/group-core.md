# group-core: journal

State: working
Quest: .campaign/xapiand/quests/group-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Design revision answering `reviews/group-core-design.md` (CHANGES, B1-B6): Xapiand
`docs/design/cluster-release.md` revision 2 (D3, D4, D11, D12, D16, new D18, new Q10, quest 5's
criteria, section 12's group-core table) on this branch, with the review's models extended in
`docs/design/cluster-release/{bundle_model,verdict_model}.py`.

## Next

1. Lead: rewrite the quest's A1, A3, A5, A6, A7 and add A10 from "Proposed criteria" below; fold
   the contract delta; route Q10 to the owner; relaunch the design review on revision 2.
2. Build starts after the design review accepts (none of `include/strata/group/**` is written).

## Blocked on

- Owner, through the lead: Q10 (design section 9), what a `quorum` write's answer means when apply
  refuses it. Recommendation (b): the answer carries apply's verdict. Client-visible against
  revision 1 (not against master): a `quorum` 2xx now means applied, not only committed, and a
  conditional write can be answered 409 after its entry committed; merges are never answered 409.
  Lead default for quest 5: (b).
- Lead: the contract `strata:checkpoint-bundle` changes (quest file "Contracts"): P_b and term, the
  file set, an opaque application-state section, and no manifest CRC32C (N9; install's pin carries
  it). D16's table has the bytes.
- Lead: strata `docs/REPLICATION.md` is outside this quest's file set. Proposed text for
  "Checkpoints and repair" lines 90-92 and 99-100: "A checkpoint bundle binds the log to strata by
  position: strata's durable head is at or past the bundle's position P_b when the bundle
  publishes. Strata owns its segment files and their retention; the journal never copies them."
  and "Recovery opens strata at its durable head P_H, fails closed if P_b > P_H, and replays the
  log above P_b; strata skips entries at or below P_H, and the application's file effects re-run."
- Lead: `Host::close_group`'s missing sequence guard (review N1) belongs to turn-seal or
  `detent:consensus-quiescence`; the journal pin for the suite (N8) is the lead's.

## Proposed criteria (quest file numbering)

- A1 (B1, B2, B3, B6) A lone voter over strata's faultfs (unsynced sectors and directory entries
  lost) for strata, the file set and the journal host, with `HeldCompletion` choosing the cut:
  create, propose, apply, checkpoint and recover at every crash point, the windows between
  strata's commit and publication and between publication and the retired files' unlink included,
  and a second crash at every point of recovery. The run covers a staging file consumed above the
  bundle, an aborted upload, an upload open across a checkpoint, a cancelled checkpoint, a strata
  commit outside the group and a refused write, with the application's counter, current volume
  and open uploads in the bundle's application state. Oracle: every acknowledged command is in the
  recovered log, and the digest (documents with versions resolved through their locators, the file
  set, the application state) equals a replay from empty of the recovered log's committed
  commands; reads open only at P_H and match that replay through it. Mutants, each
  `--fails-without`: replay the file set from P_H; unlink at apply; publish before the files sync;
  capture inside an entry's apply; file effects gated on strata's verdict (the `crash` gate check,
  jobs)
- A3 (B4, N3) `ack=none` completes at `ProposalPlaced` and `quorum` when the proposer applies its
  placement, with apply's verdict; with the followers' appends held, `none` completes and `quorum`
  does not; two merges to one id and a conditional write with an election between, every
  schedule: no acknowledged success refused, no merge answered 409, the conditional write's answer
  matches its effect, and no re-proposal; mutants that acknowledge at commit and that prepare
  before the ready point fail (tests)
- A4 (N4) adds: a mutant that publishes the applied position before strata's write is visible fails
- A5 (N1) Once a group closes, `TurnLog::read` finds none of its records, and a reopen recovers
  every acknowledged entry from the bundle; a close racing a `Committed` range ends with a bundle
  covering applied; a mutant that closes without a covering checkpoint fails (`--fails-without`, a
  test)
- A6 (B5) An idle open group holds no thread, timer, frame or descriptor of its own beyond
  consensus's row and its open strata index (2 descriptors, about 5.4 KB of heap); memory,
  descriptors and idle CPU per group stated at 10^4 groups for open idle, quiescent and closed
  groups, with a close's cost (its checkpoint's syncs); a closed group holds zero descriptors.
  strata's catalog owns the group's and index's lifetime: a catalog close of an idle group
  publishes a bundle covering applied (an `exclusive` job and a test)
- A7 (N2) `detent:consensus-quiescence` A5's consumer check: a reopened lone voter's term record
  and its first write share one turn, the barrier count of that write equal to a write to an open
  group (a test counting barriers, run after that quest lands; its A5 job is the evidence beside it)
- A10 (X10, N7) Apply leaves the loop: with one group's apply chore held, another group on the
  same loop proposes, commits and applies; capture against apply, close against a `Committed`
  range and `read_applied` against apply, each with its scheduling points enumerated, under TSan
  (tests)

## Evidence

| Criterion | Evidence |
|---|---|
| A1-A9 | none yet: design only, nothing built |
| Models | see Result; job ids recorded below once they run |

## Result

Design revision 2 of Xapiand `docs/design/cluster-release.md` on `quest/xapiand/group-core`:

- B1: two positions (file set from P_b, strata skips at or below P_H), fail closed if P_b > P_H,
  capture on the apply chore, sync and strata commit before publication, reads wait for P_H (D16).
  New beyond the review: file effects run whatever strata answers (D3), and the capture moves
  strata's position to P_b when the last entries had no strata operation (D16 step 1).
- B2: retire at apply, unlink after a bundle without the file publishes (D12, D16 step 5).
- B3: the bundle's opaque application-state section (D16's table).
- B4: (b), the `quorum` answer carries apply's verdict, with the ready point and pending versions
  (D3, D4, Q10).
- B5/N2/N7: catalog owns the lifetime (D11); A7 as quiescence's consumer check; D18 off-loop apply.

Models (`docs/design/cluster-release/`):

- `bundle_model.py`: `fixed` and `sync-late` fail 0 of 1,098 crash points each (7 scripts, power
  loss and process crash, a second crash before and halfway through replay); every broken variant
  fails some: a1-literal 56, eager-delete 11, counter-from-dir 26, publish-first 26,
  commit-after-publish 22, capture-mid 170, verdict-gated 8, read-early 52, no-mark 72.
- `verdict_model.py`, 360 schedules: (a) at commit acknowledges a refused write in 348 (applied
  prepare), 156 (pending), 0 (pending plus ready point), 259 (a predictor missing the fields
  mark), fencing every replica in each; (b) with the verdict: 0 false acknowledgements, 0 fences,
  0 merges answered 409 under all four, re-proposals 552, 121, 0, 547.

## Rulings I made

- Criteria in the design's section 5 follow the quest file's numbering (A1-A9, plus A10) so the two
  stop drifting.
- Models live in `docs/design/cluster-release/` (as `sortable-encoding/sizes.py` does), not in the
  reviewer's scratch, so the review reruns them at this commit.
- Added the `docs/design` paths to this worktree's sparse checkout to edit the design.
