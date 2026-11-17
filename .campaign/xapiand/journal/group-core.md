# group-core: journal

State: ready for review
Quest: .campaign/xapiand/quests/group-core.md (the lead's; never edit it)

Keep this current and commit it with the work, so a fresh contributor can continue. `campaign
status` reads State and the first step under Next. State is one of: working, waiting on jobs <ids>,
ready for review, fixing <findings>, blocked on <who>: <what>.

## Doing

Round 2 of the design (the last): the re-check at Xapiand:edb610078 (CHANGES on B7, B8; N10-N15)
answered in `docs/design/cluster-release.md` D3, D18, Q10, quest 5's criteria and section 12, with
both models extended (`docs/design/cluster-release/`).

## Next

1. Lead: fold the criteria deltas below into the quest file; Q10 (with the 503 after the bound) to
   the owner; anything still open after this round goes to the owner.
2. Build starts after `strata-nospace-position` and the design review accept.

## Blocked on

- Owner, through the lead: Q10. Recommendation (b), the `quorum` answer carries apply's verdict,
  plus: a refused merge gets at most three placements, then a retryable 503 with `Retry-After`
  (keeps "a merge is never answered 409"). Client-visible against revision 1, not master: a
  `quorum` 2xx means applied; a conditional write can get its 409 after commit; a merge can get a
  503 after three refused placements (0 schedules with the ready point and pending versions).
- Lead: strata `docs/REPLICATION.md` text (outside the file set), unchanged from round 1:
  "A checkpoint bundle binds the log to strata by position: strata's durable head is at or past
  the bundle's position P_b when the bundle publishes." and "Recovery opens strata at its durable
  head P_H, fails closed if P_b > P_H, and replays the log above P_b; strata skips entries at or
  below P_H, and the application's file effects re-run."

## Proposed criteria deltas (round 3, against the quest file at Xapiand:4f0c74bdb)

- A1 (B10): the run also covers a blob push that fails partway with ENOSPC and a checkpoint whose
  file sync fails; in the mutant list, replace "a retry that re-runs the whole entry" (now safe:
  positional file effects rewrite the same bytes) with "append on retry; retry a failed checkpoint
  sync and publish"
- A2 (B10): the storage case reads "one replica's storage budget refuses at allowance growth and
  its file-set write fails once with ENOSPC, then both free, and digests stay equal; a mutant that
  appends on retry fails it"
- A3 (N17): adds "an election that replaces an uncommitted entry answers its request `superseded`,
  never submitted again by the group"

## Evidence

| Criterion | Evidence |
|---|---|
| A1-A9 | none yet: design only, nothing built |
| Models, round 1 | `bundle_model.py` job 261117-024523-dd3b4d and `verdict_model.py` job 261117-024526-a92827, both exit 0 at Xapiand:faf31a50635b |
| Models, round 3 | `bundle_model.py` job 261117-030454-a1f4da, exit 0 at Xapiand:2fea83c16dea |
| Models, round 2 | `bundle_model.py` job 261117-025944-a43484 and `verdict_model.py` job 261117-025949-8a87ce, both exit 0 at Xapiand:904f8c6a7038 |

## Result

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

## Rulings I made

- Criteria in the design's section 5 follow the quest file's numbering (A1-A9, plus A10) so the two
  stop drifting.
- Models live in `docs/design/cluster-release/` (as `sortable-encoding/sizes.py` does), not in the
  reviewer's scratch, so the review reruns them at this commit.
- Added the `docs/design` paths to this worktree's sparse checkout to edit the design.
- Three placements per request: the third covers a leader change plus one predictor miss; with
  the ready point and pending versions the model never needs a second.
- A stage-1 throw retries twice, the third fences: a transient allocation failure clears, a
  damaged segment repairs by install.
